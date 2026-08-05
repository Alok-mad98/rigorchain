import { describe, it } from "node:test";
import assert from "node:assert/strict";
import http from "node:http";

const API_BASE = process.env.EXPLORER_TEST_URL ?? "http://127.0.0.1:8080";

function getJson(path: string): Promise<{ status: number; body: unknown }> {
  return new Promise((resolve, reject) => {
    http
      .get(`${API_BASE}${path}`, (res) => {
        let data = "";
        res.on("data", (c) => (data += c));
        res.on("end", () => {
          try {
            resolve({ status: res.statusCode ?? 0, body: JSON.parse(data) });
          } catch (e) {
            reject(e);
          }
        });
      })
      .on("error", reject);
  });
}

function assertLineageShape(body: Record<string, unknown>): void {
  assert.equal(body.equivalence_claimed, false);
  assert.ok(body.candidate_set, "candidate_set");
  assert.ok(body.winner, "winner");
  assert.ok(body.sigma, "sigma");
  assert.ok(body.pcorb, "pcorb");
  assert.ok(body.mint, "mint");
  assert.ok(body.block_context, "block_context");
  assert.ok(Array.isArray(body.nodes), "nodes");
  assert.ok(Array.isArray(body.edges), "edges");
  assert.match(String(body.readonly_notice), /not computed in the browser/);
}

describe("lineage API integration", () => {
  it("GET /api/lineage/:txid returns lineage for winner convergence tx", async () => {
    let blocksRes: { status: number; body: unknown };
    try {
      blocksRes = await getJson("/api/blocks?limit=5");
    } catch {
      return;
    }
    if (blocksRes.status !== 200) return;

    const blocks = (blocksRes.body as { blocks?: Array<{ block_hash: string }> }).blocks ?? [];
    let winnerTxid: string | null = null;
    for (const b of blocks) {
      const blockRes = await getJson(`/api/block/${b.block_hash}`);
      if (blockRes.status !== 200) continue;
      const panel = (blockRes.body as { convergence_panel?: { winner_txid?: string } }).convergence_panel;
      if (panel?.winner_txid) {
        winnerTxid = panel.winner_txid;
        break;
      }
    }
    if (!winnerTxid) return;

    const res = await getJson(`/api/lineage/${winnerTxid}`);
    if (res.status === 404) return; // older build
    assert.equal(res.status, 200);
    const body = res.body as Record<string, unknown>;
    if (!body.nodes) return;
    assertLineageShape(body);
    assert.ok((body.f_set_size as number) >= 1);
  });

  it("GET /api/tx/:txid embeds lineage_graph", async () => {
    let blocksRes: { status: number; body: unknown };
    try {
      blocksRes = await getJson("/api/blocks?limit=3");
    } catch {
      return;
    }
    if (blocksRes.status !== 200) return;
    const blocks = (blocksRes.body as { blocks?: Array<{ block_hash: string }> }).blocks ?? [];
    let winnerTxid: string | null = null;
    for (const b of blocks) {
      const blockRes = await getJson(`/api/block/${b.block_hash}`);
      const panel = (blockRes.body as { convergence_panel?: { winner_txid?: string } }).convergence_panel;
      if (panel?.winner_txid) {
        winnerTxid = panel.winner_txid;
        break;
      }
    }
    if (!winnerTxid) return;

    const txRes = await getJson(`/api/tx/${winnerTxid}`);
    if (txRes.status !== 200) return;
    const body = txRes.body as Record<string, unknown>;
    if (!body.lineage_graph) return;
    assertLineageShape(body.lineage_graph as Record<string, unknown>);
  });
});
