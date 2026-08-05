import { describe, it, beforeEach } from "node:test";
import assert from "node:assert/strict";
import {
  clearImporterRecovery,
  getImporterRecoveryState,
  recordImporterFailure,
  recordRecoveryRetry,
  resetImporterRecoveryForTests,
  shouldRetryImport,
} from "./importerRecovery";

describe("importerRecovery", () => {
  beforeEach(() => {
    resetImporterRecoveryForTests();
  });

  it("tracks stall and clears on success", () => {
    recordImporterFailure(1619, "root mismatch committed != computed");
    const stalled = getImporterRecoveryState();
    assert.equal(stalled.stalled_height, 1619);
    assert.equal(stalled.attempt_count, 1);
    assert.ok(stalled.last_failure_at);

    recordRecoveryRetry(1619);
    assert.ok(getImporterRecoveryState().last_retry_at);

    clearImporterRecovery(1620);
    const cleared = getImporterRecoveryState();
    assert.equal(cleared.stalled_height, null);
    assert.equal(cleared.attempt_count, 0);
  });

  it("respects retry interval after failure", () => {
    recordImporterFailure(100, "test");
    const t0 = Date.parse(getImporterRecoveryState().last_failure_at!);
    assert.equal(shouldRetryImport(t0, 60_000), false);
    assert.equal(shouldRetryImport(t0 + 60_000, 60_000), true);
  });

  it("allows import when not stalled", () => {
    assert.equal(shouldRetryImport(Date.now(), 60_000), true);
  });
});
