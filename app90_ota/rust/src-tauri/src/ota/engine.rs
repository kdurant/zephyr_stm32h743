use std::path::Path;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use std::time::Duration;

use tauri::{AppHandle, Emitter};

use crate::ota::firmware;
use crate::ota::types::{OtaConfig, OtaEvent, OtaProgress, UpgradeStage};
use crate::protocol::commands::{self};
use crate::protocol::frame::Frame;
use crate::serial::SerialManager;

/// OTA upgrade engine.
pub struct OtaEngine {
    config: OtaConfig,
    cancel_flag: Arc<AtomicBool>,
}

impl OtaEngine {
    pub fn new(config: OtaConfig) -> Self {
        Self {
            config,
            cancel_flag: Arc::new(AtomicBool::new(false)),
        }
    }

    /// Request cancellation of the ongoing upgrade.
    pub fn cancel(&self) {
        self.cancel_flag.store(true, Ordering::SeqCst);
    }

    // ── Event helpers ──

    fn emit_log(app: &AppHandle, level: &str, msg: &str) {
        let _ = app.emit(
            "ota:event",
            OtaEvent::Log {
                level: level.to_string(),
                message: msg.to_string(),
            },
        );
        log::info!("[OTA] {}", msg);
    }

    fn emit_progress(app: &AppHandle, stage: UpgradeStage, msg: &str, sent: u64, total: u64) {
        let percentage = if total > 0 {
            (sent as f64 / total as f64 * 100.0).min(100.0)
        } else {
            0.0
        };
        let _ = app.emit(
            "ota:event",
            OtaEvent::Progress(OtaProgress {
                stage,
                message: msg.to_string(),
                bytes_sent: sent,
                total_bytes: total,
                percentage,
            }),
        );
    }

    fn emit_stage(app: &AppHandle, stage: UpgradeStage) {
        let _ = app.emit("ota:event", OtaEvent::StageChanged { stage });
    }

    fn emit_error(app: &AppHandle, msg: &str) {
        let _ = app.emit(
            "ota:event",
            OtaEvent::StageChanged {
                stage: UpgradeStage::Error,
            },
        );
        let _ = app.emit(
            "ota:event",
            OtaEvent::Error {
                message: msg.to_string(),
            },
        );
        log::error!("[OTA] {}", msg);
    }

    /// Check if response is an error.
    fn check_response(resp: &Frame, label: &str) -> Result<(), String> {
        if resp.is_error_response() {
            Err(format!("{}: device returned error (0xFF)", label))
        } else {
            Ok(())
        }
    }

    // ═══════════════════════════════════════════
    // Main upgrade flow
    // ═══════════════════════════════════════════

    pub fn run_upgrade(&self, app: AppHandle, serial: Arc<SerialManager>, firmware_path: String) {
        // Reset cancel flag
        self.cancel_flag.store(false, Ordering::SeqCst);

        let fw_path = std::path::PathBuf::from(firmware_path);
        let cancel_flag = self.cancel_flag.clone();
        let config = self.config.clone();

        std::thread::spawn(move || {
            match Self::do_upgrade(&app, &serial, &fw_path, &config, &cancel_flag) {
                Ok(()) => {}
                Err(e) => {
                    // Poll MCU logs before reporting failure
                    Self::poll_logs(&app, &serial);
                    Self::emit_error(&app, &e);
                    log::error!("[OTA] Upgrade failed: {}", e);
                }
            }
        });
    }

