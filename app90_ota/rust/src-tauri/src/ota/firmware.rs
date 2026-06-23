use std::fs;
use std::path::Path;

/// A chunk of firmware data with its offset.
#[derive(Debug, Clone)]
pub struct FirmwarePacket {
    pub offset: u32,
    pub data: Vec<u8>,
}

/// Load a firmware binary from disk and return its contents.
pub fn load_firmware(path: &Path) -> Result<Vec<u8>, String> {
    fs::read(path).map_err(|e| format!("Failed to read firmware file: {}", e))
}

/// Calculate CRC32 of data (standard CRC32/ISO-HDLC).
pub fn calc_crc32(data: &[u8]) -> u32 {
    crc32fast::hash(data)
}

/// Split firmware data into fixed-size packets.
/// Each packet is at most `chunk_size` bytes.
pub fn split_into_packets(data: &[u8], chunk_size: usize) -> Vec<FirmwarePacket> {
    let total = data.len();
    let chunk_size = chunk_size.max(1);
    (0..total)
        .step_by(chunk_size)
        .map(|offset| {
            let end = (offset + chunk_size).min(total);
            FirmwarePacket {
                offset: offset as u32,
                data: data[offset..end].to_vec(),
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_split_exact() {
        let data = vec![0xAA; 100];
        let packets = split_into_packets(&data, 25);
        assert_eq!(packets.len(), 4);
        assert_eq!(packets[0].offset, 0);
        assert_eq!(packets[1].offset, 25);
        assert_eq!(packets[3].offset, 75);
        assert_eq!(packets[0].data.len(), 25);
        assert_eq!(packets[3].data.len(), 25);
    }

    #[test]
    fn test_split_uneven() {
        let data = vec![0xBB; 100];
        let packets = split_into_packets(&data, 30);
        assert_eq!(packets.len(), 4);
        assert_eq!(packets[3].data.len(), 10);
    }

    #[test]
    fn test_crc32_consistency() {
        let data = b"hello world";
        let crc1 = calc_crc32(data);
        let crc2 = calc_crc32(data);
        assert_eq!(crc1, crc2);
    }
}

