import { describe, it, beforeEach, afterEach } from "node:test";
import assert from "node:assert/strict";
import express from "express";
import http from "node:http";
import type { AddressInfo } from "node:net";
import type { Config } from "./config";
import { createRateLimitMiddleware, resetRateLimitBucketsForTests } from "./rateLimit";

function testConfig(overrides: Partial<Config> = {}): Config {
  return {
    network: "testnet",
    rigorDatadir: "/tmp",
    rigorCliBin: "rigor-cli",
    pollIntervalMs: 30_000,
    apiPort: 0,
    pgUrl: "postgres://localhost/test",
    schemaMigrationsPath: "/tmp",
    indexerName: "test",
    importBatchMaxHeight: 20,
    reorgSafetyWindow: 10,
    maxSupportedReorgDepth: 200,
    stopOnRootMismatch: false,
    mismatchRetryIntervalMs: 60_000,
    rateLimitEnabled: true,
    rateLimitWindowMs: 60_000,
    rateLimitGlobalMax: 120,
    rateLimitRoutes: { "/api/tx/:txid": 2 },
    ...overrides,
  };
}

function requestJson(
  server: http.Server,
  path: string,
  headers: Record<string, string> = {},
): Promise<{ status: number; body: any; headers: http.IncomingHttpHeaders }> {
  const addr = server.address() as AddressInfo;
  return new Promise((resolve, reject) => {
    const req = http.request(
      {
        host: "127.0.0.1",
        port: addr.port,
        path,
        method: "GET",
        headers,
      },
      (res) => {
        const chunks: Buffer[] = [];
        res.on("data", (c) => chunks.push(c));
        res.on("end", () => {
          const text = Buffer.concat(chunks).toString("utf8");
          let body: unknown = text;
          try {
            body = JSON.parse(text);
          } catch {
            /* plain text */
          }
          resolve({ status: res.statusCode ?? 0, body, headers: res.headers });
        });
      },
    );
    req.on("error", reject);
    req.end();
  });
}

describe("rateLimit API integration", () => {
  let server: http.Server;

  beforeEach(() => {
    resetRateLimitBucketsForTests();
  });

  afterEach(async () => {
    if (server) await new Promise<void>((resolve) => server.close(() => resolve()));
  });

  it("returns 429 with Retry-After when per-route cap exceeded", async () => {
    const app = express();
    app.use(createRateLimitMiddleware(testConfig()));
    app.get("/api/tx/:txid", (_req, res) => res.json({ ok: true }));

    server = await new Promise<http.Server>((resolve) => {
      const s = app.listen(0, "127.0.0.1", () => resolve(s));
    });

    const ip = { "X-Forwarded-For": "203.0.113.10" };
    assert.equal((await requestJson(server, "/api/tx/abc", ip)).status, 200);
    assert.equal((await requestJson(server, "/api/tx/def", ip)).status, 200);
    const blocked = await requestJson(server, "/api/tx/ghi", ip);
    assert.equal(blocked.status, 429);
    assert.equal(blocked.body.error, "rate limit exceeded");
    assert.ok(Number(blocked.headers["retry-after"]) >= 1);
  });
});