    fn do_upgrade(
        app: &AppHandle,
        serial: &SerialManager,
        fw_path: &Path,
        config: &OtaConfig,
        cancel_flag: &AtomicBool,
    ) -> Result<(), String> {
        let is_cancelled = || cancel_flag.load(Ordering::SeqCst);

        // ── Step 0: Load firmware ──
        Self::emit_log(app, "info", &format!("Loading firmware: {:?}", fw_path));
        let fw_data = firmware::load_firmware(fw_path).map_err(|e| {
            Self::emit_error(app, &e);
            e
        })?;
        let fw_size = fw_data.len() as u32;
        let fw_crc = firmware::calc_crc32(&fw_data);
        Self::emit_log(
            app,
            "info",
            &format!(
                "Firmware loaded: {} bytes, CRC32: 0x{:08X}",
                fw_size, fw_crc
            ),
        );

        // ── Step 1: Handshake ──
        Self::emit_stage(app, UpgradeStage::Handshaking);
        let frame = commands::build_handshake().map_err(|e| e.to_string())?;
        let resp = Self::send_frame(app, serial, &frame, "handshake", config, cancel_flag)?;
        Self::check_response(&resp, "handshake")?;
        if let Some((status, ver_major, ver_minor)) = commands::parse_handshake_response(&resp) {
            Self::emit_log(
                app,
                "info",
                &format!(
                    "Handshake OK: status={:#04X}, protocol v{}.{}",
                    status, ver_major, ver_minor
                ),
            );
        }

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 2: Get device info ──
        Self::emit_stage(app, UpgradeStage::GettingDeviceInfo);
        let frame = commands::build_get_device_info().map_err(|e| e.to_string())?;
        let resp = Self::send_frame(app, serial, &frame, "get_device_info", config, cancel_flag)?;
        Self::check_response(&resp, "get_device_info")?;
        if let Some(info) = commands::parse_device_info(&resp) {
            let _ = app.emit("ota:event", OtaEvent::DeviceInfo { info: info.clone() });
            Self::emit_log(
                app,
                "info",
                &format!(
                    "Device: {} | FW: {} | Flash: {}B (page: {}B)",
                    info.mcu_model, info.fw_version, info.flash_total_size, info.flash_page_size
                ),
            );
            // Check if firmware fits
            if fw_size > info.flash_total_size {
                let msg = format!(
                    "Firmware too large: {} > {} bytes",
                    fw_size, info.flash_total_size
                );
                Self::emit_error(app, &msg);
                return Err(msg);
            }
        }

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 3: Start upgrade ──
        Self::emit_stage(app, UpgradeStage::Erasing);
        // Build version from firmware (use fixed version for now)
        let fw_version: [u8; 4] = [1, 0, 1, 0]; // V1.0.1
        let frame = commands::build_start_upgrade(fw_size, fw_crc, &fw_version)
            .map_err(|e| e.to_string())?;
        let resp = Self::send_frame(app, serial, &frame, "start_upgrade", config, cancel_flag)?;
        Self::check_response(&resp, "start_upgrade")?;
        let (status, max_pkt_size, _erase_progress) = commands::parse_start_upgrade_response(&resp)
            .ok_or("Failed to parse start_upgrade response")?;
        if status != 0x00 {
            return Err(format!("Device refused upgrade: status={:#04X}", status));
        }
        let chunk_size = max_pkt_size.max(128).min(4096) as usize;
        Self::emit_log(
            app,
            "info",
            &format!("Device ready, max packet size: {} bytes", chunk_size),
        );

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 4: Transfer firmware packets ──
        Self::emit_stage(app, UpgradeStage::Transferring);
        let packets = firmware::split_into_packets(&fw_data, chunk_size);
        let total_packets = packets.len() as u16;
        Self::emit_log(
            app,
            "info",
            &format!(
                "Starting transfer: {} packets, {} bytes total",
                total_packets, fw_size
            ),
        );

        for (i, pkt) in packets.iter().enumerate() {
            if is_cancelled() {
                // Send cancel command
                let cancel_frame = commands::build_cancel_upgrade().map_err(|e| e.to_string())?;
                let _ = serial.with_conn(|c| c.send_frame(&cancel_frame));
                Self::emit_stage(app, UpgradeStage::Cancelled);
                return Err("Cancelled".into());
            }

            let frame =
                commands::build_transfer_data(pkt.offset, &pkt.data, i as u16, total_packets)
                    .map_err(|e| e.to_string())?;

            let resp = Self::send_frame(app, serial, &frame, "transfer_data", config, cancel_flag)?;
            Self::check_response(&resp, &format!("transfer packet {}", i))?;

            // Progress
            let sent = pkt.offset as u64 + pkt.data.len() as u64;
            Self::emit_progress(
                app,
                UpgradeStage::Transferring,
                &format!("Packet {}/{}", i + 1, total_packets),
                sent,
                fw_size as u64,
            );
        }

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 5: Transfer complete ──
        Self::emit_stage(app, UpgradeStage::Completing);
        let frame =
            commands::build_transfer_complete(fw_crc, fw_size).map_err(|e| e.to_string())?;
        let resp = Self::send_frame(
            app,
            serial,
            &frame,
            "transfer_complete",
            config,
            cancel_flag,
        )?;
        Self::check_response(&resp, "transfer_complete")?;
        if let Some((status, received)) = commands::parse_transfer_complete_response(&resp) {
            Self::emit_log(
                app,
                "info",
                &format!(
                    "Transfer complete: status={:#04X}, received={} bytes",
                    status, received
                ),
            );
        }

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 6: Verify firmware ──
        Self::emit_stage(app, UpgradeStage::Verifying);
        let frame = commands::build_verify_firmware().map_err(|e| e.to_string())?;
        let resp = Self::send_frame(app, serial, &frame, "verify_firmware", config, cancel_flag)?;
        Self::check_response(&resp, "verify_firmware")?;
        if let Some((result, calc_crc)) = commands::parse_verify_response(&resp) {
            if result == 0x00 {
                Self::emit_log(
                    app,
                    "info",
                    &format!("CRC32 verified OK: 0x{:08X}", calc_crc),
                );
            } else {
                let msg = format!(
                    "CRC32 mismatch: expected 0x{:08X}, got 0x{:08X}",
                    fw_crc, calc_crc
                );
                Self::emit_error(app, &msg);
                return Err(msg);
            }
        }

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 7: Set boot flag ──
        Self::emit_stage(app, UpgradeStage::SettingBootFlag);
        let frame = commands::build_set_boot_flag(0x01).map_err(|e| e.to_string())?;
        let resp = Self::send_frame(app, serial, &frame, "set_boot_flag", config, cancel_flag)?;
        // Poll MCU logs — this captures set_boot_flag diagnostic messages
        Self::poll_logs(app, serial);
        Self::check_response(&resp, "set_boot_flag")?;
        Self::emit_log(app, "info", "Boot flag set: new firmware");

        if is_cancelled() {
            return Err("Cancelled".into());
        }

        // ── Step 8: Reset device ──
        Self::emit_stage(app, UpgradeStage::Resetting);
        let frame = commands::build_reset_run(100).map_err(|e| e.to_string())?; // 100ms delay
        let resp = Self::send_frame(app, serial, &frame, "reset_run", config, cancel_flag)?;
        Self::check_response(&resp, "reset_run")?;
        Self::emit_log(app, "info", "Device reset command sent");

        // ── Done ──
        Self::emit_stage(app, UpgradeStage::Complete);
        Self::emit_log(app, "info", "OTA upgrade completed successfully!");
        Ok(())
    }

