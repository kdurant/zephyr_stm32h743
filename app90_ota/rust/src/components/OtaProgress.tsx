import type { UpgradeStage } from "../types/protocol";

interface Props {
    stage: UpgradeStage;
    percentage: number;
    message: string;
    sent: number;
    total: number;
    connected: boolean;
}

const STAGE_LABELS: Record<UpgradeStage, string> = {
    Idle: "空闲",
    Handshaking: "握手中",
    GettingDeviceInfo: "获取设备信息",
    Erasing: "擦除 Flash",
    Transferring: "传输中",
    Completing: "传输完成",
    Verifying: "校验中",
    SettingBootFlag: "设置启动标志",
    Resetting: "复位中",
    Complete: "升级完成 ✓",
    Cancelled: "已取消",
    Error: "错误",
};

function formatBytes(bytes: number): string {
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
}

export function OtaProgress({ stage, percentage, message, sent, total, connected }: Props) {
    const isActive = stage !== "Idle" && stage !== "Complete" && stage !== "Cancelled" && stage !== "Error";
    const barClass = stage === "Error" ? "progress-error" : stage === "Complete" ? "progress-done" : "";

    return (
        <div className="panel">
            <h3>升级进度</h3>
            {!connected && stage === "Idle" ? (
                <p className="muted">请先连接串口</p>
            ) : (
                <>
                    <div className="stage-label">{STAGE_LABELS[stage] || stage}</div>
                    <div className="progress-bar">
                        <div
                            className={`progress-fill ${barClass}`}
                            style={{ width: `${percentage}%`, transition: "width 0.3s" }}
                        />
                    </div>
                    <div className="progress-details">
                        {isActive && <span>{message}</span>}
                        {total > 0 && (
                            <span className="muted">
                                {formatBytes(sent)} / {formatBytes(total)} ({percentage.toFixed(1)}%)
                            </span>
                        )}
                    </div>
                </>
            )}
        </div>
    );
}
