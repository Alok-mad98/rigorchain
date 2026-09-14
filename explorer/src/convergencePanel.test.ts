import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  C_ROOT_LABEL,
  DRAW_ROOT_LABEL,
  buildBlockConvergencePanel,
  buildTxConvergencePanel,
} from "./convergencePanel";

describe("buildTxConvergencePanel", () => {
  it("uses inclusion height, not the latest window refresh, for candidates", () => {
    const panel = buildTxConvergencePanel(
      [
        { block_hash: "h174", block_height: 174, is_winner: false },
        { block_hash: "h171", block_height: 171, is_winner: false },
        { block_hash: "h172", block_height: 172, is_winner: false },
      ],
      { block_hash: "h171", block_height: 171 },
    );
    assert.equal(panel.role, "Candidate");
    assert.equal(panel.block_height, 171);
    assert.equal(panel.block_hash, "h171");
    assert.equal(panel.window_last_height, 174);
  });
});

describe("buildBlockConvergencePanel", () => {
  it("keeps C and split draw roots as separate RPC-aligned fields", () => {
    const panel = buildBlockConvergencePanel(
      {
        rigor_winner: "",
        committed_root: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        computed_root: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        draw_candidate_root: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        c_root: "04644ee72744a48d226f795d7cc4d692cbf763975275b7054dec59baedfaa7ba",
      },
      [
        { convergence_txid: "a", draw_eligible: false },
        { convergence_txid: "b", draw_eligible: false },
      ],
    );
    assert.equal(panel.candidate_set_size, 2);
    assert.equal(panel.eligible_count, 0);
    assert.equal(panel.c_root, "04644ee72744a48d226f795d7cc4d692cbf763975275b7054dec59baedfaa7ba");
    assert.equal(panel.convergence_root, panel.c_root);
    assert.equal(
      panel.draw_candidate_root_committed,
      "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
    );
    assert.equal(panel.draw_candidate_root, panel.draw_candidate_root_committed);
    assert.equal(panel.draw_candidate_root_computed, panel.draw_candidate_root_committed);
    assert.notEqual(panel.c_root, panel.draw_candidate_root);
    assert.equal(C_ROOT_LABEL, "C (convergence_root)");
    assert.equal(DRAW_ROOT_LABEL, "draw_candidate_root_committed");
  });

  it("splits A-8 award vs payout roots: committed pays parent, computed is this draw", () => {
    const award = buildBlockConvergencePanel(
      {
        rigor_winner: "a80d8c17",
        committed_root: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        computed_root: "5371455808011a2a80f870425e923d25eded85fb65246af20b06b8ebd2634a39",
        c_root: "c5a813823c00036f76bd1ab005dc1230cec5170315eaee70182598369c7115b0",
      },
      [
        { convergence_txid: "a80d8c17", draw_eligible: true },
        { convergence_txid: "08842485", draw_eligible: false },
      ],
    );
    assert.equal(award.status, "Convergence Winner Block");
    assert.equal(award.eligible_count, 1);
    assert.match(award.draw_candidate_root_committed, /^56944c5d/);
    assert.match(award.draw_candidate_root_computed, /^53714558/);
    assert.notEqual(award.draw_candidate_root_committed, award.draw_candidate_root_computed);

    const payout = buildBlockConvergencePanel(
      {
        rigor_winner: "",
        committed_root: "5371455808011a2a80f870425e923d25eded85fb65246af20b06b8ebd2634a39",
        computed_root: "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d",
        c_root: "",
      },
      [{ convergence_txid: "08842485", draw_eligible: false }],
    );
    assert.equal(payout.draw_candidate_root_committed, award.draw_candidate_root_computed);
    assert.match(payout.draw_candidate_root_computed, /^56944c5d/);
    assert.equal(payout.eligible_count, 0);
  });

  it("treats an RFC-020 same-tx mint as a mint block even when rigor_winner is empty", () => {
    const panel = buildBlockConvergencePanel(
      { rigor_winner: "", rigor_res_minted: 1, mint_txid: "minttx" },
      [{ convergence_txid: "minttx", draw_eligible: true }],
    );
    assert.equal(panel.status, "Convergence Winner Block");
    assert.equal(panel.winner_txid, "minttx");
  });

  it("treats a same-tx RES mint as Winner when is_winner was not set", () => {
    const panel = buildTxConvergencePanel(
      [{ block_hash: "h527", block_height: 527, is_winner: false }],
      { block_hash: "h527", block_height: 527 },
      { isResMint: true },
    );
    assert.equal(panel.role, "Winner");
    assert.equal(panel.block_height, 527);
  });

  it("does not fall back to the draw root when C is missing", () => {
    const panel = buildBlockConvergencePanel(
      { rigor_winner: "", committed_root: "56944c5d" + "aa".repeat(28) },
      [{ convergence_txid: "a", draw_eligible: false }],
    );
    assert.equal(panel.c_root, null);
    assert.equal(panel.convergence_root, null);
    assert.match(panel.draw_candidate_root, /^56944c5d/);
  });
});