    /// Helper: send frame with timeout and cancellation check.
    fn send_frame(
        app: &AppHandle,
        serial: &SerialManager,
        frame: &Frame,
        label: &str,
        config: &OtaConfig,
        cancel_flag: &AtomicBool,
    ) -> Result<Frame, String> {
        let timeout = Duration::from_millis(config.command_timeout_ms);

        for attempt in 1..=config.max_retries {
            if cancel_flag.load(Ordering::SeqCst) {
                return Err("Upgrade cancelled".to_string());
            }

            match serial.with_conn(|conn| conn.send_and_recv(frame, timeout)) {
                Ok(resp) => return Ok(resp),
                Err(e) => {
                    if attempt < config.max_retries {
                        Self::emit_log(
                            app,
                            "warn",
                            &format!("{} retry {}/{}: {}", label, attempt, config.max_retries, e),
                        );
                        std::thread::sleep(Duration::from_millis(200));
                    } else {
                        let msg = format!(
                            "{} failed after {} retries: {}",
                            label, config.max_retries, e
                        );
                        Self::emit_error(app, &msg);
                        return Err(msg);
                    }
                }
            }
        }
        unreachable!()
    }

    /// Poll MCU for buffered logs and emit them to the frontend.
    fn poll_logs(app: &AppHandle, serial: &SerialManager) {
        let Ok(frame) = commands::build_get_log() else {
            return;
        };
        let resp = match serial
            .with_conn(|conn| conn.send_and_recv(&frame, Duration::from_millis(1500)))
        {
            Ok(r) => r,
            Err(e) => {
                log::warn!("[OTA] poll_logs send/recv failed: {}", e);
                return;
            }
        };
        let Some(text) = commands::parse_get_log(&resp) else {
            return;
        };
        for line in text.lines() {
            let trimmed = line.trim();
            if !trimmed.is_empty() {
                Self::emit_log(app, "debug", &format!("[MCU] {}", trimmed));
            }
        }
    }
}

unsafe impl Send for OtaEngine {}
unsafe impl Sync for OtaEngine {}
