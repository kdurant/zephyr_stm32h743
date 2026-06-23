use std::io::{Read, Write};
use std::sync::mpsc::{self, Receiver, RecvTimeoutError, Sender};
use std::time::Duration;

use serialport::{SerialPort, SerialPortInfo};

use crate::protocol::codec::FrameDecoder;
use crate::protocol::frame::Frame;

/// Wraps a serial port connection with background read thread and frame decoding.
pub struct SerialConnection {
    port: Box<dyn SerialPort>,
    frame_rx: Receiver<Frame>,
    /// Shutdown signal: when taken + dropped, background thread exits.
    shutdown_tx: Option<Sender<()>>,
    /// Background thread handle — joined on drop to ensure port FD is released.
    read_thread: Option<std::thread::JoinHandle<()>>,
}

impl Drop for SerialConnection {
    fn drop(&mut self) {
        // Signal the background thread to stop by dropping the shutdown sender
        self.shutdown_tx.take();
        // Wait for the thread to finish reading and close its port clone
        if let Some(handle) = self.read_thread.take() {
            let _ = handle.join();
        }
        log::info!("Serial connection fully closed");
    }
}

impl SerialConnection {
    /// Open a serial port and start background read task.
    pub fn open(port_name: &str, baud_rate: u32, _timeout: Duration) -> Result<Self, String> {
        let port = serialport::new(port_name, baud_rate)
            .timeout(Duration::from_millis(10)) // short timeout for responsive reads
            .open()
            .map_err(|e| format!("Failed to open serial port '{}': {}", port_name, e))?;

        let (frame_tx, frame_rx) = mpsc::channel::<Frame>();
        let (shutdown_tx, shutdown_rx) = mpsc::channel::<()>();

        let mut decoder = FrameDecoder::new();
        let mut port_clone = port
            .try_clone()
            .map_err(|e| format!("Cannot clone port: {}", e))?;
        let tx_clone = frame_tx.clone();

        // Background read loop
        let read_thread = std::thread::spawn(move || {
            let mut buf = [0u8; 1024];
            loop {
                // Check shutdown signal — exits when sender is dropped
                match shutdown_rx.try_recv() {
                    Err(std::sync::mpsc::TryRecvError::Disconnected) => break,
                    _ => {}
                }
                match port_clone.read(&mut buf) {
                    Ok(n) if n > 0 => {
                        let frames = decoder.feed(&buf[..n]);
                        for frame in frames {
                            if tx_clone.send(frame).is_err() {
                                // Receiver dropped — stop thread
                                return;
                            }
                        }
                    }
                    Ok(_) => {
                        // Read 0 bytes — port may be closed
                        std::thread::sleep(Duration::from_millis(1));
                    }
                    Err(ref e) if e.kind() == std::io::ErrorKind::TimedOut => {
                        // Timeout is normal, continue
                    }
                    Err(_) => {
                        // Port error — stop
                        break;
                    }
                }
            }
            // port_clone dropped here → FD released
        });

        Ok(Self {
            port,
            frame_rx,
            shutdown_tx: Some(shutdown_tx),
            read_thread: Some(read_thread),
        })
    }

    /// Send a frame over the serial port.
    pub fn send_frame(&mut self, frame: &Frame) -> Result<(), String> {
        let bytes = frame.to_bytes();
        self.port
            .write_all(&bytes)
            .map_err(|e| format!("Serial write error: {}", e))?;
        self.port
            .flush()
            .map_err(|e| format!("Serial flush error: {}", e))?;
        Ok(())
    }

    /// Receive a frame with a timeout.
    pub fn recv_frame(&self, timeout: Duration) -> Result<Frame, String> {
        match self.frame_rx.recv_timeout(timeout) {
            Ok(frame) => Ok(frame),
            Err(RecvTimeoutError::Timeout) => Err("Receive timeout".to_string()),
            Err(RecvTimeoutError::Disconnected) => Err("Serial connection closed".to_string()),
        }
    }

    /// Send a frame and wait for a response frame.
    /// Returns the response frame, or an error on timeout / send failure.
    pub fn send_and_recv(&mut self, frame: &Frame, timeout: Duration) -> Result<Frame, String> {
        self.send_frame(frame)?;
        self.recv_frame(timeout)
    }
}

/// List available serial ports.
pub fn list_ports() -> Vec<SerialPortInfo> {
    match serialport::available_ports() {
        Ok(ports) => {
            log::info!("Found {} serial port(s)", ports.len());
            for p in &ports {
                log::info!("  {} : {:?}", p.port_name, p.port_type);
            }
            ports
        }
        Err(e) => {
            log::error!("Failed to enumerate serial ports: {}", e);
            Vec::new()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_list_ports() {
        let ports = list_ports();
        println!("Found {} port(s):", ports.len());
        for p in &ports {
            println!("  {} : {:?}", p.port_name, p.port_type);
        }
        // Just verify it doesn't panic
        assert!(true);
    }
}
