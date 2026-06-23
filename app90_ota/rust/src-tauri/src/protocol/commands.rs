// ── Command codes ──
pub const CMD_HANDSHAKE: u16 = 0x0001;
pub const CMD_GET_DEVICE_INFO: u16 = 0x0002;
pub const CMD_START_UPGRADE: u16 = 0x0003;
pub const CMD_TRANSFER_DATA: u16 = 0x0004;
pub const CMD_TRANSFER_COMPLETE: u16 = 0x0005;
pub const CMD_VERIFY_FIRMWARE: u16 = 0x0006;
pub const CMD_RESET_RUN: u16 = 0x0007;
pub const CMD_CANCEL_UPGRADE: u16 = 0x0008;
pub const CMD_QUERY_STATUS: u16 = 0x0009;
pub const CMD_SET_BOOT_FLAG: u16 = 0x000A;

use crate::protocol::frame::Frame;

// ═══════════════════════════════════════════════
// 0x0001 — Handshake
// ═══════════════════════════════════════════════

/// Build handshake request (no data).
pub fn build_handshake() -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_HANDSHAKE, vec![])
}

/// Parse handshake response.
/// Returns (device_status, protocol_major, protocol_minor).
pub fn parse_handshake_response(frame: &Frame) -> Option<(u8, u8, u8)> {
    if frame.data.len() >= 3 {
        Some((frame.data[0], frame.data[1], frame.data[2]))
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x0002 — Get Device Info
// ═══════════════════════════════════════════════

pub fn build_get_device_info() -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_GET_DEVICE_INFO, vec![])
}

#[derive(Debug, Clone, Default, serde::Serialize, serde::Deserialize)]
pub struct DeviceInfo {
    pub mcu_model: String,
    pub fw_version: String,    // "V1.0.1"
    pub flash_total_size: u32, // bytes
    pub flash_page_size: u16,  // bytes
    pub flash_used_size: u32,  // bytes
    pub fw_start_addr: u32,
}

/// Parse device info response.
pub fn parse_device_info(frame: &Frame) -> Option<DeviceInfo> {
    if frame.data.len() < 34 {
        return None;
    }
    let d = &frame.data;
    let mcu_model = String::from_utf8_lossy(&d[0..16])
        .trim_end_matches('\0')
        .to_string();
    let fw_version = format!(
        "V{}.{}.{}",
        d[16],
        d[17],
        u16::from_le_bytes([d[18], d[19]])
    );
    let flash_total_size = u32::from_le_bytes([d[20], d[21], d[22], d[23]]);
    let flash_page_size = u16::from_le_bytes([d[24], d[25]]);
    let flash_used_size = u32::from_le_bytes([d[26], d[27], d[28], d[29]]);
    let fw_start_addr = u32::from_le_bytes([d[30], d[31], d[32], d[33]]);

    Some(DeviceInfo {
        mcu_model,
        fw_version,
        flash_total_size,
        flash_page_size,
        flash_used_size,
        fw_start_addr,
    })
}

// ═══════════════════════════════════════════════
// 0x0003 — Start Upgrade
// ═══════════════════════════════════════════════

/// Build start upgrade request.
pub fn build_start_upgrade(
    total_size: u32,
    crc32: u32,
    version: &[u8; 4],
) -> Result<Frame, crate::protocol::frame::FrameError> {
    let mut data = Vec::with_capacity(12);
    data.extend_from_slice(&total_size.to_le_bytes());
    data.extend_from_slice(&crc32.to_le_bytes());
    data.extend_from_slice(version);
    Frame::new(CMD_START_UPGRADE, data)
}

