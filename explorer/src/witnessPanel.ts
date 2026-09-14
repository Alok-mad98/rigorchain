/** Presentation-only witness / RES mint proof mapping from indexed import rows. */

import type { TxConvergencePanel, TxConvergenceRole } from "./convergencePanel";
import { atomsToResDisplay } from "./amount";
import { isCoinbaseMintProofItems } from "./resMint";

export const READONLY_RPC_NOTICE = "Read-only view; authority from rigord RPC only.";

export const PCORB_FIELD_NAMES = [
  "convergence_txid",
  "convergence_amount_sat",
  "bind_hash",
  "height",
  "target_prev",
  "convergence_root",
] as const;

/** Byte length of each field in PCORB_FIELD_NAMES, as enforced by `Consensus::ParseConvergeProof`. */
export const PCORB_FIELD_BYTE_LENGTHS = [32, 8, 32, 8, 4, 32] as const;

export type WitnessPathKind = "coinbase" | "oru_input" | "res_mint_pcorb" | "res_key_path" | "other" | "none";

export type PcorbStackItem = {
  index: number;
  field_name: string;
  hex: string;
  note: string | null;
};

export type VinWitnessView = {
  vin_index: number;
  path_kind: WitnessPathKind;
  path_label: string;
  witness_stack: string[] | null;
  witness_hex: string;
  stack_depth: number;
  pcorb_items: PcorbStackItem[] | null;
};

export type ResMintConfirmationStatus = "Confirmed in block" | "Pending candidate state" | "Not a RES mint";

export type ResMintProofView = {
  present: boolean;
  mint_txid: string | null;
  mint_vout_index: number | null;
  mint_amount_atoms: string | null;
  mint_amount_display: string | null;
  winner_convergence_txid: string | null;
  block_hash: string | null;
  block_height: number | null;
  confirmation_status: ResMintConfirmationStatus;
  matches_winner_block: boolean;
  proof_items: PcorbStackItem[] | null;
  convergence_root: string | null;
};

export type TxProofStatusView = {
  candidate_set_height: number | null;
  winner_status: TxConvergenceRole;
  convergence_root: string | null;
};

export type TxWitnessPanel = {
  inputs: VinWitnessView[];
  res_mint_proof: ResMintProofView;
  proof_status: TxProofStatusView;
  rpc_readonly_notice: string;
};

function asWitnessStack(raw: unknown): string[] | null {
  if (!Array.isArray(raw)) return null;
  return raw.map((x) => String(x));
}

function hexByteLength(hex: string): number {
  return Math.floor(hex.replace(/^0x/i, "").length / 2);
}

/**
 * Historical 6-field PCORB stack shape. Live issuance is RFC-020 body-tx mint;
 * this matcher is display-only and is not a mint detector.
 */
export function hasPcorbProofFields(stack: string[] | null): boolean {
  if (!stack || stack.length < PCORB_FIELD_BYTE_LENGTHS.length) return false;
  return PCORB_FIELD_BYTE_LENGTHS.every((len, i) => hexByteLength(stack[i]) === len);
}

export function classifyWitnessPath(coinbase: boolean, stack: string[] | null): WitnessPathKind {
  if (coinbase) return "coinbase";
  if (!stack || stack.length === 0) return "none";
  if (hasPcorbProofFields(stack)) return "other";
  if (stack.length === 2) return "res_key_path";
  return "oru_input";
}

export function pathLabel(kind: WitnessPathKind): string {
  switch (kind) {
    case "coinbase":
      return "Coinbase";
    case "oru_input":
      return "ORU input witness";
    case "res_mint_pcorb":
      return "Retired PCORB stack (not issuance)";
    case "res_key_path":
      return "RES key-path spend";
    case "other":
      return "Witness (not a mint proof)";
    default:
      return "No witness";
  }
}

/** Pushes above the six proof fields belong to the funding script, not to the proof. */
export function buildPcorbItems(stack: string[]): PcorbStackItem[] {
  return stack.slice(0, PCORB_FIELD_NAMES.length).map((hex, index) => {
    const field_name = PCORB_FIELD_NAMES[index] ?? `item_${index}`;
    const note = field_name === "target_prev" ? "replay bind (parent nBits)" : null;
    return { index, field_name, hex, note };
  });
}

export { atomsToResDisplay } from "./amount";

