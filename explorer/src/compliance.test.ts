import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { INDEXER_LAG_CRITICAL_BLOCKS, isIntegrityIndexerLag, isRpcWorkQueueError } from "./compliance";

describe("compliance integrity vs RPC load", () => {
  it("treats work-queue overflow as operational, not an integrity mismatch", () => {
    const err = new Error(
      "Command failed: rigor-cli getblockhash 0 error: Server response: Work queue depth exceeded",
    );
    assert.equal(isRpcWorkQueueError(err), true);
    assert.equal(isRpcWorkQueueError(new Error("connection refused")), false);
  });

  it("does not flag indexer lag inside one draw window as CRITICAL", () => {
    assert.equal(isIntegrityIndexerLag(1), false);
    assert.equal(isIntegrityIndexerLag(INDEXER_LAG_CRITICAL_BLOCKS), false);
    assert.equal(isIntegrityIndexerLag(INDEXER_LAG_CRITICAL_BLOCKS + 1), true);
  });
});
