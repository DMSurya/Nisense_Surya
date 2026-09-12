import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Dev server proxies /api and /auth to the FastAPI/Keycloak stack so the SPA
// runs against the same origin (matches the Caddy reverse proxy in prod).
export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      "/api": { target: "http://localhost:8000", changeOrigin: true },
      // Keycloak with KC_HTTP_RELATIVE_PATH=/auth — forward path intact
      "/auth": { target: "http://localhost:8080", changeOrigin: true },
    },
  },
  build: { outDir: "dist", sourcemap: true },
});
