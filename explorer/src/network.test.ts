import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  ALPHAPLUS_CODENAME,
  ALPHAPLUS_GENESIS_HASH,
  ALPHAPLUS_PRODUCT,
  bech32HrpForNetwork,
  defaultRpcPort,
  parseExplorerNetwork,
  rigorCliChainArgs,
} from "./network";
import {
  optionalBigInt,
  optionalHash,
  displayOrNA,
  displayOrNotAvailable,
  reverseHexBytes,
  witnessPushMatchesTxid,
  witness32ToDisplayTxid,
  isA8PayRootMismatch,
  EMPTY_DRAW_ELIGIBLE_ROOT,
} from "./rpcFields";

describe("AlphaPlus network helpers", () => {
  it("defaults mainnet HRP to rg and RPC to 8761", () => {
    assert.equal(parseExplorerNetwork(undefined), "mainnet");
    assert.equal(bech32HrpForNetwork("mainnet"), "rg");
    assert.equal(defaultRpcPort("mainnet"), 8761);
    assert.deepEqual(rigorCliChainArgs("mainnet"), []);
  });

  it("regtest HRP is trg; testnet HRP is configurable", () => {
    assert.equal(bech32HrpForNetwork("regtest"), "trg");
    assert.equal(bech32HrpForNetwork("testnet"), "trg");
    assert.equal(bech32HrpForNetwork("testnet", "custom"), "custom");
  });

  it("never uses Bitcoin 8332 as default", () => {
    assert.notEqual(defaultRpcPort("mainnet"), 8332);
    assert.notEqual(defaultRpcPort("testnet"), 8332);
  });

  it("pins AlphaPlus identity and genesis", () => {
    assert.equal(ALPHAPLUS_PRODUCT, "RIGOR v0.1.1-alphaplus");
    assert.equal(ALPHAPLUS_CODENAME, "AlphaPlus Consensus Freeze");
    assert.equal(
      ALPHAPLUS_GENESIS_HASH,
      "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758",
    );
  });
});

describe("defensive RPC fields", () => {
  it("bind_height missing becomes null", () => {
    const cr: Record<string, unknown> = {};
    assert.equal(optionalBigInt(cr["bind_height"]), null);
    assert.equal(displayOrNotAvailable(optionalBigInt(cr["bind_height"])), "Not available");
  });

  it("missing RES target displays N/A", () => {
    assert.equal(displayOrNA(null), "N/A");
    assert.equal(displayOrNA(""), "N/A");
  });

  it("zero bind_hash is treated as absent", () => {
    assert.equal(optionalHash("0000000000000000000000000000000000000000000000000000000000000000"), null);
  });

  it("matches PCORB witness[0] against RPC display winner txid in either endianness", () => {
    const display = "a80d8c1706877e3b748cbcdd6f1a00c746426829b30865542b7255f99dcae3b3";
    const native = reverseHexBytes(display);
    assert.notEqual(native, display);
    assert.equal(witnessPushMatchesTxid(native, display), true);
    assert.equal(witnessPushMatchesTxid(display, display), true);
    assert.equal(witness32ToDisplayTxid(native), display);
    assert.equal(witnessPushMatchesTxid(native, "00".repeat(32)), false);
  });

  it("A-8 root check is parent.computed vs this.committed, not same-block fields", () => {
    const empty = EMPTY_DRAW_ELIGIBLE_ROOT;
    const awarded = "5371455808011a2a80f870425e923d25eded85fb65246af20b06b8ebd2634a39";
    // Height 305 awards: same-block committed≠computed, but it pays the parent's empty draw.
    assert.equal(isA8PayRootMismatch(empty, empty, 305), false);
    // Height 306 pays 305's awarded root even though this block's computed is empty.
    assert.equal(isA8PayRootMismatch(awarded, awarded, 306), false);
    assert.equal(isA8PayRootMismatch(empty, awarded, 306), true);
    assert.equal(isA8PayRootMismatch(awarded, empty, 0), false);
    assert.equal(isA8PayRootMismatch(awarded, null, 306), false);
    // Genesis computed is uint256(); height 1 commits the empty eligible-set root.
    assert.equal(isA8PayRootMismatch(empty, "0".repeat(64), 1), false);
    assert.equal(isA8PayRootMismatch(awarded, "0".repeat(64), 1), true);
  });
});
