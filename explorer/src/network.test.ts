import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  BECH32_HRP,
  MAINNET_GENESIS_HASH,
  RIGOR_CHAIN_NAME,
  RIGOR_CODENAME,
  RIGOR_PRODUCT,
  RPC_CHAIN_ID,
  bech32HrpForNetwork,
  defaultRpcPort,
  expectedGenesisForNetwork,
  parseExplorerNetwork,
  requireExplorerNetwork,
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

describe("v0.1.3 network helpers", () => {
  it("defaults the parse helper to mainnet but refuses unset network at startup", () => {
    assert.equal(parseExplorerNetwork(undefined), "mainnet");
    assert.equal(bech32HrpForNetwork("mainnet"), "rg");
    assert.equal(defaultRpcPort("mainnet"), 8761);
    assert.deepEqual(rigorCliChainArgs("mainnet"), []);
    assert.throws(() => requireExplorerNetwork(undefined), /RIGOR_NETWORK is required/);
    assert.throws(() => requireExplorerNetwork("bitcoin"), /Unknown RIGOR_NETWORK/);
  });

  it("maps HRP per node chainparams and rejects a non-canonical override", () => {
    assert.equal(bech32HrpForNetwork("mainnet"), BECH32_HRP.mainnet);
    assert.equal(bech32HrpForNetwork("testnet"), "trg");
    assert.equal(bech32HrpForNetwork("regtest"), "rgrt");
    assert.equal(bech32HrpForNetwork("devtest"), "dvt");
    assert.equal(bech32HrpForNetwork("testnet", "trg"), "trg");
    assert.throws(() => bech32HrpForNetwork("testnet", "custom"), /canonical HRP/);
  });

  it("pins RPC chain ids and frozen genesis per network", () => {
    assert.equal(RPC_CHAIN_ID.mainnet, "main");
    assert.equal(RPC_CHAIN_ID.testnet, "test");
    assert.equal(RPC_CHAIN_ID.regtest, "regtest");
    assert.equal(expectedGenesisForNetwork("mainnet", {}), MAINNET_GENESIS_HASH);
    assert.equal(expectedGenesisForNetwork("regtest", {}), null);
    assert.equal(expectedGenesisForNetwork("regtest", { EXPECTED_GENESIS_HASH: "ab" }), "ab");
  });

  it("never uses Bitcoin 8332 as default", () => {
    assert.notEqual(defaultRpcPort("mainnet"), 8332);
    assert.notEqual(defaultRpcPort("testnet"), 8332);
  });

  it("pins v0.1.3 identity and live mainnet genesis", () => {
    assert.equal(RIGOR_CHAIN_NAME, "RIGOR");
    assert.equal(RIGOR_PRODUCT, "RIGOR v0.1.3");
    assert.equal(RIGOR_CODENAME, "Production Release");
    assert.equal(
      MAINNET_GENESIS_HASH,
      "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717",
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

  it("matches a native 32-byte push against RPC display winner txid in either endianness", () => {
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
