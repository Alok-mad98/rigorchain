import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  CONVERGENCE_LOCK_DESTINATION_LABEL,
  classifyTxKind,
  convergeOutputNote,
  destinationLabel,
  pickPrimaryOruOutput,
  txKindLabel,
  txKindNotice,
} from "./txSummary";

const OP_CONVERGE_HEX = "14" + "00".repeat(20) + "bd";

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

  it("prefers OP_CONVERGE burn over larger change", () => {
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
        address: null,
        witness_version: null,
        script_pubkey: OP_CONVERGE_HEX,
      },
    ]);
    assert.ok(pick);
    assert.equal(pick.role, "convergence_lock");
    assert.equal(String(pick.output.value_atoms), "6553600000000");
    assert.equal(classifyTxKind({ isCoinbase: false, primaryRole: pick.role, hasOru: true }), "oru_destroy");
    assert.equal(txKindLabel("oru_destroy"), "ORU destroyed");
    assert.equal(txKindNotice("oru_destroy"), "ORU destroyed. No RES minted.");
    assert.equal(convergeOutputNote("oru_destroy"), "OP_CONVERGE · ORU destroyed · no RES minted");
    assert.equal(txKindLabel("convergence_lock"), CONVERGENCE_LOCK_DESTINATION_LABEL);
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
    assert.equal(txKindNotice("res_mint"), "ORU destroyed. RES minted.");
    assert.equal(
      classifyTxKind({
        isCoinbase: true,
        primaryRole: "output",
        hasOru: true,
        hasRes: true,
        isResMint: true,
      }),
      "res_mint",
    );
    assert.equal(
      classifyTxKind({
        isCoinbase: false,
        primaryRole: null,
        hasOru: false,
        hasRes: true,
        isResMint: false,
      }),
      "other",
    );
    assert.equal(destinationLabel("convergence_lock"), CONVERGENCE_LOCK_DESTINATION_LABEL);
    assert.equal(destinationLabel("payment"), "Receiver");
  });

  it("does not treat Witness v1 RES-shaped rows as Convergence", () => {
    const pick = pickPrimaryOruOutput([
      {
        vout_index: 0,
        value_atoms: "100000000",
        asset_kind: "ORU",
        address: "rg1qchange",
        witness_version: 0,
      },
      {
        vout_index: 1,
        value_atoms: "200000000",
        asset_kind: "ORU",
        address: "rg1pnotconverge",
        witness_version: 1,
      },
    ]);
    assert.ok(pick);
    assert.equal(pick.role, "payment");
  });
});
