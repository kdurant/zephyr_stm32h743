// Prevents additional console window on Windows in release
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

fn main() {
    // ── Linux WebKitGTK workaround ──
    // On some Linux systems, WebKitGTK's GPU compositing mode causes the
    // webview to render as a black screen. Disabling compositing mode
    // forces CPU-based rendering which works reliably.
    #[cfg(target_os = "linux")]
    {
        std::env::set_var("WEBKIT_DISABLE_COMPOSITING_MODE", "1");
    }

    // Initialize env_logger so that log::info!() etc appear on stderr
    env_logger::init();

    ota_pc_tool_lib::run()
}
