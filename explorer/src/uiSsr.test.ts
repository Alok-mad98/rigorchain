import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { injectSsrRoot, renderBlockRoot, renderStatusRoot, renderTxRoot } from "./uiSsr";
import { CVG2_SCRIPT_LABEL } from "./txSummary";

describe("uiSsr", () => {
  it("replaces the Loading placeholder with rendered block amounts", () => {
    const template = `<div id="root">Loading...</div>`;
    const html = injectSsrRoot(
      template,
      renderBlockRoot(
        {
          block: { height: "168", block_hash: "aa" },
          summary: { height: "168", hash: "aa", oru_subsidy_display: "1000.00010391" },
          convergence_panel: { status: "No Convergence Activity", candidate_set_size: 0, candidate_txids: [] },
          txs: [
            {
              txid: "bcd9ed2ed8dadd6ef93f631fb8cc8d746e4c5df910799d5e7feac645d91c5536",
              tx_kind_label: "ORU transfer",
              amount_display: "65536.00000000",
              asset_kind: "ORU",
            },
          ],
        },
        "aa",
      ),
    );
    assert.match(html, /data-ssr="1"/);
    assert.match(html, /65536\.00000000/);
    assert.doesNotMatch(html, />Loading\.\.\.</);
  });

  it("renders C and split draw_candidate_root committed/computed labels", () => {
    const html = renderBlockRoot(
      {
        block: { height: "305", block_hash: "d8" },
        summary: { height: "305", hash: "d8" },
        convergence_panel: {
          status: "Convergence Winner Block",
          candidate_set_size: 2,
          eligible_count: 1,
          c_root: "04644ee72744a48d226f795d7cc4d692cbf763975275b7054dec59baedfaa7ba",
          draw_candidate_root_committed: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
          draw_candidate_root_computed: "5371455808011a2a80f870425e923d25eded85fb65246af20b06b8ebd2634a39",
          winner_txid: "a80d8c17",
          candidate_txids: ["aa", "bb"],
        },
        txs: [],
      },
      "d8",
    );
    assert.match(html, /C \(convergence_root\)/);
    assert.match(html, /draw_candidate_root_committed/);
    assert.match(html, /draw_candidate_root_computed/);
    assert.match(html, /04644ee72744a48d/);
    assert.match(html, /56944c5d3f98413e/);
    assert.match(html, /5371455808011a2a/);
    assert.match(html, /pays parent draw/);
    assert.match(html, /this block awards · eligible=1/);
    assert.match(html, /empty eligible set/);
  });

  it("renders Draw Identity with Not available when fingerprint is missing", () => {
    const html = renderBlockRoot(
      {
        block: { height: "0", block_hash: "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758" },
        summary: {
          height: "0",
          hash: "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758",
          consensus_fingerprint_display: "Not available",
          consensus_fingerprint_status: "Not available",
        },
        txs: [],
      },
      "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758",
    );
    assert.match(html, /Draw Identity/);
    assert.match(html, /Draw Fingerprint:/);
    assert.match(html, /Not available/);
    assert.match(html, /1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758/);
  });

  it("shows A-8 RES award note with payout-block link", () => {
    const html = renderBlockRoot(
      {
        block: { height: "305", block_hash: "award" },
        summary: {
          height: "305",
          hash: "award",
          res_minted_display: "0",
          res_award_note: "100.00 RES paid in block 306",
          res_payout_block_hash: "payout306",
        },
        txs: [],
      },
      "award",
    );
    assert.match(html, /RES award/);
    assert.match(html, /100\.00 RES paid in block 306/);
    assert.match(html, /href="\/block\/payout306"/);
  });

  it("labels a CVG2 output as an unspendable script, not a receiver", () => {
    const html = renderTxRoot(
      {
        summary: {
          sender: "rg1qk4v6avkzfu3rmzaph2jrhep8w4vvwj9xjrlpn9",
          receiver: "rg1pgdtywvsqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp5565f",
          amount_role: "convergence_lock",
          amount_display: "1000.00000000",
        },
        vout: [
          {
            vout_index: 0,
            asset_kind: "ORU",
            value_atoms: "100000000000",
            witness_version: 1,
            address: "rg1pgdtywvsqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp5565f",
          },
        ],
      },
      "6383575272fbc1a5d78ae2ea8ece4ce714e1aa3ee692f441b2a53d83e9140f2d",
    );
    assert.match(html, new RegExp(CVG2_SCRIPT_LABEL.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")));
    assert.doesNotMatch(html, /<th>Receiver<\/th>/);
    assert.match(html, /<th>Amount<\/th>/);
    assert.match(html, /Draw Fingerprint/);
    assert.doesNotMatch(html, /Chain Fingerprint/);
  });

  it("keeps homepage S3 as a pulse and sends draw-root detail to RES amount", () => {
    const html = renderStatusRoot({
      chain_identity: { product: "AlphaPlus" },
      block_health: {},
      convergence_health: {
        winner_blocks_last_100: 2,
        winner_frequency_last_100: 0.02,
        active_convergences: 1,
        pending_windows: 3,
        c_root: "04644ee72744a48d226f795d7cc4d692cbf763975275b7054dec59baedfaa7ba",
        draw_candidate_root_committed: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        draw_candidate_root_computed: "5371455808011a2a80f870425e923d25eded85fb65246af20b06b8ebd2634a39",
        f_set_distribution_last_100_blocks: { "1": 12, "2": 3 },
      },
      economic_snapshot: { network_res_minted_display: "200.00 RES" },
      mining_activity: {},
      system_integrity: { indexer_sync_lag_blocks: 0, explorer_compliance: { mismatch_active: false } },
    });
    assert.match(html, /S3 — Convergence &amp; integrity/);
    assert.match(html, /Winner frequency/);
    assert.match(html, /href="\/res\.html"/);
    assert.doesNotMatch(html, /draw_candidate_root_committed/);
    assert.doesNotMatch(html, /draw_candidate_root_computed/);
    assert.doesNotMatch(html, /\|F\| distribution/);
    assert.doesNotMatch(html, /04644ee72744a48d/);
  });
});
