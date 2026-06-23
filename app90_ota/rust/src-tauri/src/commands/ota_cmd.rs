use std::sync::Arc;
use std::time::Duration;

use tauri::{AppHandle, Emitter, State};

use crate::serial::SerialManager;
use crate::ota::engine::OtaEngine;
use crate::protocol::commands;
use crate::protocol::frame::Frame;

/// Send a frame and wait for response (helper for simple request-response commands).
fn send_and_recv(serial: &SerialManager, frame: Frame, label: &str) -> Result<Frame, String> {
    let timeout = Duration::from_secs(3);
    serial.with_conn(|conn| conn.send_and_recv(&frame, timeout))
        .map_err(|e| format!("{}: {}", label, e))
}

// ═══════════════════════════════════════════
// Handshake
// ═══════════════════════════════════════════

#[tauri::command]
pub fn handshake(
    serial: State<'_, Arc<SerialManager>>,
) -> Result<String, String> {
    let frame = commands::build_handshake().map_err(|e| e.to_string())?;
    let resp = send_and_recv(&serial, frame, "handshake")?;
    if let Some((status, major, minor)) = commands::parse_handshake_response(&resp) {
        Ok(format!("status={:#04X}, protocol=v{}.{}", status, major, minor))
    } else {
        Err("Failed to parse handshake response".into())
    }
}

// ═══════════════════════════════════════════
// Get device info
// ═══════════════════════════════════════════

#[tauri::command]
pub fn get_device_info(
    app: AppHandle,
    serial: State<'_, Arc<SerialManager>>,
) -> Result<(), String> {
    let frame = commands::build_get_device_info().map_err(|e| e.to_string())?;
    let resp = send_and_recv(&serial, frame, "get_device_info")?;
    if let Some(info) = commands::parse_device_info(&resp) {
        let _ = app.emit("ota:event", crate::ota::types::OtaEvent::DeviceInfo {
            info: info.clone(),
        });
        Ok(())
    } else {
        Err("Failed to parse device info response".into())
    }
}

// ═══════════════════════════════════════════
// Start upgrade (full flow)
// ═══════════════════════════════════════════

#[tauri::command]
pub fn start_upgrade(
    app: AppHandle,
    engine: State<'_, OtaEngine>,
    serial: State<'_, Arc<SerialManager>>,
    firmware_path: String,
) -> Result<(), String> {
    if !serial.is_connected() {
        return Err("Serial port not connected".into());
    }
    engine.run_upgrade(app, serial.inner().clone(), firmware_path);
    Ok(())
}

// ═══════════════════════════════════════════
// Cancel upgrade
// ═══════════════════════════════════════════

#[tauri::command]
pub fn cancel_upgrade(
    engine: State<'_, OtaEngine>,
    serial: State<'_, Arc<SerialManager>>,
) -> Result<(), String> {
    engine.cancel();
    if let Ok(frame) = commands::build_cancel_upgrade() {
        let _ = send_and_recv(&serial, frame, "cancel_upgrade");
    }
    Ok(())
}

// ═══════════════════════════════════════════
// Query status
// ═══════════════════════════════════════════

#[tauri::command]
pub fn query_status(
    serial: State<'_, Arc<SerialManager>>,
) -> Result<String, String> {
    let frame = commands::build_query_status().map_err(|e| e.to_string())?;
    let resp = send_and_recv(&serial, frame, "query_status")?;
    if let Some(status) = commands::parse_query_status(&resp) {
        let stage_str = match status.stage {
            0 => "idle",
            1 => "erasing",
            2 => "receiving",
            3 => "verifying",
            4 => "complete",
            _ => "error",
        };
        Ok(format!(
            "stage={}, received={}/{} bytes, error={:#04X}",
            stage_str, status.received_bytes, status.total_size, status.error_code
        ))
    } else {
        Err("Failed to parse status response".into())
    }
}

// ═══════════════════════════════════════════
// Verify firmware
// ═══════════════════════════════════════════

#[tauri::command]
pub fn verify_firmware(
    serial: State<'_, Arc<SerialManager>>,
) -> Result<String, String> {
    let frame = commands::build_verify_firmware().map_err(|e| e.to_string())?;
    let resp = send_and_recv(&serial, frame, "verify_firmware")?;
    if let Some((result, crc)) = commands::parse_verify_response(&resp) {
        let status = match result {
            0x00 => "CRC match",
            0x01 => "CRC mismatch",
            _ => "verify failed",
        };
        Ok(format!("{} (CRC32=0x{:08X})", status, crc))
    } else {
        Err("Failed to parse verify response".into())
    }
}

// ═══════════════════════════════════════════
// Reset device
// ═══════════════════════════════════════════

#[tauri::command]
pub fn reset_device(
    serial: State<'_, Arc<SerialManager>>,
    delay_ms: u16,
) -> Result<(), String> {
    let frame = commands::build_reset_run(delay_ms).map_err(|e| e.to_string())?;
    let resp = send_and_recv(&serial, frame, "reset_run")?;
    if resp.is_error_response() {
        Err("Device refused reset".into())
    } else {
        Ok(())
    }
}

