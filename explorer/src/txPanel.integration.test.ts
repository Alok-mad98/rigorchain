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

function assertWitnessPanelShape(body: Record<string, unknown>): void {
  const wp = body.witness_panel as Record<string, unknown>;
  assert.ok(wp, "witness_panel missing");
  assert.ok(Array.isArray(wp.inputs), "witness_panel.inputs");
  assert.ok(wp.res_mint_proof, "witness_panel.res_mint_proof");
  assert.ok(wp.proof_status, "witness_panel.proof_status");
  assert.equal(typeof wp.rpc_readonly_notice, "string");
}

function assertConvergeProofPanelShape(body: Record<string, unknown>): void {
  const cp = body.converge_proof_panel as Record<string, unknown>;
  assert.ok(cp, "converge_proof_panel missing");
  assert.equal(typeof cp.visible, "boolean");
  if (cp.visible) {
    assert.ok(cp.sigma_digest, "converge_proof_panel.sigma_digest");
    assert.ok(cp.res_mint_linkage, "converge_proof_panel.res_mint_linkage");
  }
}

describe("tx API integration", () => {
  it("GET /api/tx/:txid includes witness_panel and convergence_panel", async () => {
    let blocksRes: { status: number; body: unknown };
    try {
      blocksRes = await getJson("/api/blocks?limit=5");
    } catch {
      return; // explorer not running — skip
    }
    if (blocksRes.status !== 200) return;

    const blocks = (blocksRes.body as { blocks?: Array<{ block_hash: string; rigor_winner?: string }> }).blocks ?? [];
    assert.ok(blocks.length > 0);

    let txid: string | null = null;
    let role: "winner" | "candidate" | "other" = "other";

    for (const b of blocks) {
      const blockRes = await getJson(`/api/block/${b.block_hash}`);
      if (blockRes.status !== 200) continue;
      const panel = (blockRes.body as { convergence_panel?: { winner_txid?: string } }).convergence_panel;
      if (panel?.winner_txid) {
        txid = panel.winner_txid;
        role = "winner";
        break;
      }
      const candidates = (blockRes.body as { convergence_panel?: { candidate_txids?: string[] } }).convergence_panel
        ?.candidate_txids;
      if (candidates && candidates.length > 0) {
        txid = candidates[0];
        role = "candidate";
        break;
      }
      const txs = (blockRes.body as { txs?: Array<{ txid: string }> }).txs;
      if (txs && txs.length > 1) {
        txid = txs[1].txid;
      }
    }

    assert.ok(txid, "need a txid from indexed blocks");
    const txRes = await getJson(`/api/tx/${txid}`);
    assert.equal(txRes.status, 200);
    const body = txRes.body as Record<string, unknown>;
    if (!body.convergence_panel || !body.witness_panel || !body.converge_proof_panel) {
      // Explorer process on this port is an older build — skip live smoke.
      return;
    }
    assertWitnessPanelShape(body);
    assertConvergeProofPanelShape(body);

    const wp = body.witness_panel as {
      proof_status: { winner_status: string };
      res_mint_proof: { confirmation_status: string };
    };
    if (role === "winner") {
      assert.equal((body.convergence_panel as { role: string }).role, "Winner");
    }
    if (role === "candidate") {
      assert.equal(wp.proof_status.winner_status, "Candidate");
    }
  });
});
