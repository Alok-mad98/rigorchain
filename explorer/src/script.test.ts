import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  classifyScriptPubKey,
  CONVERGENCE_OUTPUT_STATE,
  isConvergenceScriptPubKey,
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
    assert.equal(cls.assetKind, "NONE");
    assert.equal(cls.isConvergence, false);
    assert.match(cls.scriptState ?? "", /Historical RFC-014/);
    assert.notEqual(cls.assetKind, "ORU");
    assert.notEqual(cls.assetKind, "RES");
  });
});
