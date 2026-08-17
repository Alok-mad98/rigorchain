import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { computeTxFee, formatOruAtoms } from "./fee";

describe("explorer fee display", () => {
  it("coinbase fee is 0", () => {
    const f = computeTxFee({
      isCoinbase: true,
      oruInputAtoms: null,
      oruOutputAtoms: 1000n * 100000000n,
      involvesRes: false,
    });
    assert.equal(f.fee_kind, "coinbase");
    assert.equal(f.fee_display, "0");
    assert.equal(f.fee_atoms, "0");
  });

  it("ORU fee is inputs minus outputs", () => {
    const f = computeTxFee({
      isCoinbase: false,
      oruInputAtoms: 100000000n,
      oruOutputAtoms: 99999000n,
      involvesRes: false,
    });
    assert.equal(f.fee_kind, "oru");
    assert.equal(f.fee_atoms, "1000");
    assert.equal(f.fee_display, formatOruAtoms(1000n));
  });

  it("RES involvement is N/A", () => {
    const f = computeTxFee({
      isCoinbase: false,
      oruInputAtoms: 100000000n,
      oruOutputAtoms: 1n,
      involvesRes: true,
    });
    assert.equal(f.fee_kind, "na");
    assert.equal(f.fee_display, "N/A");
    assert.equal(f.fee_atoms, null);
  });

  it("missing inputs is N/A", () => {
    const f = computeTxFee({
      isCoinbase: false,
      oruInputAtoms: null,
      oruOutputAtoms: 1n,
      involvesRes: false,
    });
    assert.equal(f.fee_display, "N/A");
  });
});
