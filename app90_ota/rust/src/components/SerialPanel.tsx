interface Props {
    ports: string[];
    portName: string;
    baudRate: number;
    connected: boolean;
    error: string | null;
    onPortNameChange: (name: string) => void;
    onBaudRateChange: (rate: number) => void;
    onRefresh: () => void;
    onConnect: () => void;
    onDisconnect: () => void;
}

const BAUD_RATES = [9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600];

export function SerialPanel({
    ports, portName, baudRate, connected, error,
    onPortNameChange, onBaudRateChange, onRefresh, onConnect, onDisconnect,
}: Props) {
    return (
        <div className="panel">
            <h3>串口设置</h3>
            <div className="form-row">
                <label>端口</label>
                <select value={portName} onChange={(e) => onPortNameChange(e.target.value)} disabled={connected}>
                    <option value="">-- 选择串口 --</option>
                    {ports.map((p) => (
                        <option key={p} value={p}>{p}</option>
                    ))}
                </select>
                <button className="btn-sm" onClick={onRefresh} disabled={connected}>刷新</button>
            </div>
            <div className="form-row">
                <label>波特率</label>
                <select value={baudRate} onChange={(e) => onBaudRateChange(Number(e.target.value))} disabled={connected}>
                    {BAUD_RATES.map((r) => (
                        <option key={r} value={r}>{r}</option>
                    ))}
                </select>
            </div>
            <div className="form-row">
                {connected ? (
                    <button className="btn btn-disconnect" onClick={onDisconnect}>断开</button>
                ) : (
                    <button className="btn btn-connect" onClick={onConnect} disabled={!portName}>连接</button>
                )}
                {connected && <span className="status-dot connected">已连接</span>}
            </div>
            {error && <p className="error-text">{error}</p>}
        </div>
    );
}
