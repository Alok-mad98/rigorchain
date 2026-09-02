import { describe, it } from "node:test";
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import {
  classifyScriptPubKey,
  CONVERGENCE_OUTPUT_STATE,
  isConvergenceScriptPubKey,
  isResMintScriptPubKey,
  isOpConvergeScriptPubKey,
  isRetiredCvg2WitnessV1,
  OP_CONVERGE_SCRIPT_NAME,
} from "./script";

/** OP_PUSHBYTES_20 || 20 zero bytes || OP_CONVERGE (0xbd). */
const OP_CONVERGE_HEX = "14" + "00".repeat(20) + "bd";
/** Historical RFC-014 / retired Witness v1 CVG2. */
const RETIRED_CVG2_V1 =
  "51204356473200000000000000000000000000000000000000000000000000000000";
const P2WPKH = "001409ba7bdc9a1c2f6114431e483f26c1de0271893c";
const RES_NATIVE_V1 = "5120" + "11".repeat(32);

describe("scriptPubKey address encoding", () => {
  it("encodes AlphaPlus mainnet P2WPKH as rg1q…", () => {
    const hex = P2WPKH;
    const cls = classifyScriptPubKey(hex, "rg");
    assert.equal(cls.assetKind, "ORU");
    assert.equal(cls.witnessVersion, 0);
    assert.equal(cls.isConvergence, false);
    assert.equal(cls.address, "rg1qpxa8hhy6rshkz9zrreyr7fkpmcp8rzful9s3kn");
  });

  it("encodes the same program on regtest as trg1q…", () => {
    const hex = P2WPKH;
    const cls = classifyScriptPubKey(hex, "trg");
    assert.equal(cls.assetKind, "ORU");
    assert.ok(cls.address?.startsWith("trg1q"));
  });
});

describe("OP_CONVERGE live classification", () => {
  it("recognizes OP_CONVERGE as ORU burn / draw-entry, not a third asset", () => {
    const cls = classifyScriptPubKey(OP_CONVERGE_HEX, "rg");
    assert.equal(cls.assetKind, "ORU");
    assert.equal(cls.isConvergence, true);
    assert.equal(cls.witnessVersion, null);
    assert.equal(cls.address, null);
    assert.equal(cls.scriptName, OP_CONVERGE_SCRIPT_NAME);
    assert.equal(cls.scriptState, CONVERGENCE_OUTPUT_STATE);
    assert.equal(isOpConvergeScriptPubKey(OP_CONVERGE_HEX), true);
    assert.equal(isConvergenceScriptPubKey(OP_CONVERGE_HEX), true);
    assert.equal(isRetiredCvg2WitnessV1(OP_CONVERGE_HEX), false);
  });

  it("does not treat native Witness v1 RES as convergence", () => {
    const cls = classifyScriptPubKey(RES_NATIVE_V1, "rg");
    assert.equal(cls.assetKind, "RES");
    assert.equal(cls.isConvergence, false);
    assert.equal(cls.scriptName, "Witness v1 RES");
  });
});

describe("retired Witness v1 CVG2", () => {
  it("rejects retired CVG2 as a live convergence script and does not invent a third asset", () => {
    const cls = classifyScriptPubKey(RETIRED_CVG2_V1, "rg");
    assert.equal(isRetiredCvg2WitnessV1(RETIRED_CVG2_V1), true);
    assert.equal(isConvergenceScriptPubKey(RETIRED_CVG2_V1), false);
    assert.equal(isOpConvergeScriptPubKey(RETIRED_CVG2_V1), false);
    assert.equal(cls.assetKind, "INVALID");
    assert.equal(cls.isConvergence, false);
    assert.match(cls.scriptState ?? "", /Historical RFC-014/);
    assert.notEqual(cls.assetKind, "ORU");
    assert.notEqual(cls.assetKind, "RES");
    assert.notEqual(cls.assetKind, "NONE");
  });
});

describe("script-as-asset fail-closed classification", () => {
  it("classifies canonical OP_RES_MINT as RES", () => {
    const mint = "00" + "14" + "11".repeat(20) + "bb";
    const cls = classifyScriptPubKey(mint, "rg");
    assert.equal(cls.assetKind, "RES");
    assert.equal(cls.scriptName, "OP_RES_MINT");
    assert.equal(cls.address, null);
    assert.equal(isResMintScriptPubKey(mint), true);
  });

  it("rejects OP_5 mint encoding as INVALID, matching C++ P0-039", () => {
    const op5 = "55" + "14" + "11".repeat(20) + "bb";
    const cls = classifyScriptPubKey(op5, "rg");
    assert.equal(cls.assetKind, "INVALID");
    assert.equal(isResMintScriptPubKey(op5), false);
    assert.notEqual(cls.assetKind, "RES");
  });

  it("classifies GetResMintScript(5) PUSHBYTES_1 0x05 as RES", () => {
    const mint5 = "0105" + "14" + "11".repeat(20) + "bb";
    const cls = classifyScriptPubKey(mint5, "rg");
    assert.equal(cls.assetKind, "RES");
    assert.equal(isResMintScriptPubKey(mint5), true);
  });

  it("replays P0-024 classification golden JSON (no second path)", () => {
    const jsonPath = path.join(__dirname, "..", "..", "formal_verification", "vectors", "rigor_classification_vectors.json");
    const rows = JSON.parse(fs.readFileSync(jsonPath, "utf8")) as Array<{
      name: string;
      scriptPubKey: string;
      expected: string;
    }>;
    assert.ok(rows.length >= 14);
    for (const row of rows) {
      const cls = classifyScriptPubKey(row.scriptPubKey, "rg");
      assert.equal(cls.assetKind, row.expected, row.name);
    }
  });

  it("classifies v0/32 P2WSH as INVALID, not ORU", () => {
    const p2wsh = "0020" + "22".repeat(32);
    const cls = classifyScriptPubKey(p2wsh, "rg");
    assert.equal(cls.assetKind, "INVALID");
    assert.notEqual(cls.assetKind, "ORU");
  });

  it("classifies retired RES2 as INVALID, not RES", () => {
    const res2 = "5120" + Buffer.from("RES2", "ascii").toString("hex") + "00".repeat(28);
    const cls = classifyScriptPubKey(res2, "rg");
    assert.equal(cls.assetKind, "INVALID");
    assert.notEqual(cls.assetKind, "RES");
  });

  it("classifies unknown scripts as INVALID, never NONE or ORU", () => {
    const p2pkh = "76a914" + "33".repeat(20) + "88ac";
    const cls = classifyScriptPubKey(p2pkh, "rg");
    assert.equal(cls.assetKind, "INVALID");
    assert.notEqual(cls.assetKind, "NONE");
    assert.notEqual(cls.assetKind, "ORU");
  });
});
