import { defineConfig } from "vite";

// Tauri serves the dev build from this fixed port (src-tauri/tauri.conf.json devUrl). The same
// page opened in Chrome / Edge uses WebHID instead of the Rust side (src/transport.ts).
export default defineConfig({
  clearScreen: false,
  server: { port: 1420, strictPort: true },
  build: { target: "safari16", outDir: "dist" },
});
