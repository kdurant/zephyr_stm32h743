import { useState, useEffect, useCallback, useRef } from "react";
import type { DeviceInfo, UpgradeStage, OtaEventRaw, OtaLog } from "../types/protocol";

// Lazy-load Tauri APIs to avoid crash during webview initialization
async function getListen() {
    const mod = await import("@tauri-apps/api/event");
    return mod.listen;
}

async function getInvoke() {
    const mod = await import("@tauri-apps/api/core");
    return mod.invoke;
}

export function useOta() {
    const [stage, setStage] = useState<UpgradeStage>("Idle");
    const [progress, setProgress] = useState({ sent: 0, total: 0, percentage: 0 });
    const [message, setMessage] = useState("");
    const [deviceInfo, setDeviceInfo] = useState<DeviceInfo | null>(null);
    const [logs, setLogs] = useState<OtaLog[]>([]);
    const [error, setError] = useState<string | null>(null);
    const logsEndRef = useRef<HTMLDivElement>(null);

    // Listen to ota:event from backend
    useEffect(() => {
        let unlistenFn: (() => void) | null = null;
        let cancelled = false;

        // Async setup — listen() returns a Promise
        (async () => {
            try {
                const listen = await getListen();
                const unlisten = await listen<OtaEventRaw>("ota:event", (event) => {
                    const data = event.payload;

                    switch (data.type) {
                        case "progress":
                            setStage(data.stage || "Idle");
                            setProgress({
                                sent: data.bytes_sent || 0,
                                total: data.total_bytes || 0,
                                percentage: data.percentage || 0,
                            });
                            setMessage(data.message || "");
                            break;

                        case "log":
                            setLogs((prev) => [
                                ...prev.slice(-500), // keep last 500
                                { level: data.level || "info", message: data.message || "" },
                            ]);
                            break;

                        case "error":
                            setError(data.message || "Unknown error");
                            setLogs((prev) => [
                                ...prev.slice(-500),
                                { level: "error", message: data.message || "Unknown error" },
                            ]);
                            break;

                        case "device_info":
                            if (data.info) {
                                setDeviceInfo(data.info);
                            }
                            break;

                        case "stage_changed":
                            setStage(data.stage || "Idle");
                            break;
                    }
                });
                if (cancelled) {
                    unlisten();
                } else {
                    unlistenFn = unlisten;
                }
            } catch (e) {
                console.error("[useOta] Failed to listen to ota:event:", e);
            }
        })();

        return () => {
            cancelled = true;
            if (unlistenFn) {
                unlistenFn();
            }
        };
    }, []);

    // Auto-scroll logs
    useEffect(() => {
        logsEndRef.current?.scrollIntoView({ behavior: "smooth" });
    }, [logs]);

    const addLog = useCallback((level: string, msg: string) => {
        setLogs((prev) => [...prev.slice(-500), { level, message: msg }]);
    }, []);

    const handshake = useCallback(async () => {
        try {
            const invoke = await getInvoke();
            const result = await invoke<string>("handshake");
            addLog("info", `握手: ${result}`);
            return result;
        } catch (e) {
            addLog("error", `握手失败: ${e}`);
            throw e;
        }
    }, [addLog]);

    const getDeviceInfo = useCallback(async () => {
        try {
            addLog("info", "获取设备信息...");
            const invoke = await getInvoke();
            await invoke("get_device_info");
        } catch (e) {
            addLog("error", `获取设备信息失败: ${e}`);
            throw e;
        }
    }, [addLog]);

    const startUpgrade = useCallback(async (firmwarePath: string) => {
        try {
            setError(null);
            addLog("info", `开始升级: ${firmwarePath}`);
            const invoke = await getInvoke();
            await invoke("start_upgrade", { firmwarePath });
        } catch (e) {
            addLog("error", `升级启动失败: ${e}`);
            setError(String(e));
        }
    }, [addLog]);

    const cancelUpgrade = useCallback(async () => {
        try {
            addLog("warn", "取消升级...");
            const invoke = await getInvoke();
            await invoke("cancel_upgrade");
        } catch (e) {
            addLog("error", `取消失败: ${e}`);
        }
    }, [addLog]);

    const verifyFirmware = useCallback(async () => {
        try {
            const invoke = await getInvoke();
            const result = await invoke<string>("verify_firmware");
            addLog("info", `校验: ${result}`);
            return result;
        } catch (e) {
            addLog("error", `校验失败: ${e}`);
            throw e;
        }
    }, [addLog]);

    const resetDevice = useCallback(async (delayMs: number = 100) => {
        try {
            addLog("info", `复位设备 (延迟 ${delayMs}ms)...`);
            const invoke = await getInvoke();
            await invoke("reset_device", { delayMs });
            addLog("info", "复位命令已发送");
        } catch (e) {
            addLog("error", `复位失败: ${e}`);
        }
    }, [addLog]);

    const clearLogs = useCallback(() => setLogs([]), []);

    return {
        stage,
        progress,
        message,
        deviceInfo,
        logs,
        error,
        logsEndRef,
        setError,
        addLog,
        handshake,
        getDeviceInfo,
        startUpgrade,
        cancelUpgrade,
        verifyFirmware,
        resetDevice,
        clearLogs,
    };
}
