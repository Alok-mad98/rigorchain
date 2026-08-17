import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  collectConsensusFingerprint,
  computeConsensusFingerprintNative,
  displayFingerprint,
  fingerprintStatus,
  rpcHashToNative,
  shortFingerprint,
  winnerSigmaFromCandidates,
} from "./consensusFingerprint";

describe("consensusFingerprint", () => {
  it("matches published native-order vectors", () => {
    const vectors = [
      {
        convergence_root: "0000000000000000000000000000000000000000000000000000000000000000",
        winner_txid: "0000000000000000000000000000000000000000000000000000000000000000",
        sigma: "0000000000000000000000000000000000000000000000000000000000000000",
        height: 100,
        target_prev: 0x00000000,
        expected: "a58b9a1f2d0f3994ef8e81b08258ae0ef130a4f234b1e331f4ee4efef2ad35d4",
      },
      {
        convergence_root: "0101010101010101010101010101010101010101010101010101010101010101",
        winner_txid: "0202020202020202020202020202020202020202020202020202020202020202",
        sigma: "0303030303030303030303030303030303030303030303030303030303030303",
        height: 1,
        target_prev: 0x1d00ffff,
        expected: "632cc6d0d5ab86a2fa74219cdf6f3f1cef91b71abc4aa34e52a496303e4adb8d",
      },
      {
        convergence_root: "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f",
        winner_txid: "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f",
        sigma: "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f",
        height: 210000,
        target_prev: 0x1c0fffff,
        expected: "062925341840c6feac9f86f9964f6d35fff81088c51b358c88e78cb374e01950",
      },
    ];
    for (const v of vectors) {
      const got = computeConsensusFingerprintNative({
        convergenceRootNativeHex: v.convergence_root,
        winnerTxidNativeHex: v.winner_txid,
        sigmaNativeHex: v.sigma,
        height: v.height,
        targetPrev: v.target_prev,
      });
      assert.equal(got, v.expected);
    }
  });

  it("prefers a node-published fingerprint and does not require other fields", () => {
    const published = "a58b9a1f2d0f3994ef8e81b08258ae0ef130a4f234b1e331f4ee4efef2ad35d4";
    assert.equal(collectConsensusFingerprint({ published }), published);
  });

  it("returns null when winner/sigma/C are missing (genesis / N/A)", () => {
    assert.equal(
      collectConsensusFingerprint({
        convergenceRootRpc: "00".repeat(32),
        height: 0,
        targetPrevBits: "1d00ffff",
      }),
      null,
    );
    assert.equal(displayFingerprint(null), "Not available");
    assert.equal(fingerprintStatus(null), "Not available");
    assert.equal(shortFingerprint(null), "N/A");
  });

  it("reconstructs the native vector from GetHex RPC fields", () => {
    const nativeC = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    const nativeW = "202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f";
    const nativeS = "404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f";
    const rpcC = Buffer.from(nativeC, "hex").reverse().toString("hex");
    const rpcW = Buffer.from(nativeW, "hex").reverse().toString("hex");
    const rpcS = Buffer.from(nativeS, "hex").reverse().toString("hex");
    assert.equal(rpcHashToNative(rpcC)?.toString("hex"), nativeC);
    const got = collectConsensusFingerprint({
      convergenceRootRpc: rpcC,
      winnerTxidRpc: rpcW,
      sigmaRpc: rpcS,
      height: 210000,
      targetPrevBits: "1c0fffff",
    });
    assert.equal(got, "062925341840c6feac9f86f9964f6d35fff81088c51b358c88e78cb374e01950");
  });

  it("picks winner sigma from getconvergenceresult candidates", () => {
    assert.equal(
      winnerSigmaFromCandidates("aa".repeat(32), [
        { txid: "bb".repeat(32), h: "11".repeat(32) },
        { txid: "AA".repeat(32), h: "22".repeat(32) },
      ]),
      "22".repeat(32),
    );
  });
});
