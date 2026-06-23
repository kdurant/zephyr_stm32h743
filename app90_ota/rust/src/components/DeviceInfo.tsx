import type { DeviceInfo } from "../types/protocol";

interface Props {
    info: DeviceInfo | null;
}

export function DeviceInfo({ info }: Props) {
    if (!info) {
        return (
            <div className="panel">
                <h3>设备信息</h3>
                <p className="muted">未获取设备信息，请先握手</p>
            </div>
        );
    }

    const flashMb = (info.flash_total_size / (1024 * 1024)).toFixed(1);
    const usedKb = (info.flash_used_size / 1024).toFixed(1);

    return (
        <div className="panel">
            <h3>设备信息</h3>
            <table className="info-table">
                <tbody>
                    <tr><td>型号</td><td>{info.mcu_model}</td></tr>
                    <tr><td>固件版本</td><td>{info.fw_version}</td></tr>
                    <tr><td>Flash 容量</td><td>{flashMb} MB</td></tr>
                    <tr><td>Flash 页大小</td><td>{info.flash_page_size} B</td></tr>
                    <tr><td>已用空间</td><td>{usedKb} KB</td></tr>
                    <tr><td>固件起始地址</td><td>0x{info.fw_start_addr.toString(16).toUpperCase().padStart(8, "0")}</td></tr>
                </tbody>
            </table>
        </div>
    );
}
