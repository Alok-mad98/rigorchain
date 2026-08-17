/**
 * Explorer fee display only. Does not change the consensus fee model.
 * ORU: fee_i = ORU_in − ORU_out. Coinbase: 0. RES: never invent a fee.
 */

export type FeeKind = "coinbase" | "oru" | "na";

export type FeeView = {
  fee_atoms: string | null;
  fee_display: string;
  fee_kind: FeeKind;
};

const ORU_DECIMALS = 8n;
const ORU_BASE = 100000000n;

export function formatOruAtoms(atoms: bigint): string {
  const neg = atoms < 0n;
  const a = neg ? -atoms : atoms;
  const int = a / ORU_BASE;
  const frac = a % ORU_BASE;
  return `${neg ? "-" : ""}${int}.${frac.toString().padStart(Number(ORU_DECIMALS), "0")}`;
}

export function computeTxFee(args: {
  isCoinbase: boolean;
  /** Sum of spent ORU input values. null if any ORU prevout is missing. */
  oruInputAtoms: bigint | null;
  oruOutputAtoms: bigint;
  /** True if any input or output is RES — do not invent RES fee logic. */
  involvesRes: boolean;
}): FeeView {
  if (args.isCoinbase) {
    return { fee_atoms: "0", fee_display: "0", fee_kind: "coinbase" };
  }
  if (args.involvesRes) {
    return { fee_atoms: null, fee_display: "N/A", fee_kind: "na" };
  }
  if (args.oruInputAtoms === null) {
    return { fee_atoms: null, fee_display: "N/A", fee_kind: "na" };
  }
  const fee = args.oruInputAtoms - args.oruOutputAtoms;
  return {
    fee_atoms: fee.toString(),
    fee_display: formatOruAtoms(fee),
    fee_kind: "oru",
  };
}
