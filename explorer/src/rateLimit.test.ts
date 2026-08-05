import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { checkRateLimit, resetRateLimitBucketsForTests } from "./rateLimit";

describe("rateLimit", () => {
  it("allows requests under per-route cap", () => {
    resetRateLimitBucketsForTests();
    const profile = { windowMs: 60_000, maxPerWindow: 3 };
    assert.equal(checkRateLimit("1.2.3.4", "/api/tx/:txid", profile, 1000).allowed, true);
    assert.equal(checkRateLimit("1.2.3.4", "/api/tx/:txid", profile, 2000).allowed, true);
    assert.equal(checkRateLimit("1.2.3.4", "/api/tx/:txid", profile, 3000).allowed, true);
    const blocked = checkRateLimit("1.2.3.4", "/api/tx/:txid", profile, 4000);
    assert.equal(blocked.allowed, false);
    assert.ok(blocked.retryAfterSec >= 1);
  });

  it("isolates buckets per IP and route", () => {
    resetRateLimitBucketsForTests();
    const profile = { windowMs: 60_000, maxPerWindow: 1 };
    assert.equal(checkRateLimit("1.2.3.4", "/api/tx/:txid", profile, 1000).allowed, true);
    assert.equal(checkRateLimit("5.6.7.8", "/api/tx/:txid", profile, 1000).allowed, true);
    assert.equal(checkRateLimit("1.2.3.4", "/api/block/:hash", profile, 1000).allowed, true);
  });
});
