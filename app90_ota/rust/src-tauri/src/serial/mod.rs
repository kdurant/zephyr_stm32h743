pub mod port;

use std::sync::{Arc, Mutex};

use self::port::SerialConnection;

/// Global serial connection manager.
/// Shared across Tauri commands via `tauri::State`.
pub struct SerialManager {
    pub conn: Mutex<Option<SerialConnection>>,
}

impl SerialManager {
    pub fn new() -> Self {
        Self {
            conn: Mutex::new(None),
        }
    }

    /// Create Arc-wrapped manager for Tauri state.
    pub fn shared() -> Arc<Self> {
        Arc::new(Self::new())
    }

    /// Check if connected.
    pub fn is_connected(&self) -> bool {
        self.conn.lock().unwrap().is_some()
    }

    /// Execute a closure with the serial connection.
    pub fn with_conn<F, R>(&self, f: F) -> Result<R, String>
    where
        F: FnOnce(&mut SerialConnection) -> Result<R, String>,
    {
        let mut guard = self.conn.lock().unwrap();
        match guard.as_mut() {
            Some(conn) => f(conn),
            None => Err("Serial port not connected".to_string()),
        }
    }

    /// Set the connection (connect).
    pub fn set_connection(&self, conn: SerialConnection) {
        let mut guard = self.conn.lock().unwrap();
        *guard = Some(conn);
    }

    /// Clear the connection (disconnect).
    pub fn clear_connection(&self) {
        let mut guard = self.conn.lock().unwrap();
        *guard = None; // dropping the connection closes the port
    }
}

