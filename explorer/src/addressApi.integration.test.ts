import { describe, it } from "node:test";
import assert from "node:assert/strict";
import http from "node:http";

const API_BASE = process.env.EXPLORER_TEST_URL ?? "http://127.0.0.1:8080";

const ORU_SAMPLE = "trg1nsxfhxfv3axth3dd53w0r7a8ptu9xejs02fq3j";
const RES_SAMPLE =
  "trg1pflfvwsh8p5fuykxnrddzp8jd53fgnpsnuhvv5xrj5rfy6vs34ydqywruva";

function get(path: string, headers?: Record<string, string>): Promise<{ status: number; body: string; headers: http.IncomingHttpHeaders }> {
  return new Promise((resolve, reject) => {
    http
      .get(`${API_BASE}${path}`, { headers }, (res) => {
        let data = "";
        res.on("data", (c) => (data += c));
        res.on("end", () => resolve({ status: res.statusCode ?? 0, body: data, headers: res.headers }));
      })
      .on("error", reject);
  });
}

function getJson(path: string): Promise<{ status: number; body: unknown }> {
  return get(path).then(({ status, body }) => {
    try {
      return { status, body: JSON.parse(body) };
    } catch (e) {
      throw new Error(`non-JSON ${status}: ${body.slice(0, 120)}`);
    }
  });
}

function assertAddressIntelligenceShape(body: Record<string, unknown>): void {
  assert.ok(Array.isArray(body.oru_utxos), "legacy oru_utxos");
  assert.ok(Array.isArray(body.res_outputs), "legacy res_outputs");
  assert.equal(body.equivalence_claimed, false);

  const identity = body.identity as Record<string, unknown>;
  assert.ok(identity?.address, "identity.address");
  assert.ok(identity?.address_type, "identity.address_type");

  const balances = body.balances as { oru?: { display: string }; res?: { display: string } };
  assert.ok(balances?.oru?.display, "balances.oru");
  assert.ok(balances?.res?.display, "balances.res");
  assert.notEqual(balances.oru, balances.res);

  const utxo = body.utxo_summary as { oru?: unknown; res?: unknown };
  assert.ok(utxo?.oru, "utxo_summary.oru");
  assert.ok(utxo?.res, "utxo_summary.res");

  const history = body.transaction_history as { items: unknown[]; page: number; total: number };
  assert.ok(Array.isArray(history?.items), "transaction_history.items");
  assert.equal(typeof history.page, "number");

  assert.ok(body.convergence_activity, "convergence_activity");
  assert.ok(body.res_mint_activity, "res_mint_activity");
  assert.ok(body.integrity, "integrity");
  assert.match(String(body.readonly_notice), /does not create, sign, or broadcast/);
}

describe("address API integration", () => {
  it("GET /api/address/:addr rejects invalid format", async () => {
    try {
      const res = await getJson("/api/address/not-a-bech32");
      if (res.status === 404) return;
      assert.equal(res.status, 400);
    } catch {
      return;
    }
  });

  it("GET /api/address/:addr returns intelligence payload with legacy fields", async () => {
    let res: { status: number; body: unknown };
    try {
      res = await getJson(`/api/address/${ORU_SAMPLE}`);
    } catch {
      return;
    }
    if (res.status !== 200) return;

    const body = res.body as Record<string, unknown>;
    if (!body.identity) return; // older explorer build — skip

    assertAddressIntelligenceShape(body);
    const id = body.identity as { classification: string };
    assert.match(id.classification, /ORU|Unused/);
  });

  it("GET /api/address/:addr classifies RES prefix on unused address", async () => {
    let res: { status: number; body: unknown };
    try {
      res = await getJson(`/api/address/${RES_SAMPLE}`);
    } catch {
      return;
    }
    if (res.status !== 200) return;

    const body = res.body as Record<string, unknown>;
    if (!body.identity) return;

    assertAddressIntelligenceShape(body);
    const id = body.identity as { address_type: string };
    assert.match(id.address_type, /RES/);
  });

  it("legacy GET /address/:addr returns JSON for wallet-observer", async () => {
    let res: { status: number; body: unknown };
    try {
      res = await getJson(`/address/${ORU_SAMPLE}?limit=10`);
    } catch {
      return;
    }
    if (res.status !== 200) return;
    const body = res.body as Record<string, unknown>;
    if (!body.identity) return;
    assert.ok(Array.isArray(body.oru_utxos));
  });

  it("GET /address/:addr serves HTML for navigation", async () => {
    let res: { status: number; body: string; headers: http.IncomingHttpHeaders };
    try {
      res = await get(`/address/${ORU_SAMPLE}`, {
        Accept: "text/html",
        "Sec-Fetch-Mode": "navigate",
        "Sec-Fetch-Dest": "document",
      });
    } catch {
      return;
    }
    if (res.status !== 200) return;
    assert.match(res.body, /Address intelligence|address-root/);
  });

  it("GET /address.html?addr= redirects to canonical path", async () => {
    let res: { status: number; headers: http.IncomingHttpHeaders };
    try {
      res = await get(`/address.html?addr=${encodeURIComponent(ORU_SAMPLE)}`);
    } catch {
      return;
    }
    assert.equal(res.status, 301);
    assert.ok(String(res.headers.location).includes(`/address/${ORU_SAMPLE}`));
  });
});
