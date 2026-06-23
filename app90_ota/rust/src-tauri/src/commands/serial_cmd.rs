use std::sync::Arc;
use std::time::Duration;

use tauri::State;

use crate::serial::{port, SerialManager};

/// List available serial port names (only USB serial: ttyACM*, ttyUSB*).
#[tauri::command]
pub fn list_serial_ports() -> Result<Vec<String>, String> {
    let ports = port::list_ports();
    Ok(ports
        .iter()
        .map(|p| p.port_name.clone())
        .filter(|name| name.contains("ttyACM") || name.contains("ttyUSB"))
        .collect())
}

/// Connect to a serial port.
#[tauri::command]
pub fn connect_serial(
    serial: State<'_, Arc<SerialManager>>,
    port_name: String,
    baud_rate: u32,
) -> Result<(), String> {
    if serial.is_connected() {
        serial.clear_connection();
    }
    let conn = port::SerialConnection::open(&port_name, baud_rate, Duration::from_secs(3))?;
    serial.set_connection(conn);
    log::info!(
        "Connected to serial port: {} @ {} baud",
        port_name,
        baud_rate
    );
    Ok(())
}

/// Disconnect from the serial port.
#[tauri::command]
pub fn disconnect_serial(serial: State<'_, Arc<SerialManager>>) -> Result<(), String> {
    serial.clear_connection();
    log::info!("Serial port disconnected");
    Ok(())
}

/// Get connection status.
#[tauri::command]
pub fn is_connected(serial: State<'_, Arc<SerialManager>>) -> Result<bool, String> {
    Ok(serial.is_connected())
}
