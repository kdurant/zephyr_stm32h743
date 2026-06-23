import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

const host = process.env.TAURI_DEV_HOST;

export default defineConfig(async () => ({
    plugins: [react()],
    // Use relative paths only for production builds (Tauri custom protocol).
    // In dev mode, Vite serves from root, so "/" is correct.
    base: process.env.TAURI_ENV_PLATFORM ? "./" : "/",
    clearScreen: false,
    server: {
        port: 1420,
        strictPort: true,
        host: host || "127.0.0.1",
        hmr: host
            ? { protocol: "ws", host, port: 1421 }
            : undefined,
        watch: {
            ignored: ["**/src-tauri/**"],
        },
    },
}));
