import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { formatResEventRow } from "./db";
import { opConvergeScriptSql } from "./circulating";

describe("RES events listing", () => {
  it("maps a body mint to burn + minted result", () => {
    const row = formatResEventRow({
      convergence_txid: "aa".repeat(32),
      included_height: "501",
      lock_atoms: "102400000000",
      mint_amount_atoms: "100000000",
      mint_height: "501",
    });
    assert.equal(row.height, "501");
    assert.equal(row.lock_display, "1024.00000000 ORU");
    assert.equal(row.mint_amount, "1.00000000 RES");
    assert.match(String(row.result), /Minted 1\.00000000 RES/);
  });

  it("maps a plain OP_CONVERGE as Destroy only", () => {
    const row = formatResEventRow({
      convergence_txid: "bb".repeat(32),
      included_height: "10",
      lock_atoms: "500000000",
      mint_amount_atoms: "0",
    });
    assert.equal(row.result, "Destroy only");
    assert.equal(row.mint_amount, "0");
    assert.equal(row.lock_display, "5.00000000 ORU");
  });

  it("matches live OP_CONVERGE scripts in SQL, not retired CVG2", () => {
    assert.match(opConvergeScriptSql("o.script_pubkey"), /o\.script_pubkey/);
    assert.match(opConvergeScriptSql(), /14\[0-9a-f\]\{40\}bd/);
    assert.doesNotMatch(opConvergeScriptSql(), /43564732/);
  });
});