export function buildVinWitnessView(vin: {
  vin_index: number;
  coinbase: boolean;
  txinwitness: unknown;
}): VinWitnessView {
  const stack = asWitnessStack(vin.txinwitness);
  const path_kind = classifyWitnessPath(!!vin.coinbase, stack);
  const witness_hex = stack ? stack.join("") : "";
  const pcorb_items = path_kind === "res_mint_pcorb" && stack ? buildPcorbItems(stack) : null;

  return {
    vin_index: vin.vin_index,
    path_kind,
    path_label: pathLabel(path_kind),
    witness_stack: stack,
    witness_hex,
    stack_depth: stack?.length ?? 0,
    pcorb_items,
  };
}

export function buildResMintProofView(
  txid: string,
  resMintRows: Array<Record<string, unknown>>,
  block: { block_hash: string; height: bigint | number; rigor_winner: string; rigor_res_minted: number } | null,
  convergencePanel: TxConvergencePanel,
  pcorbFromWitness: PcorbStackItem[] | null,
): ResMintProofView {
  const empty: ResMintProofView = {
    present: false,
    mint_txid: null,
    mint_vout_index: null,
    mint_amount_atoms: null,
    mint_amount_display: null,
    winner_convergence_txid: null,
    block_hash: null,
    block_height: null,
    confirmation_status: "Not a RES mint",
    matches_winner_block: false,
    proof_items: null,
    convergence_root: null,
  };

  if (resMintRows.length > 0) {
    const row = resMintRows[0];
    const winnerTxid = String(row.winner_convergence_txid ?? "");
    const blockHash = String(row.block_hash ?? "");
    const blockHeight = Number(row.block_height ?? 0);
    const proofRaw = row.proof_items as { witness_stack?: string[]; source?: string } | null;
    const stack =
      !isCoinbaseMintProofItems(proofRaw) && Array.isArray(proofRaw?.witness_stack)
        ? proofRaw!.witness_stack!
        : null;
    const proof_items = stack ? buildPcorbItems(stack) : null;

    return {
      present: true,
      mint_txid: txid,
      mint_vout_index: Number(row.mint_vout_index ?? 0),
      mint_amount_atoms: String(row.mint_amount ?? "0"),
      mint_amount_display: atomsToResDisplay(String(row.mint_amount ?? "0")),
      winner_convergence_txid: winnerTxid || null,
      block_hash: blockHash || null,
      block_height: blockHeight,
      confirmation_status: "Confirmed in block",
      matches_winner_block: Boolean(block?.rigor_res_minted === 1),
      proof_items,
      convergence_root: String(row.convergence_root ?? ""),
    };
  }

  if (convergencePanel.role === "Candidate") {
    return {
      ...empty,
      present: false,
      confirmation_status: "Pending candidate state",
      block_hash: convergencePanel.block_hash,
      block_height: convergencePanel.block_height,
      convergence_root: null,
    };
  }

  return empty;
}

export function buildTxProofStatus(
  convergencePanel: TxConvergencePanel,
  convergenceRows: Array<{ block_height: bigint | number; convergence_root?: string }>,
  resMintProof: ResMintProofView,
): TxProofStatusView {
  const root =
    resMintProof.convergence_root ??
    (convergenceRows[0]?.convergence_root ? String(convergenceRows[0].convergence_root) : null);

  return {
    candidate_set_height: convergencePanel.block_height ?? resMintProof.block_height,
    winner_status: convergencePanel.role,
    convergence_root: root,
  };
}

export function buildTxWitnessPanel(
  txid: string,
  vins: Array<{ vin_index: number; coinbase: boolean; txinwitness: unknown }>,
  resMintRows: Array<Record<string, unknown>>,
  block: { block_hash: string; height: bigint | number; rigor_winner: string; rigor_res_minted: number } | null,
  convergencePanel: TxConvergencePanel,
  convergenceRows: Array<{ block_height: bigint | number; convergence_root?: string }>,
): TxWitnessPanel {
  const inputs = vins.map(buildVinWitnessView);
  const res_mint_proof = buildResMintProofView(txid, resMintRows, block, convergencePanel, null);
  const proof_status = buildTxProofStatus(convergencePanel, convergenceRows, res_mint_proof);

  return {
    inputs,
    res_mint_proof,
    proof_status,
    rpc_readonly_notice: READONLY_RPC_NOTICE,
  };
}
