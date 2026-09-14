import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { atomsFromSql, reconcileIndexedAsset, reconcileOruSupplySplit } from "./accounting";

describe("indexer asset reconciliation", () => {
  it("sets current balance to unspent and UTXO count to unspent count", () => {
    const row = reconcileIndexedAsset({
      receivedAtoms: 100n,
      spentAtoms: 100n,
      unspentAtoms: 0n,
      unspentCount: 0,
      spendableCount: 0,
    });
    assert.equal(row.consistent, true);
    assert.equal(row.currentBalanceAtoms, 0n);
    assert.equal(row.totalUtxoCount, 0);
    assert.equal(row.totalReceivedAtoms, 100n);
    assert.equal(row.totalSpentAtoms, 100n);
  });

  it("rejects counting a spent output as a UTXO", () => {
    const row = reconcileIndexedAsset({
      receivedAtoms: 100n,
      spentAtoms: 100n,
      unspentAtoms: 0n,
      unspentCount: 1,
      spendableCount: 0,
    });
    assert.equal(row.consistent, false);
    assert.match(row.errors.join(" "), /unspent atoms are 0/);
  });

  it("requires received = spent + unspent + locked", () => {
    const ok = reconcileIndexedAsset({
      receivedAtoms: 150n,
      spentAtoms: 100n,
      unspentAtoms: 40n,
      lockedAtoms: 10n,
      unspentCount: 1,
      spendableCount: 1,
    });
    assert.equal(ok.consistent, true);
    const bad = reconcileIndexedAsset({
      receivedAtoms: 150n,
      spentAtoms: 100n,
      unspentAtoms: 40n,
      unspentCount: 1,
      spendableCount: 1,
    });
    assert.equal(bad.consistent, false);
  });

  it("forbids spendable count above unspent count", () => {
    const row = reconcileIndexedAsset({
      receivedAtoms: 50n,
      spentAtoms: 0n,
      unspentAtoms: 50n,
      unspentCount: 1,
      spendableCount: 2,
    });
    assert.equal(row.consistent, false);
  });
});

describe("ORU supply split", () => {
  it("requires mature + immature = unlocked unspent", () => {
    assert.equal(
      reconcileOruSupplySplit({
        matureAtoms: 80n,
        immatureAtoms: 20n,
        unspentUnlockedAtoms: 100n,
      }).consistent,
      true,
    );
    assert.equal(
      reconcileOruSupplySplit({
        matureAtoms: 80n,
        immatureAtoms: 20n,
        unspentUnlockedAtoms: 90n,
      }).consistent,
      false,
    );
  });
});

describe("atomsFromSql", () => {
  it("parses pg numeric text", () => {
    assert.equal(atomsFromSql("100000000"), 100000000n);
    assert.equal(atomsFromSql(null), 0n);
  });
});
