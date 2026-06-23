// ── Matching Rust types from ota/types.rs ──

export interface DeviceInfo {
    mcu_model: string;
    fw_version: string;
    flash_total_size: number;
    flash_page_size: number;
    flash_used_size: number;
    fw_start_addr: number;
}

export type UpgradeStage =
    | "Idle"
    | "Handshaking"
    | "GettingDeviceInfo"
    | "Erasing"
    | "Transferring"
    | "Completing"
    | "Verifying"
    | "SettingBootFlag"
    | "Resetting"
    | "Complete"
    | "Cancelled"
    | "Error";

export interface OtaProgress {
    stage: UpgradeStage;
    message: string;
    bytes_sent: number;
    total_bytes: number;
    percentage: number;
}

export interface OtaLog {
    level: string;
    message: string;
}

export type OtaEvent =
    | { type: "progress"; payload: OtaProgress }
    | { type: "log"; payload: OtaLog }
    | { type: "error"; payload: { message: string } }
    | { type: "device_info"; payload: { info: DeviceInfo } }
    | { type: "stage_changed"; payload: { stage: UpgradeStage } };

// The actual event format from Rust (serde tag-based):
export interface OtaEventRaw {
    type: string;
    // progress
    stage?: UpgradeStage;
    message?: string;
    bytes_sent?: number;
    total_bytes?: number;
    percentage?: number;
    // log
    level?: string;
    // error
    // device_info
    info?: DeviceInfo;
}

export interface SerialPort {
    name: string;
}
