import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  CVG2_SCRIPT_LABEL,
  classifyTxKind,
  destinationLabel,
  pickPrimaryOruOutput,
  txKindLabel,
} from "./txSummary";

describe("txSummary", () => {
  it("selects the 65536 ORU payment, not the change remainder", () => {
    const pick = pickPrimaryOruOutput([
      {
        vout_index: 0,
        value_atoms: "6553600000000",
        asset_kind: "ORU",
        address: "rg1qy5cey7tehsdm54agj2ya7dw5rkfddx8fcrme4y",
        witness_version: 0,
      },
      {
        vout_index: 1,
        value_atoms: "45399989371",
        asset_kind: "ORU",
        address: "rg1qchange",
        witness_version: 0,
      },
    ]);
    assert.ok(pick);
    assert.equal(pick.role, "payment");
    assert.equal(String(pick.output.value_atoms), "6553600000000");
    assert.equal(pick.output.address, "rg1qy5cey7tehsdm54agj2ya7dw5rkfddx8fcrme4y");
  });

  it("prefers Witness v1 ORU lock over larger change", () => {
    const pick = pickPrimaryOruOutput([
      {
        vout_index: 0,
        value_atoms: "45299988660",
        asset_kind: "ORU",
        address: "rg1qchange",
        witness_version: 0,
      },
      {
        vout_index: 1,
        value_atoms: "6553600000000",
        asset_kind: "ORU",
        address: "rg1pgdtywvsqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqp5565f",
        witness_version: 1,
      },
    ]);
    assert.ok(pick);
    assert.equal(pick.role, "convergence_lock");
    assert.equal(String(pick.output.value_atoms), "6553600000000");
    assert.equal(classifyTxKind({ isCoinbase: false, primaryRole: pick.role, hasOru: true }), "convergence_lock");
    assert.equal(txKindLabel("convergence_lock"), CVG2_SCRIPT_LABEL);
    assert.equal(
      classifyTxKind({
        isCoinbase: false,
        primaryRole: "payment",
        hasOru: true,
        hasRes: true,
        isResMint: true,
      }),
      "res_mint",
    );
    assert.equal(txKindLabel("res_mint"), "RES mint");
    assert.equal(destinationLabel("convergence_lock"), CVG2_SCRIPT_LABEL);
    assert.equal(destinationLabel("payment"), "Receiver");
  });
});
