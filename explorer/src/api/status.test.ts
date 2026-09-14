import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { averageBlockTimeSeconds, DRAW_STATUS_DISPLAY, drawWindowRemaining, fSetDistribution, isCoinbaseMature, RPC_AUTHORITY_DISCLAIMER } from "./status";

describe("status dashboard helpers", () => {
  it("computes average block time from indexed timestamps", () => {
    const avg = averageBlockTimeSeconds([100, 200, 400]);
    assert.equal(avg, 150);
  });

  it("counts blocks remaining until a candidate ages out of the draw window", () => {
    const r = drawWindowRemaining({ tipHeight: 178, windowSize: 32, sourceHeights: [171] });
    assert.deepEqual(r, { remaining: 24, completesAt: 202 });
    assert.equal(drawWindowRemaining({ tipHeight: 100, windowSize: 32, sourceHeights: [] }).remaining, null);
  });

  it("builds |F| distribution histogram", () => {
    const dist = fSetDistribution([0, 2, 2, 4]);
    assert.deepEqual(dist, { "0": 1, "2": 2, "4": 1 });
  });

  it("exposes issuance status constant", () => {
    assert.equal(DRAW_STATUS_DISPLAY, "ORU destroyed. RES minted.");
  });

  it("treats coinbase as transferable only after 100 confirmations", () => {
    assert.equal(isCoinbaseMature(206, 107), true);
    assert.equal(isCoinbaseMature(206, 108), false);
    assert.equal(isCoinbaseMature(100, 1), true);
    assert.equal(isCoinbaseMature(99, 1), false);
    assert.equal(isCoinbaseMature(99, 0), true);
  });

  it("RPC disclaimer forbids local consensus computation", () => {
    assert.match(RPC_AUTHORITY_DISCLAIMER, /rigord RPC/);
    assert.match(RPC_AUTHORITY_DISCLAIMER, /No local consensus computation/);
  });
});
