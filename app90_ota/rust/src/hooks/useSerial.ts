import { useState, useCallback, useEffect } from "react";

// Lazy-load Tauri API to avoid crash if not available yet
async function getInvoke() {
    const mod = await import("@tauri-apps/api/core");
    return mod.invoke;
}

export function useSerial() {
    const [ports, setPorts] = useState<string[]>([]);
    const [connected, setConnected] = useState(false);
    const [portName, setPortName] = useState("");
    const [baudRate, setBaudRate] = useState(115200);
    const [error, setError] = useState<string | null>(null);

    const refreshPorts = useCallback(async () => {
        try {
            const invoke = await getInvoke();
            const list = await invoke<string[]>("list_serial_ports");
            setPorts(list);
            setError(null);
        } catch (e) {
            setError(String(e));
        }
    }, []);

    // Auto-refresh on mount
    useEffect(() => {
        refreshPorts();
    }, [refreshPorts]);

    const connect = useCallback(async () => {
        if (!portName) {
            setError("请选择串口");
            return;
        }
        try {
            setError(null);
            const invoke = await getInvoke();
            await invoke("connect_serial", { portName, baudRate });
            setConnected(true);
        } catch (e) {
            setError(String(e));
            setConnected(false);
        }
    }, [portName, baudRate]);

    const disconnect = useCallback(async () => {
        try {
            setError(null);
            const invoke = await getInvoke();
            await invoke("disconnect_serial");
            setConnected(false);
        } catch (e) {
            setError(String(e));
        }
    }, []);

    return {
        ports,
        connected,
        portName,
        setPortName,
        baudRate,
        setBaudRate,
        error,
        setError,
        refreshPorts,
        connect,
        disconnect,
    };
}
