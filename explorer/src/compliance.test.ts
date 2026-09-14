import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { isCatchupIndexerLag, isIntegrityIndexerLag, isRpcWorkQueueError } from "./compliance";

describe("compliance integrity vs RPC load", () => {
  it("treats work-queue overflow as operational, not an integrity mismatch", () => {
    const err = new Error(
      "Command failed: rigor-cli getblockhash 0 error: Server response: Work queue depth exceeded",
    );
    assert.equal(isRpcWorkQueueError(err), true);
    assert.equal(isRpcWorkQueueError(new Error("connection refused")), false);
  });

  it("never treats catch-up lag as CRITICAL integrity", () => {
    assert.equal(isIntegrityIndexerLag(1), false);
    assert.equal(isIntegrityIndexerLag(32), false);
    assert.equal(isIntegrityIndexerLag(434), false);
    assert.equal(isCatchupIndexerLag(0), false);
    assert.equal(isCatchupIndexerLag(434), true);
  });
});
