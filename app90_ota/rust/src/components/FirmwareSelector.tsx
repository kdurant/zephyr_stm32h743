import { useState, useCallback } from "react";

interface Props {
    onFirmwareSelected: (path: string, name: string) => void;
    firmwarePath: string;
    firmwareName: string;
}

export function FirmwareSelector({ onFirmwareSelected, firmwarePath, firmwareName }: Props) {
    const [error, setError] = useState<string | null>(null);

    const selectFile = useCallback(async () => {
        try {
            setError(null);
            const { open } = await import("@tauri-apps/plugin-dialog");
            const selected = await open({
                multiple: false,
                filters: [
                    { name: "固件文件", extensions: ["bin", "BIN"] },
                    { name: "所有文件", extensions: ["*"] },
                ],
            });
            if (selected) {
                const path = typeof selected === "string" ? selected : selected;
                const name = path.split(/[/\\]/).pop() || path;
                onFirmwareSelected(path, name);
            }
        } catch (e) {
            setError(String(e));
        }
    }, [onFirmwareSelected]);

    return (
        <div className="panel">
            <h3>固件选择</h3>
            <div className="form-row">
                <button className="btn btn-connect" onClick={selectFile}>选择固件</button>
            </div>
            {firmwareName && (
                <div className="firmware-info">
                    <p className="filename">{firmwareName}</p>
                    <p className="muted" style={{ fontSize: "0.75em" }}
                        title={firmwarePath}>
                        {firmwarePath.length > 50
                            ? firmwarePath.slice(0, 25) + "..." + firmwarePath.slice(-22)
                            : firmwarePath}
                    </p>
                </div>
            )}
            {error && <p className="error-text">{error}</p>}
        </div>
    );
}
