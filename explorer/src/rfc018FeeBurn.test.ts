import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  ORU_COIN,
  RES_COIN,
  destroyedFeeOruForBlock,
  nextCumulativeBurn,
  oruResTransferFeeBurnShare,
  rfc018DestroyedFeeOru,
  rollbackCumulativeBurn,
  rfc018FeeBurnView,
} from "./rfc018FeeBurn";

const oru = (n: bigint): { asset_kind: "ORU"; value_atoms: bigint } => ({
  asset_kind: "ORU",
  value_atoms: n,
});
const res = (n: bigint): { asset_kind: "RES"; value_atoms: bigint } => ({
  asset_kind: "RES",
  value_atoms: n,
});

describe("RFC-018 explorer fee-burn statistics", () => {
  it("locks 1/2 floor share and eight-decimal coins (not 1e6 or 100 atoms)", () => {
    assert.equal(ORU_COIN, 100000000n);
    assert.equal(RES_COIN, 100000000n);
    assert.notEqual(RES_COIN, 100n);
    assert.notEqual(RES_COIN, 1000000n);
    assert.equal(oruResTransferFeeBurnShare(0n), 0n);
    assert.equal(oruResTransferFeeBurnShare(1n), 0n);
    assert.equal(oruResTransferFeeBurnShare(2n), 1n);
    assert.equal(oruResTransferFeeBurnShare(1000n), 500n);
    assert.equal(oruResTransferFeeBurnShare(ORU_COIN), ORU_COIN / 2n);
  });

  it("burns floor(ORU_fee/2) on a RES-transfer block", () => {
    const burn = rfc018DestroyedFeeOru({
      isCoinbase: false,
      isResMint: false,
      inputs: [res(40n * RES_COIN), oru(ORU_COIN)],
      outputs: [res(40n * RES_COIN), oru(ORU_COIN - 1000n)],
    });
    assert.equal(burn, 500n);
  });

  it("burns nothing on a block without RES-transfer fees", () => {
    const pureOru = rfc018DestroyedFeeOru({
      isCoinbase: false,
      isResMint: false,
      inputs: [oru(ORU_COIN)],
      outputs: [oru(ORU_COIN - 2000n)],
    });
    const coinbase = rfc018DestroyedFeeOru({
      isCoinbase: true,
      isResMint: false,
      inputs: [],
      outputs: [oru(125n * ORU_COIN)],
    });
    const mint = rfc018DestroyedFeeOru({
      isCoinbase: false,
      isResMint: true,
      inputs: [oru(1024n * ORU_COIN)],
      outputs: [res(40n * RES_COIN), oru(ORU_COIN)],
    });
    assert.equal(pureOru, 0n);
    assert.equal(coinbase, 0n);
    assert.equal(mint, 0n);
  });

  it("sums multiple RES-transfer transactions in one block", () => {
    const txs = [
      {
        isCoinbase: true,
        isResMint: false,
        inputs: [] as Array<{ asset_kind: "ORU" | "RES"; value_atoms: bigint }>,
        outputs: [oru(125n * ORU_COIN)],
      },
      {
        isCoinbase: false,
        isResMint: false,
        inputs: [res(RES_COIN), oru(2n * ORU_COIN)],
        outputs: [res(RES_COIN), oru(2n * ORU_COIN - 4000n)],
      },
      {
        isCoinbase: false,
        isResMint: false,
        inputs: [res(RES_COIN), oru(ORU_COIN)],
        outputs: [res(RES_COIN), oru(ORU_COIN - 1000n)],
      },
    ];
    assert.equal(destroyedFeeOruForBlock(txs), 2000n + 500n);
  });

  it("accumulates cumulative burn across blocks", () => {
    const b0 = 0n;
    const b1 = 500n;
    const b2 = 0n;
    const b3 = 1500n;
    const c1 = nextCumulativeBurn(0n, b0);
    const c2 = nextCumulativeBurn(c1, b1);
    const c3 = nextCumulativeBurn(c2, b2);
    const c4 = nextCumulativeBurn(c3, b3);
    assert.equal(c1, 0n);
    assert.equal(c2, 500n);
    assert.equal(c3, 500n);
    assert.equal(c4, 2000n);
  });

  it("rolls back the disconnected tip without summing the abandoned branch", () => {
    const parentCum = 500n;
    const oldTipBurn = 1500n;
    const newBranchBurn = 100n;
    const afterOldTip = nextCumulativeBurn(parentCum, oldTipBurn);
    assert.equal(afterOldTip, 2000n);
    const restored = rollbackCumulativeBurn(afterOldTip, oldTipBurn);
    assert.equal(restored, parentCum);
    const afterNewTip = nextCumulativeBurn(restored, newBranchBurn);
    assert.equal(afterNewTip, 600n);
    assert.notEqual(afterNewTip, parentCum + oldTipBurn + newBranchBurn);
  });

  it("keeps fee-burn view fields separate from circulating and Convergence", () => {
    const view = rfc018FeeBurnView(500n, 1500n, (a) => `${a} atoms`);
    assert.equal(view.block_burned_oru, "500");
    assert.equal(view.cumulative_burned_oru, "1500");
    assert.equal(view.equivalence_claimed, false);
    assert.equal("oru_circulating_spendable" in view, false);
    assert.equal("cumulative_convergence" in view, false);
    assert.equal("total_issued_oru" in view, false);
  });
});
