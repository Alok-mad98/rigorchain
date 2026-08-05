import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { averageBlockTimeSeconds, DRAW_STATUS_DISPLAY, fSetDistribution, RPC_AUTHORITY_DISCLAIMER } from "./status";

describe("status dashboard helpers", () => {
  it("computes average block time from indexed timestamps", () => {
    const avg = averageBlockTimeSeconds([100, 200, 400]);
    assert.equal(avg, 150);
  });

  it("builds |F| distribution histogram", () => {
    const dist = fSetDistribution([0, 2, 2, 4]);
    assert.deepEqual(dist, { "0": 1, "2": 2, "4": 1 });
  });

  it("exposes FVR-M draw status constant", () => {
    assert.match(DRAW_STATUS_DISPLAY, /PERMANENTLY IMPOSSIBLE/);
    assert.match(DRAW_STATUS_DISPLAY, /FVR-M/);
  });

  it("RPC disclaimer forbids local consensus computation", () => {
    assert.match(RPC_AUTHORITY_DISCLAIMER, /rigord RPC/);
    assert.match(RPC_AUTHORITY_DISCLAIMER, /No local consensus computation/);
  });
});
