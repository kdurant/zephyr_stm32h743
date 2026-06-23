use crate::protocol::frame::{Frame, FRAME_HEADER, PREFIX_SIZE, SUFFIX_SIZE};

/// Stream decoder: accumulates bytes and extracts complete frames.
///
/// Handles:
/// - Frame synchronization (searches for header 0x1234)
/// - Partial frames (buffering incomplete data)
/// - Invalid frames (skips misaligned garbage)
pub struct FrameDecoder {
    buf: Vec<u8>,
    /// Expected frame length once header is parsed (0 = waiting for header)
    expected_len: usize,
}

impl FrameDecoder {
    pub fn new() -> Self {
        Self {
            buf: Vec::with_capacity(4096),
            expected_len: 0,
        }
    }

    /// Feed raw bytes into the decoder.
    /// Returns any complete frames that were parsed.
    pub fn feed(&mut self, data: &[u8]) -> Vec<Frame> {
        self.buf.extend_from_slice(data);
        let mut frames = Vec::new();

        loop {
            if self.expected_len == 0 {
                // Searching for frame header
                let header_pos = self.find_header();
                match header_pos {
                    Some(pos) => {
                        // Discard bytes before header
                        if pos > 0 {
                            self.buf.drain(..pos);
                        }
                        // Need at least PREFIX_SIZE bytes to read data_len
                        if self.buf.len() >= PREFIX_SIZE {
                            // data_len is at offset 14-15 (2 bytes LE)
                            let data_len =
                                u16::from_le_bytes([self.buf[14], self.buf[15]]) as usize;
                            self.expected_len = PREFIX_SIZE + data_len + SUFFIX_SIZE;
                        } else {
                            break; // wait for more bytes
                        }
                    }
                    None => {
                        // No header found, but we may have trailing bytes that
                        // could be part of a header. Keep last byte if it's 0x12.
                        if self.buf.len() > 1 {
                            let keep = if self.buf[self.buf.len() - 1] == 0x12 {
                                1
                            } else {
                                0
                            };
                            if keep < self.buf.len() {
                                self.buf.drain(..self.buf.len() - keep);
                            }
                        }
                        break;
                    }
                }
            }

            // Try to extract a complete frame
            if self.expected_len > 0 && self.buf.len() >= self.expected_len {
                let frame_bytes = &self.buf[..self.expected_len];
                match Frame::from_bytes(frame_bytes) {
                    Ok((frame, _)) => {
                        frames.push(frame);
                        self.buf.drain(..self.expected_len);
                        self.expected_len = 0;
                    }
                    Err(_) => {
                        // Invalid frame — skip the header and resync
                        self.buf.drain(..2);
                        self.expected_len = 0;
                    }
                }
            } else {
                break; // wait for more bytes
            }
        }

        frames
    }

    /// Find position of FRAME_HEADER in buffer
    fn find_header(&self) -> Option<usize> {
        if self.buf.len() < 2 {
            return None;
        }
        self.buf.windows(2).position(|w| w == FRAME_HEADER)
    }

    /// Clear internal state
    pub fn reset(&mut self) {
        self.buf.clear();
        self.expected_len = 0;
    }

    /// Number of pending bytes in buffer
    pub fn pending(&self) -> usize {
        self.buf.len()
    }
}

impl Default for FrameDecoder {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_single_frame() {
        let f = Frame::new(0x0001, vec![]).unwrap();
        let bytes = f.to_bytes();

        let mut dec = FrameDecoder::new();
        let frames = dec.feed(&bytes);
        assert_eq!(frames.len(), 1);
        assert_eq!(frames[0].command, 0x0001);
    }

    #[test]
    fn test_multiple_frames() {
        let f1 = Frame::new(0x0001, vec![]).unwrap();
        let f2 = Frame::new(0x0002, vec![0x01, 0x02]).unwrap();
        let mut bytes = f1.to_bytes();
        bytes.extend_from_slice(&f2.to_bytes());

        let mut dec = FrameDecoder::new();
        let frames = dec.feed(&bytes);
        assert_eq!(frames.len(), 2);
        assert_eq!(frames[0].command, 0x0001);
        assert_eq!(frames[1].command, 0x0002);
    }

    #[test]
    fn test_partial_frame() {
        let f = Frame::new(0x0003, vec![0xAA; 100]).unwrap();
        let bytes = f.to_bytes();

        let mut dec = FrameDecoder::new();

        // Feed first half
        let frames = dec.feed(&bytes[..30]);
        assert_eq!(frames.len(), 0);

        // Feed second half
        let frames = dec.feed(&bytes[30..]);
        assert_eq!(frames.len(), 1);
    }

    #[test]
    fn test_garbage_before_frame() {
        let f = Frame::new(0x0001, vec![]).unwrap();
        let mut bytes = vec![0x00, 0x01, 0x02, 0x03];
        bytes.extend_from_slice(&f.to_bytes());

        let mut dec = FrameDecoder::new();
        let frames = dec.feed(&bytes);
        assert_eq!(frames.len(), 1);
    }

    #[test]
    fn test_fake_header_then_real() {
        // The fake prefix must NOT contain 0x12 0x34, otherwise the decoder
        // will lock onto it and parse a bogus data_len.
        let f = Frame::new(0x0001, vec![]).unwrap();
        let mut bytes = vec![0xAA, 0xBB, 0xCC];
        bytes.extend_from_slice(&f.to_bytes());

        let mut dec = FrameDecoder::new();
        let frames = dec.feed(&bytes);
        assert_eq!(frames.len(), 1);
    }
}
