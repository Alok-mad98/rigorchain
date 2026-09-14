import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { evaluateIssuanceGeneration, evaluateStartupIdentity, StartupClosedError } from "./startupGuard";
import { ISSUANCE_GENERATION } from "./resIssuanceSchedule";

describe("fail-closed startup identity", () => {
  it("accepts matching regtest node, HRP, and genesis pin", () => {
    const r = evaluateStartupIdentity({
      network: "regtest",
      bech32Hrp: "rgrt",
      nodeChain: "regtest",
      nodeGenesis: "aa".repeat(32),
      expectedGenesis: "aa".repeat(32),
      dbNetwork: "regtest",
    });
    assert.equal(r.ok, true);
    assert.deepEqual(r.errors, []);
  });

  it("refuses mainnet config pointed at a regtest node", () => {
    const r = evaluateStartupIdentity({
      network: "mainnet",
      bech32Hrp: "rg",
      nodeChain: "regtest",
      nodeGenesis: "aa".repeat(32),
      expectedGenesis: "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717",
      dbNetwork: "mainnet",
    });
    assert.equal(r.ok, false);
    assert.ok(r.errors.some((e) => /Chain mismatch/.test(e)));
    assert.ok(r.errors.some((e) => /Genesis mismatch/.test(e)));
  });

  it("refuses HRP override that is not the network canonical HRP", () => {
    const r = evaluateStartupIdentity({
      network: "mainnet",
      bech32Hrp: "trg",
      nodeChain: "main",
      nodeGenesis: "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717",
      expectedGenesis: "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717",
      dbNetwork: "mainnet",
    });
    assert.equal(r.ok, false);
    assert.ok(r.errors.some((e) => /HRP mismatch/.test(e)));
  });

  it("refuses an indexer DB built for a different network", () => {
    const r = evaluateStartupIdentity({
      network: "regtest",
      bech32Hrp: "rgrt",
      nodeChain: "regtest",
      nodeGenesis: "aa".repeat(32),
      expectedGenesis: null,
      dbNetwork: "mainnet",
    });
    assert.equal(r.ok, false);
    assert.ok(r.errors.some((e) => /Indexer DB network/.test(e)));
  });

  it("allows unpinned regtest genesis when chain and HRP match", () => {
    const r = evaluateStartupIdentity({
      network: "regtest",
      bech32Hrp: "rgrt",
      nodeChain: "regtest",
      nodeGenesis: "bb".repeat(32),
      expectedGenesis: null,
      dbNetwork: "regtest",
    });
    assert.equal(r.ok, true);
  });

  it("stamps issuance generation on an empty index", () => {
    const r = evaluateIssuanceGeneration({
      storedGeneration: null,
      lastHeight: -1,
      expectedGeneration: ISSUANCE_GENERATION,
    });
    assert.equal(r.ok, true);
    assert.equal(r.shouldStamp, true);
  });

  it("restamps a leftover generation on an empty index", () => {
    const r = evaluateIssuanceGeneration({
      storedGeneration: "rfc019:cap=61345467819135:p0=102400000000",
      lastHeight: -1,
      expectedGeneration: ISSUANCE_GENERATION,
    });
    assert.equal(r.ok, true);
    assert.equal(r.shouldStamp, true);
  });

  it("refuses a populated index with no issuance generation", () => {
    const r = evaluateIssuanceGeneration({
      storedGeneration: null,
      lastHeight: 10,
      expectedGeneration: ISSUANCE_GENERATION,
    });
    assert.equal(r.ok, false);
    assert.equal(r.shouldStamp, false);
    assert.ok(r.errors.some((e) => /no issuance_generation/.test(e)));
  });

  it("refuses an index still pinned to the RFC-019 cap generation", () => {
    const r = evaluateIssuanceGeneration({
      storedGeneration: "rfc019:cap=61345467819135:p0=102400000000",
      lastHeight: 10,
      expectedGeneration: ISSUANCE_GENERATION,
    });
    assert.equal(r.ok, false);
    assert.ok(r.errors.some((e) => /613454/.test(e)));
  });

  it("accepts a matching RFC-020 issuance generation", () => {
    const r = evaluateIssuanceGeneration({
      storedGeneration: ISSUANCE_GENERATION,
      lastHeight: 10,
      expectedGeneration: ISSUANCE_GENERATION,
    });
    assert.equal(r.ok, true);
    assert.equal(r.shouldStamp, false);
  });

  it("formats a closed error from the identity list", () => {
    const err = new StartupClosedError(["Chain mismatch"]);
    assert.match(err.message, /Explorer startup refused/);
    assert.deepEqual(err.errors, ["Chain mismatch"]);
  });
});
