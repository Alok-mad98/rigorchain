import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  RES_COIN,
  RES_DISPLAY_DECIMALS,
  atomsToResDisplay,
  formatFixedAtoms,
  resStringToAtoms,
} from "./amount";

describe("RES six-decimal amount scale", () => {
  it("locks 1 RES = 1e6 atoms, not 100", () => {
    assert.equal(RES_COIN, 1000000n);
    assert.equal(RES_DISPLAY_DECIMALS, 6);
    assert.notEqual(RES_COIN, 100n);
  });

  it("formats and parses RPC six-decimal strings", () => {
    assert.equal(atomsToResDisplay(RES_COIN), "1.000000 RES");
    assert.equal(atomsToResDisplay(RES_COIN / 10n), "0.100000 RES");
    assert.equal(atomsToResDisplay(1n), "0.000001 RES");
    assert.equal(resStringToAtoms("1.000000"), RES_COIN);
    assert.equal(resStringToAtoms("40.000000"), 40n * RES_COIN);
    assert.equal(formatFixedAtoms(RES_COIN, 6), "1.000000");
  });
});
