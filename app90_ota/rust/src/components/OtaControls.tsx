import type { UpgradeStage } from "../types/protocol";

interface Props {
    stage: UpgradeStage;
    connected: boolean;
    hasFirmware: boolean;
    onHandshake: () => void;
    onGetInfo: () => void;
    onStartUpgrade: () => void;
    onCancel: () => void;
    onVerify: () => void;
    onReset: () => void;
}

export function OtaControls({
    stage, connected, hasFirmware,
    onHandshake, onGetInfo, onStartUpgrade, onCancel, onVerify, onReset,
}: Props) {
    const isBusy = stage !== "Idle" && stage !== "Complete" && stage !== "Cancelled" && stage !== "Error";
    const canStart = connected && hasFirmware && !isBusy;
    const canCancel = isBusy;

    return (
        <div className="panel">
            <h3>升级控制</h3>
            <div className="btn-group">
                <button className="btn btn-primary" onClick={onHandshake} disabled={!connected || isBusy}>
                    握手
                </button>
                <button className="btn btn-primary" onClick={onGetInfo} disabled={!connected || isBusy}>
                    获取信息
                </button>
            </div>
            <div className="btn-group">
                <button
                    className="btn btn-start"
                    onClick={onStartUpgrade}
                    disabled={!canStart}
                >
                    开始升级
                </button>
                <button
                    className="btn btn-cancel"
                    onClick={onCancel}
                    disabled={!canCancel}
                >
                    取消
                </button>
            </div>
            <div className="btn-group">
                <button className="btn btn-primary" onClick={onVerify} disabled={!connected || isBusy}>
                    校验固件
                </button>
                <button className="btn btn-primary" onClick={onReset} disabled={!connected || isBusy}>
                    复位设备
                </button>
            </div>
        </div>
    );
}
