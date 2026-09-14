/**
 * Indexer presentation accounting. Does not compute consensus.
 *
 * Address current balance is unspent atoms at indexer height.
 * Total received / spent are lifetime. Do not display spent outputs as UTXOs.
 */

export type IndexedAssetRow = {
  receivedAtoms: bigint;
  spentAtoms: bigint;
  unspentAtoms: bigint;
  unspentCount: number;
  spendableCount: number;
  /** Unspent but not current balance (OP_CONVERGE / retired CVG2). */
  lockedAtoms?: bigint;
};

export type ReconciledAssetSummary = {
  consistent: boolean;
  errors: string[];
  currentBalanceAtoms: bigint;
  totalUtxoCount: number;
  spendableUtxoCount: number;
  totalReceivedAtoms: bigint;
  totalSpentAtoms: bigint;
};

function asNonNegInt(n: number): number {
  if (!Number.isFinite(n) || n < 0) return 0;
  return Math.floor(n);
}

function asAtoms(v: bigint): bigint {
  return v < 0n ? 0n : v;
}

/**
 * Derive the public address totals from one spent-flag partition.
 * current balance = unspent; Total UTXOs = unspent count; spendable ≤ unspent.
 */
export function reconcileIndexedAsset(row: IndexedAssetRow): ReconciledAssetSummary {
  const receivedAtoms = asAtoms(row.receivedAtoms);
  const spentAtoms = asAtoms(row.spentAtoms);
  const unspentAtoms = asAtoms(row.unspentAtoms);
  const lockedAtoms = asAtoms(row.lockedAtoms ?? 0n);
  const unspentCount = asNonNegInt(row.unspentCount);
  const spendableCount = asNonNegInt(row.spendableCount);
  const errors: string[] = [];

  if (receivedAtoms !== spentAtoms + unspentAtoms + lockedAtoms) {
    errors.push(
      `received (${receivedAtoms}) != spent (${spentAtoms}) + unspent (${unspentAtoms}) + locked (${lockedAtoms})`,
    );
  }
  if (unspentCount === 0 && unspentAtoms !== 0n) {
    errors.push(`unspent count is 0 but unspent atoms are ${unspentAtoms}`);
  }
  if (unspentAtoms === 0n && unspentCount !== 0) {
    errors.push(`unspent atoms are 0 but unspent count is ${unspentCount}`);
  }
  if (spendableCount > unspentCount) {
    errors.push(`spendable count ${spendableCount} > unspent count ${unspentCount}`);
  }

  return {
    consistent: errors.length === 0,
    errors,
    currentBalanceAtoms: unspentAtoms,
    totalUtxoCount: unspentCount,
    spendableUtxoCount: spendableCount,
    totalReceivedAtoms: receivedAtoms,
    totalSpentAtoms: spentAtoms,
  };
}

export type OruSupplySplit = {
  matureAtoms: bigint;
  immatureAtoms: bigint;
  unspentUnlockedAtoms: bigint;
};

/**
 * Mature + immature must partition unlocked unspent ORU (no OP_CONVERGE / retired CVG2).
 * Circulating spendable is the mature slice only.
 */
export function reconcileOruSupplySplit(split: OruSupplySplit): { consistent: boolean; errors: string[] } {
  const mature = asAtoms(split.matureAtoms);
  const immature = asAtoms(split.immatureAtoms);
  const unlocked = asAtoms(split.unspentUnlockedAtoms);
  const errors: string[] = [];
  if (mature + immature !== unlocked) {
    errors.push(
      `mature (${mature}) + immature (${immature}) != unlocked unspent (${unlocked})`,
    );
  }
  return { consistent: errors.length === 0, errors };
}

export function atomsFromSql(v: unknown): bigint {
  try {
    return BigInt(String(v ?? "0"));
  } catch {
    return 0n;
  }
}
