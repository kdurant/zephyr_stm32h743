use serde::{Deserialize, Serialize};

/// OTA upgrade stages
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum UpgradeStage {
    Idle,
    Handshaking,
    GettingDeviceInfo,
    Erasing,
    Transferring,
    Completing,
    Verifying,
    SettingBootFlag,
    Resetting,
    Complete,
    Cancelled,
    Error,
}

impl UpgradeStage {
    pub fn as_str(&self) -> &'static str {
        match self {
            Self::Idle => "idle",
            Self::Handshaking => "handshaking",
            Self::GettingDeviceInfo => "getting_device_info",
            Self::Erasing => "erasing",
            Self::Transferring => "transferring",
            Self::Completing => "completing",
            Self::Verifying => "verifying",
            Self::SettingBootFlag => "setting_boot_flag",
            Self::Resetting => "resetting",
            Self::Complete => "complete",
            Self::Cancelled => "cancelled",
            Self::Error => "error",
        }
    }
}

/// Progress information sent to frontend
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct OtaProgress {
    pub stage: UpgradeStage,
    pub message: String,
    pub bytes_sent: u64,
    pub total_bytes: u64,
    pub percentage: f64,
}

// Re-export DeviceInfo from protocol commands
pub use crate::protocol::commands::DeviceInfo;

/// Event payloads emitted to frontend via Tauri events
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "type")]
pub enum OtaEvent {
    #[serde(rename = "progress")]
    Progress(OtaProgress),
    #[serde(rename = "log")]
    Log { level: String, message: String },
    #[serde(rename = "error")]
    Error { message: String },
    #[serde(rename = "device_info")]
    DeviceInfo { info: DeviceInfo },
    #[serde(rename = "stage_changed")]
    StageChanged { stage: UpgradeStage },
}

/// Configuration for the OTA engine
#[derive(Debug, Clone)]
pub struct OtaConfig {
    /// Max retries per command
    pub max_retries: u32,
    /// Timeout per command (milliseconds)
    pub command_timeout_ms: u64,
}

impl Default for OtaConfig {
    fn default() -> Self {
        Self {
            max_retries: 3,
            command_timeout_ms: 3000,
        }
    }
}

