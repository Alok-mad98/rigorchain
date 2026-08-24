/**
 * Explorer circulating-ORU accounting (presentation).
 * Does not change consensus. Node UTXO set already omits OP_CONVERGE;
 * this layer must not add those locked outputs back into circulating charts.
 *
 * RFC-018 fee burns never create an output, so they cannot appear here.
 *
 * Historical RFC-014 / retired Witness v1 CVG2 rows may still exist in old
 * indexer DBs until reindex; they are also excluded from circulating.
 */

import { isConvergenceScriptPubKey, isRetiredCvg2WitnessV1 } from "./script";

export type IndexedOruRow = {
  spent: boolean;
  script_pubkey: string;
  amount_atoms: bigint | string | number;
};

export function isCirculatingSpendableOruRow(row: IndexedOruRow): boolean {
  if (row.spent) return false;
  if (isConvergenceScriptPubKey(row.script_pubkey)) return false;
  if (isRetiredCvg2WitnessV1(row.script_pubkey)) return false;
  return true;
}

/** Sum of indexed unspent ORU that may be treated as circulating / spendable UTXO. */
export function circulatingOruAtoms(rows: IndexedOruRow[]): bigint {
  let sum = 0n;
  for (const row of rows) {
    if (!isCirculatingSpendableOruRow(row)) continue;
    sum += BigInt(String(row.amount_atoms));
  }
  return sum;
}

/**
 * SQL predicate: unspent, not live OP_CONVERGE, and not historical RFC-014 CVG2 v1.
 * OP_CONVERGE wire: 0x14 || 20 bytes || 0xbd (44 hex chars).
 */
export const CIRCULATING_ORU_SQL_PREDICATE =
  "spent = FALSE" +
  " AND LOWER(REPLACE(TRIM(script_pubkey), ' ', '')) !~ '^14[0-9a-f]{40}bd$'" +
  " AND LOWER(script_pubkey) NOT LIKE '512043564732%'";