/// Parse start upgrade response.
/// Returns (status, max_packet_size, erase_progress).
pub fn parse_start_upgrade_response(frame: &Frame) -> Option<(u8, u16, u8)> {
    if frame.data.len() >= 4 {
        let status = frame.data[0];
        let max_pkt = u16::from_le_bytes([frame.data[1], frame.data[2]]);
        let erase_progress = frame.data[3];
        Some((status, max_pkt, erase_progress))
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x0004 — Transfer Data
// ═══════════════════════════════════════════════

/// Build a firmware data transfer packet.
pub fn build_transfer_data(
    offset: u32,
    chunk: &[u8],
    packet_seq: u16,
    total_packets: u16,
) -> Result<Frame, crate::protocol::frame::FrameError> {
    let mut data = Vec::with_capacity(4 + chunk.len());
    data.extend_from_slice(&offset.to_le_bytes());
    data.extend_from_slice(chunk);
    let mut f = Frame::new(CMD_TRANSFER_DATA, data)?;
    f.packet_seq = packet_seq;
    f.total_packets = total_packets;
    Ok(f)
}

/// Parse transfer data response.
/// Returns (status, received_offset).
pub fn parse_transfer_data_response(frame: &Frame) -> Option<(u8, u32)> {
    if frame.data.len() >= 5 {
        let status = frame.data[0];
        let offset =
            u32::from_le_bytes([frame.data[1], frame.data[2], frame.data[3], frame.data[4]]);
        Some((status, offset))
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x0005 — Transfer Complete
// ═══════════════════════════════════════════════

pub fn build_transfer_complete(
    crc32: u32,
    total_size: u32,
) -> Result<Frame, crate::protocol::frame::FrameError> {
    let mut data = Vec::with_capacity(8);
    data.extend_from_slice(&crc32.to_le_bytes());
    data.extend_from_slice(&total_size.to_le_bytes());
    Frame::new(CMD_TRANSFER_COMPLETE, data)
}

/// Parse transfer complete response.
/// Returns (status, received_size).
pub fn parse_transfer_complete_response(frame: &Frame) -> Option<(u8, u32)> {
    if frame.data.len() >= 5 {
        let status = frame.data[0];
        let size = u32::from_le_bytes([frame.data[1], frame.data[2], frame.data[3], frame.data[4]]);
        Some((status, size))
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x0006 — Verify Firmware
// ═══════════════════════════════════════════════

pub fn build_verify_firmware() -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_VERIFY_FIRMWARE, vec![])
}

/// Parse verify firmware response.
/// Returns (result, calculated_crc32).
/// result: 0x00=match, 0x01=mismatch, 0xFF=failed
pub fn parse_verify_response(frame: &Frame) -> Option<(u8, u32)> {
    if frame.data.len() >= 5 {
        let result = frame.data[0];
        let crc = u32::from_le_bytes([frame.data[1], frame.data[2], frame.data[3], frame.data[4]]);
        Some((result, crc))
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x0007 — Reset Run
// ═══════════════════════════════════════════════

pub fn build_reset_run(delay_ms: u16) -> Result<Frame, crate::protocol::frame::FrameError> {
    let data = delay_ms.to_le_bytes().to_vec();
    Frame::new(CMD_RESET_RUN, data)
}

// ═══════════════════════════════════════════════
// 0x0008 — Cancel Upgrade
// ═══════════════════════════════════════════════

pub fn build_cancel_upgrade() -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_CANCEL_UPGRADE, vec![])
}

// ═══════════════════════════════════════════════
// 0x0009 — Query Status
// ═══════════════════════════════════════════════

pub fn build_query_status() -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_QUERY_STATUS, vec![])
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct UpgradeStatus {
    pub stage: u8, // 0=idle, 1=erasing, 2=receiving, 3=verifying, 4=complete, 0xFF=error
    pub received_bytes: u32,
    pub total_size: u32,
    pub error_code: u8,
}

/// Parse query status response.
pub fn parse_query_status(frame: &Frame) -> Option<UpgradeStatus> {
    if frame.data.len() >= 10 {
        Some(UpgradeStatus {
            stage: frame.data[0],
            received_bytes: u32::from_le_bytes([
                frame.data[1],
                frame.data[2],
                frame.data[3],
                frame.data[4],
            ]),
            total_size: u32::from_le_bytes([
                frame.data[5],
                frame.data[6],
                frame.data[7],
                frame.data[8],
            ]),
            error_code: frame.data[9],
        })
    } else {
        None
    }
}

// ═══════════════════════════════════════════════
// 0x000A — Set Boot Flag
// ═══════════════════════════════════════════════

/// Build set boot flag request.
/// flag: 0x00 = rollback to old firmware, 0x01 = mark new firmware as available
pub fn build_set_boot_flag(flag: u8) -> Result<Frame, crate::protocol::frame::FrameError> {
    Frame::new(CMD_SET_BOOT_FLAG, vec![flag])
}
