// ── Protocol layer ──
pub mod protocol;

// ── Serial communication ──
pub mod serial;

// ── OTA engine ──
pub mod ota;

// ── Tauri commands ──
pub mod commands;

use ota::engine::OtaEngine;
use ota::types::OtaConfig;
use serial::SerialManager;
use std::sync::Arc;

// ────────────────────────────────────────────
// App entry
// ────────────────────────────────────────────

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_fs::init())
        .manage(Arc::new(SerialManager::new()))
        .manage(OtaEngine::new(OtaConfig::default()))
        .setup(|_app| {
            log::info!("OTA firmware upgrade tool started");
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            // Serial commands
            commands::serial_cmd::list_serial_ports,
            commands::serial_cmd::connect_serial,
            commands::serial_cmd::disconnect_serial,
            commands::serial_cmd::is_connected,
            // OTA commands
            commands::ota_cmd::handshake,
            commands::ota_cmd::get_device_info,
            commands::ota_cmd::start_upgrade,
            commands::ota_cmd::cancel_upgrade,
            commands::ota_cmd::query_status,
            commands::ota_cmd::verify_firmware,
            commands::ota_cmd::reset_device,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
