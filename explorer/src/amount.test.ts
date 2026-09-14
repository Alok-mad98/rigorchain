import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  RES_COIN,
  RES_DISPLAY_DECIMALS,
  atomsToResDisplay,
  formatFixedAtoms,
  oruStringToAtoms,
  resStringToAtoms,
} from "./amount";

describe("RES eight-decimal amount scale", () => {
  it("locks 1 RES = 1e8 atoms, not 1e6 or 100", () => {
    assert.equal(RES_COIN, 100000000n);
    assert.equal(RES_DISPLAY_DECIMALS, 8);
    assert.notEqual(RES_COIN, 100n);
    assert.notEqual(RES_COIN, 1000000n);
  });

  it("formats and parses RPC eight-decimal strings", () => {
    assert.equal(atomsToResDisplay(RES_COIN), "1.00000000 RES");
    assert.equal(atomsToResDisplay(RES_COIN / 10n), "0.10000000 RES");
    assert.equal(atomsToResDisplay(1n), "0.00000001 RES");
    assert.equal(resStringToAtoms("1.00000000"), RES_COIN);
    assert.equal(resStringToAtoms("40.00000000"), 40n * RES_COIN);
    assert.equal(formatFixedAtoms(RES_COIN, 8), "1.00000000");
  });

  it("parses RPC scientific notation for 1-atom RES and 1024-atom ORU burns", () => {
    assert.equal(resStringToAtoms("1e-8"), 1n);
    assert.equal(resStringToAtoms("1E-8"), 1n);
    assert.equal(resStringToAtoms(String(1e-8)), 1n);
    assert.equal(oruStringToAtoms("1.024e-5"), 1024n);
    assert.equal(oruStringToAtoms("1024.0"), 1024n * 100000000n);
  });
});
