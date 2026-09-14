import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

const rpcTarget = process.env.RIGOR_RPC_URL ?? "http://127.0.0.1:28761";
const rpcUser = process.env.RIGOR_RPC_USER ?? "rigor";
const rpcPassword = process.env.RIGOR_RPC_PASSWORD ?? "rigor-local-test";
const rpcAuth = Buffer.from(`${rpcUser}:${rpcPassword}`).toString("base64");

export default defineConfig({
  plugins: [react()],
  server: {
    host: "127.0.0.1",
    port: 5173,
    strictPort: true,
    proxy: {
      "/rpc": {
        target: rpcTarget,
        changeOrigin: true,
        rewrite: () => "/",
        configure: (proxy) => {
          proxy.on("proxyReq", (proxyReq) => {
            proxyReq.setHeader("Authorization", `Basic ${rpcAuth}`);
          });
        },
      },
    },
  },
  preview: {
    host: "127.0.0.1",
    port: 4173,
    strictPort: true,
  },
  build: {
    outDir: "dist",
    sourcemap: true,
  },
});
