import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { isRpcNonRetryableError } from "./rpcCli";

describe("rpcCli non-retryable errors", () => {
  it("does not retry Method not found (retired lottery RPCs)", () => {
    assert.equal(isRpcNonRetryableError("error code: -32601\nerror message:\nMethod not found"), true);
    assert.equal(isRpcNonRetryableError("Command failed: rigor-cli getconvergencestats"), false);
    assert.equal(isRpcNonRetryableError("Work queue depth exceeded"), true);
  });
});
