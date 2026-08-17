/**
 * AlphaPlus consensus primitives reproduced in the browser.
 * Binary-first: all hashing is over Uint8Array.
 *
 * Winner tie-break follows production RFC-000 §9 / C++ ConvergenceWinnerTieBreakCompare:
 * unsigned byte comparison of txid (not XOR with C). That is the output nodes produce.
 */

import { bytesToBigInt, compareBytes, concatBytes, requireLen32, u32ToLeBytes, zero32 } from "./bytes";
import { resProbabilityTargetNative } from "./constants";
import { hash256 } from "./hash";

export async function computeConvergenceRoot(txidsNative: Uint8Array[]): Promise<Uint8Array> {
  if (txidsNative.length < 2) {
    return zero32();
  }
  const sorted = txidsNative.map((id) => requireLen32(id, "txid")).slice();
  sorted.sort(compareBytes);
  return hash256(concatBytes(...sorted));
}

/**
 * σ = Hash256(txid || ConvergenceRoot || PreviousBlockHash || Height(LE32))
 * Total preimage: 100 bytes.
 */
export async function computeSigma(
  txidNative: Uint8Array,
  convergenceRoot: Uint8Array,
  previousBlockHashNative: Uint8Array,
  height: number,
): Promise<Uint8Array> {
  const buf = concatBytes(
    requireLen32(txidNative, "txid"),
    requireLen32(convergenceRoot, "ConvergenceRoot"),
    requireLen32(previousBlockHashNative, "PreviousBlockHash"),
    u32ToLeBytes(height),
  );
  if (buf.length !== 100) {
    throw new Error(`sigma preimage must be 100 bytes, got ${buf.length}`);
  }
  return hash256(buf);
}

export function isEligible(sigmaNative: Uint8Array, weight: bigint, targetNative?: Uint8Array): boolean {
  if (weight < 1n) return false;
  const t = targetNative ?? resProbabilityTargetNative();
  const h = bytesToBigInt(requireLen32(sigmaNative, "sigma"));
  const target = bytesToBigInt(requireLen32(t, "T"));
  return h < target * weight;
}

export type WinnerCandidate = {
  txidNative: Uint8Array;
  sigmaNative: Uint8Array;
  weight: bigint;
};

/**
 * Eligible set: H_i < T · f_i
 * Selection: lowest H_i / f_i via 512-bit cross-multiply
 * Tie-break: unsigned byte comparison of txid (RFC-000 §9)
 */
export function selectWinner(candidates: WinnerCandidate[], targetNative?: Uint8Array): number {
  let winner = -1;
  for (let i = 0; i < candidates.length; i++) {
    const c = candidates[i];
    if (!isEligible(c.sigmaNative, c.weight, targetNative)) continue;
    if (winner < 0) {
      winner = i;
      continue;
    }
    const best = candidates[winner];
    const lhs = bytesToBigInt(c.sigmaNative) * best.weight;
    const rhs = bytesToBigInt(best.sigmaNative) * c.weight;
    if (lhs < rhs) {
      winner = i;
    } else if (lhs === rhs && compareBytes(c.txidNative, best.txidNative) < 0) {
      winner = i;
    }
  }
  return winner;
}
