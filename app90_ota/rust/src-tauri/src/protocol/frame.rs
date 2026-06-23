use thiserror::Error;

// ── Frame constants ──
pub const FRAME_HEADER: [u8; 2] = [0x12, 0x34];
pub const FRAME_TAIL: [u8; 2] = [0xCD, 0xEF];
pub const HEADER_SIZE: usize = 2;
pub const FIXED_FIELD_SIZE: usize = 14; // seq(2) + src(2) + dst(2) + total(2) + pkt_seq(2) + cmd(2) + len(2)
pub const PREFIX_SIZE: usize = HEADER_SIZE + FIXED_FIELD_SIZE; // 16
pub const SUFFIX_SIZE: usize = 4; // rsv(1) + rsv(1) + tail(2)
pub const MIN_FRAME_SIZE: usize = PREFIX_SIZE + SUFFIX_SIZE; // 20

// ── Address constants ──
pub const ADDR_PC: u16 = 0x0000;
pub const ADDR_DEVICE: u16 = 0x0020;
pub const ADDR_DAQ: u16 = 0x0010;

/// Error response marker: 4 bytes of 0xFF in data area
pub const ERROR_MARKER: [u8; 4] = [0xFF, 0xFF, 0xFF, 0xFF];

// ── Frame struct ──

#[derive(Debug, Clone, PartialEq)]
pub struct Frame {
    pub seq: u16,
    pub src_addr: u16,
    pub dst_addr: u16,
    pub total_packets: u16,
    pub packet_seq: u16,
    pub command: u16,
    pub data: Vec<u8>,
}

#[derive(Error, Debug)]
pub enum FrameError {
    #[error("data too long: {0} bytes (max 65535)")]
    DataTooLong(usize),
    #[error("buffer too short: need at least {need} bytes, got {got}")]
    BufferTooShort { need: usize, got: usize },
    #[error("invalid frame header: expected {expected:02X?}, got {got:02X?}")]
    InvalidHeader { expected: [u8; 2], got: [u8; 2] },
    #[error("invalid frame tail: expected {expected:02X?}, got {got:02X?}")]
    InvalidTail { expected: [u8; 2], got: [u8; 2] },
    #[error("declared data length {declared} doesn't match actual {actual}")]
    DataLengthMismatch { declared: u16, actual: usize },
}

impl Frame {
    /// Build a request frame from PC (0x00) to device (0x20).
    /// Automatically fills src_addr, dst_addr, total_packets, packet_seq.
    pub fn new(command: u16, data: Vec<u8>) -> Result<Self, FrameError> {
        if data.len() > u16::MAX as usize {
            return Err(FrameError::DataTooLong(data.len()));
        }
        Ok(Self {
            seq: 0,
            src_addr: ADDR_PC,
            dst_addr: ADDR_DEVICE,
            total_packets: 1,
            packet_seq: 0,
            command,
            data,
        })
    }

    /// Build a frame with full control over all fields.
    pub fn with_fields(
        seq: u16,
        src_addr: u16,
        dst_addr: u16,
        total_packets: u16,
        packet_seq: u16,
        command: u16,
        data: Vec<u8>,
    ) -> Result<Self, FrameError> {
        if data.len() > u16::MAX as usize {
            return Err(FrameError::DataTooLong(data.len()));
        }
        Ok(Self {
            seq,
            src_addr,
            dst_addr,
            total_packets,
            packet_seq,
            command,
            data,
        })
    }

    /// Serialize frame to bytes.
    pub fn to_bytes(&self) -> Vec<u8> {
        let data_len = self.data.len() as u16;
        let total = PREFIX_SIZE + data_len as usize + SUFFIX_SIZE;
        let mut buf = Vec::with_capacity(total);

        buf.extend_from_slice(&FRAME_HEADER);
        buf.extend_from_slice(&self.seq.to_le_bytes());
        buf.extend_from_slice(&self.src_addr.to_le_bytes());
        buf.extend_from_slice(&self.dst_addr.to_le_bytes());
        buf.extend_from_slice(&self.total_packets.to_le_bytes());
        buf.extend_from_slice(&self.packet_seq.to_le_bytes());
        buf.extend_from_slice(&self.command.to_le_bytes());
        buf.extend_from_slice(&data_len.to_le_bytes());
        buf.extend_from_slice(&self.data);
        buf.push(0x00); // reserved
        buf.push(0x00); // reserved
        buf.extend_from_slice(&FRAME_TAIL);

        buf
    }

