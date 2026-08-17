/**
 * Explorer-only primary-output selection. Does not change consensus or CTxOut.
 * Script classification: Witness v1 ORU is a CVG2 unspendable script; otherwise
 * the largest ORU output is the payment (change is typically the smaller remainder).
 */

/** Display label for the CVG2 script encoding — not a spendable receiver. */
export const CVG2_SCRIPT_LABEL = "CVG2 / unspendable convergence script";

export type OutputRow = {
  vout_index: number;
  value_atoms: string | bigint | number;
  asset_kind: string;
  address?: string | null;
  witness_version?: number | string | null;
};

export type PrimaryOruRole = "convergence_lock" | "payment" | "output";

export type PrimaryOruPick = {
  output: OutputRow;
  role: PrimaryOruRole;
};

export function isWitnessV1(version: number | string | null | undefined): boolean {
  return Number(version) === 1;
}

export function pickPrimaryOruOutput(vout: OutputRow[]): PrimaryOruPick | null {
  const oru = vout.filter((o) => o.asset_kind === "ORU");
  if (oru.length === 0) return null;

  const lock = oru.find((o) => isWitnessV1(o.witness_version));
  if (lock) return { output: lock, role: "convergence_lock" };

  if (oru.length === 1) return { output: oru[0], role: "output" };

  const primary = oru.reduce((a, b) => (BigInt(a.value_atoms) >= BigInt(b.value_atoms) ? a : b));
  return { output: primary, role: "payment" };
}

export type TxKind = "coinbase" | "convergence_lock" | "res_mint" | "oru_transfer" | "other";

export function classifyTxKind(args: {
  isCoinbase: boolean;
  primaryRole: PrimaryOruRole | null;
  hasOru: boolean;
  hasRes?: boolean;
  isResMint?: boolean;
}): TxKind {
  if (args.isCoinbase) return "coinbase";
  if (args.isResMint) return "res_mint";
  if (args.primaryRole === "convergence_lock") return "convergence_lock";
  if (args.hasOru) return "oru_transfer";
  if (args.hasRes) return "res_mint";
  return "other";
}

export function txKindLabel(kind: TxKind): string {
  if (kind === "coinbase") return "Coinbase";
  if (kind === "convergence_lock") return CVG2_SCRIPT_LABEL;
  if (kind === "res_mint") return "RES mint";
  if (kind === "oru_transfer") return "ORU transfer";
  return "Other";
}

export function amountRoleLabel(role: PrimaryOruRole | null): string {
  if (role === "convergence_lock") return CVG2_SCRIPT_LABEL;
  if (role === "payment") return "Payment";
  if (role === "output") return "Output";
  return "Amount";
}

/** Summary-row header: CVG2 is a script encoding, not a payment receiver. */
export function destinationLabel(role: PrimaryOruRole | string | null | undefined): string {
  return role === "convergence_lock" ? CVG2_SCRIPT_LABEL : "Receiver";
}
