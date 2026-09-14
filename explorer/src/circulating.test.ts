import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { circulatingOruAtoms, isCirculatingSpendableOruRow, matureSpendableOruSql, plainDestroyOruSql } from "./circulating";
import { isConvergenceScriptPubKey, isRetiredCvg2WitnessV1 } from "./script";

const SPENDABLE_V0 = "001409ba7bdc9a1c2f6114431e483f26c1de0271893c";
const OP_CONVERGE_HEX = "14" + "00".repeat(20) + "bd";
/** Historical RFC-014 / retired Witness v1 CVG2. */
const RETIRED_CVG2_V1 =
  "51204356473200000000000000000000000000000000000000000000000000000000";

describe("circulating ORU excludes OP_CONVERGE and spent rows", () => {
  it("detects OP_CONVERGE scriptPubKey", () => {
    assert.equal(isConvergenceScriptPubKey(OP_CONVERGE_HEX), true);
    assert.equal(isConvergenceScriptPubKey(SPENDABLE_V0), false);
    assert.equal(isRetiredCvg2WitnessV1(RETIRED_CVG2_V1), true);
    assert.equal(isConvergenceScriptPubKey(RETIRED_CVG2_V1), false);
  });

  it("does not count OP_CONVERGE ORU as circulating", () => {
    const rows = [
      { spent: false, script_pubkey: SPENDABLE_V0, amount_atoms: 100000000 },
      { spent: false, script_pubkey: OP_CONVERGE_HEX, amount_atoms: 500000000 },
      { spent: true, script_pubkey: SPENDABLE_V0, amount_atoms: 200000000 },
    ];
    assert.equal(isCirculatingSpendableOruRow(rows[1]), false);
    assert.equal(circulatingOruAtoms(rows), 100000000n);
  });

  it("does not count historical RFC-014 CVG2 rows as circulating", () => {
    const rows = [
      { spent: false, script_pubkey: SPENDABLE_V0, amount_atoms: 100000000 },
      { spent: false, script_pubkey: RETIRED_CVG2_V1, amount_atoms: 500000000 },
    ];
    assert.equal(isCirculatingSpendableOruRow(rows[1]), false);
    assert.equal(circulatingOruAtoms(rows), 100000000n);
  });
});

describe("plain destroy SQL", () => {
  it("selects OP_CONVERGE ORU with no RES mint", () => {
    const sql = plainDestroyOruSql("o");
    assert.match(sql, /o\.asset_kind = 'ORU'/);
    assert.match(sql, /res_mints/);
    assert.match(sql, /asset_kind = 'RES'/);
    assert.match(sql, /14\[0-9a-f\]\{40\}bd/);
  });
});

describe("mature spendable SQL", () => {
  it("requires coinbase maturity against indexer tip", () => {
    const sql = matureSpendableOruSql();
    assert.match(sql, /is_coinbase/);
    assert.match(sql, />= 100/);
    assert.match(sql, /spent = FALSE/);
  });
});
