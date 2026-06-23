import { useState, useCallback } from "react";
import { SerialPanel } from "./components/SerialPanel";
import { DeviceInfo } from "./components/DeviceInfo";
import { FirmwareSelector } from "./components/FirmwareSelector";
import { OtaProgress } from "./components/OtaProgress";
import { OtaControls } from "./components/OtaControls";
import { LogConsole } from "./components/LogConsole";
import { useSerial } from "./hooks/useSerial";
import { useOta } from "./hooks/useOta";
import "./App.css";

function App() {
    const serial = useSerial();
    const ota = useOta();

    const [firmwarePath, setFirmwarePath] = useState("");
    const [firmwareName, setFirmwareName] = useState("");

    const handleFirmwareSelected = useCallback((path: string, name: string) => {
        setFirmwarePath(path);
        setFirmwareName(name);
        ota.addLog("info", `已选择固件: ${name}`);
    }, [ota]);

    const handleStartUpgrade = useCallback(async () => {
        if (!firmwarePath) {
            ota.addLog("error", "请先选择固件文件");
            return;
        }
        try {
            await ota.startUpgrade(firmwarePath);
        } catch {
            // error handled in hook
        }
    }, [firmwarePath, ota]);

    const handleHandshake = useCallback(async () => {
        try {
            await ota.handshake();
            // After handshake, get device info
            await ota.getDeviceInfo();
        } catch {
            // error handled in hook
        }
    }, [ota]);

    return (
        <div className="app-container">
            <header className="app-header">
                <h1>OTA 固件升级工具</h1>
                <span className={`status-badge ${serial.connected ? "connected" : ""}`}>
                    {serial.connected ? "已连接" : "未连接"}
                </span>
            </header>

            <main className="app-main">
                <aside className="sidebar">
                    <SerialPanel
                        ports={serial.ports}
                        portName={serial.portName}
                        baudRate={serial.baudRate}
                        connected={serial.connected}
                        error={serial.error}
                        onPortNameChange={serial.setPortName}
                        onBaudRateChange={serial.setBaudRate}
                        onRefresh={serial.refreshPorts}
                        onConnect={serial.connect}
                        onDisconnect={serial.disconnect}
                    />
                    <FirmwareSelector
                        onFirmwareSelected={handleFirmwareSelected}
                        firmwarePath={firmwarePath}
                        firmwareName={firmwareName}
                    />
                    <OtaControls
                        stage={ota.stage}
                        connected={serial.connected}
                        hasFirmware={!!firmwarePath}
                        onHandshake={handleHandshake}
                        onGetInfo={ota.getDeviceInfo}
                        onStartUpgrade={handleStartUpgrade}
                        onCancel={ota.cancelUpgrade}
                        onVerify={ota.verifyFirmware}
                        onReset={() => ota.resetDevice(100)}
                    />
                </aside>

                <section className="content">
                    <DeviceInfo info={ota.deviceInfo} />
                    <OtaProgress
                        stage={ota.stage}
                        percentage={ota.progress.percentage}
                        message={ota.message}
                        sent={ota.progress.sent}
                        total={ota.progress.total}
                        connected={serial.connected}
                    />
                    <LogConsole logs={ota.logs} onClear={ota.clearLogs} />
                </section>
            </main>
        </div>
    );
}

export default App;