    /// Deserialize frame from bytes.
    pub fn from_bytes(buf: &[u8]) -> Result<(Self, usize), FrameError> {
        if buf.len() < MIN_FRAME_SIZE {
            return Err(FrameError::BufferTooShort {
                need: MIN_FRAME_SIZE,
                got: buf.len(),
            });
        }

        // Validate header
        let header: [u8; 2] = [buf[0], buf[1]];
        if header != FRAME_HEADER {
            return Err(FrameError::InvalidHeader {
                expected: FRAME_HEADER,
                got: header,
            });
        }

        let mut pos = HEADER_SIZE;

        // Read fixed fields (little-endian)
        let seq = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let src_addr = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let dst_addr = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let total_packets = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let packet_seq = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let command = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;
        let data_len = u16::from_le_bytes([buf[pos], buf[pos + 1]]);
        pos += 2;

        let total_expected = PREFIX_SIZE + data_len as usize + SUFFIX_SIZE;
        if buf.len() < total_expected {
            return Err(FrameError::BufferTooShort {
                need: total_expected,
                got: buf.len(),
            });
        }

        // Data
        let data = buf[pos..pos + data_len as usize].to_vec();
        pos += data_len as usize;

        // Reserved bytes (skip)
        pos += 2;

        // Validate tail
        let tail: [u8; 2] = [buf[pos], buf[pos + 1]];
        if tail != FRAME_TAIL {
            return Err(FrameError::InvalidTail {
                expected: FRAME_TAIL,
                got: tail,
            });
        }

        let frame = Frame {
            seq,
            src_addr,
            dst_addr,
            total_packets,
            packet_seq,
            command,
            data,
        };

        Ok((frame, total_expected))
    }

    /// Check if this frame is an error response (data == 4 bytes of 0xFF).
    pub fn is_error_response(&self) -> bool {
        self.data.len() == 4 && self.data == ERROR_MARKER
    }

    /// Check if this frame has a status byte at data[0] indicating success (0x00).
    pub fn is_success(&self) -> bool {
        !self.data.is_empty() && self.data[0] == 0x00
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_roundtrip_empty_data() {
        let f = Frame::new(0x0001, vec![]).unwrap();
        let bytes = f.to_bytes();
        let (f2, consumed) = Frame::from_bytes(&bytes).unwrap();
        assert_eq!(consumed, bytes.len());
        assert_eq!(f, f2);
    }

    #[test]
    fn test_roundtrip_with_data() {
        let data = vec![0xAA, 0xBB, 0xCC, 0xDD, 0xEE];
        let f = Frame::new(0x0004, data.clone()).unwrap();
        let bytes = f.to_bytes();
        let (f2, consumed) = Frame::from_bytes(&bytes).unwrap();
        assert_eq!(consumed, bytes.len());
        assert_eq!(f.data, data);
        assert_eq!(f.command, 0x0004);
    }

    #[test]
    fn test_error_detection() {
        let f = Frame::new(0x0003, vec![0xFF, 0xFF, 0xFF, 0xFF]).unwrap();
        assert!(f.is_error_response());
    }

    #[test]
    fn test_success_detection() {
        let f = Frame::new(0x0003, vec![0x00]).unwrap();
        assert!(f.is_success());
    }

    #[test]
    fn test_invalid_header() {
        let mut bytes = Frame::new(0x0001, vec![]).unwrap().to_bytes();
        bytes[0] = 0x00;
        let result = Frame::from_bytes(&bytes);
        assert!(matches!(result, Err(FrameError::InvalidHeader { .. })));
    }

    #[test]
    fn test_invalid_tail() {
        let mut bytes = Frame::new(0x0001, vec![]).unwrap().to_bytes();
        let last = bytes.len() - 1;
        bytes[last] = 0x00;
        let result = Frame::from_bytes(&bytes);
        assert!(matches!(result, Err(FrameError::InvalidTail { .. })));
    }
}
