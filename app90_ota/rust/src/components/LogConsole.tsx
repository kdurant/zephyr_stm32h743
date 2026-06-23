import { useRef, useEffect } from "react";
import type { OtaLog } from "../types/protocol";

interface Props {
    logs: OtaLog[];
    onClear: () => void;
}

const LEVEL_COLORS: Record<string, string> = {
    info: "#4fc3f7",
    warn: "#ffb74d",
    error: "#ef5350",
    success: "#66bb6a",
};

export function LogConsole({ logs, onClear }: Props) {
    const containerRef = useRef<HTMLDivElement>(null);

    useEffect(() => {
        if (containerRef.current) {
            containerRef.current.scrollTop = containerRef.current.scrollHeight;
        }
    }, [logs]);

    const formatTime = () => {
        const now = new Date();
        return now.toLocaleTimeString("zh-CN", { hour12: false });
    };

    return (
        <div className="panel log-panel">
            <div className="log-header">
                <h3>日志</h3>
                <button className="btn-sm" onClick={onClear}>清除</button>
            </div>
            <div className="log-container" ref={containerRef}>
                {logs.length === 0 ? (
                    <p className="muted">无日志</p>
                ) : (
                    logs.map((log, i) => (
                        <div key={i} className="log-entry" style={{ color: LEVEL_COLORS[log.level] || "#ccc" }}>
                            <span className="log-time">{formatTime()}</span>
                            <span> {log.message}</span>
                        </div>
                    ))
                )}
            </div>
        </div>
    );
}
