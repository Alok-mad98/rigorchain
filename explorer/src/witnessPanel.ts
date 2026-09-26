/** Presentation-only witness / RES mint proof mapping from indexed import rows. */

import type { TxConvergencePanel, TxConvergenceRole } from "./convergencePanel";
import { atomsToResDisplay } from "./amount";

export const READONLY_RPC_NOTICE = "Read-only view; authority from rigord RPC only.";

export type WitnessPathKind = "coinbase" | "oru_input" | "res_key_path" | "other" | "none";

export type VinWitnessView = {
  vin_index: number;
  path_kind: WitnessPathKind;
  path_label: string;
  witness_stack: string[] | null;
  witness_hex: string;
  stack_depth: number;
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
};

export type TxProofStatusView = {
  candidate_set_height: number | null;
  winner_status: TxConvergenceRole;
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

export function classifyWitnessPath(coinbase: boolean, stack: string[] | null): WitnessPathKind {
  if (coinbase) return "coinbase";
  if (!stack || stack.length === 0) return "none";
  if (stack.length === 2) return "res_key_path";
  return "oru_input";
}

export function pathLabel(kind: WitnessPathKind): string {
  switch (kind) {
    case "coinbase":
      return "Coinbase";
    case "oru_input":
      return "ORU input witness";
    case "res_key_path":
      return "RES key-path spend";
    case "other":
      return "Witness (not a mint proof)";
    default:
      return "No witness";
  }
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

  return {
    vin_index: vin.vin_index,
    path_kind,
    path_label: pathLabel(path_kind),
    witness_stack: stack,
    witness_hex,
    stack_depth: stack?.length ?? 0,
  };
}

export function buildResMintProofView(
  txid: string,
  resMintRows: Array<Record<string, unknown>>,
  block: { block_hash: string; height: bigint | number; rigor_winner: string; rigor_res_minted: number } | null,
  convergencePanel: TxConvergencePanel,
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
  };

  if (resMintRows.length > 0) {
    const row = resMintRows[0];
    const winnerTxid = String(row.winner_convergence_txid ?? "");
    const blockHash = String(row.block_hash ?? "");
    const blockHeight = Number(row.block_height ?? 0);

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
    };
  }

  if (convergencePanel.role === "Candidate") {
    return {
      ...empty,
      present: false,
      confirmation_status: "Pending candidate state",
      block_hash: convergencePanel.block_hash,
      block_height: convergencePanel.block_height,
    };
  }

  return empty;
}

export function buildTxProofStatus(
  convergencePanel: TxConvergencePanel,
  resMintProof: ResMintProofView,
): TxProofStatusView {
  return {
    candidate_set_height: convergencePanel.block_height ?? resMintProof.block_height,
    winner_status: convergencePanel.role,
  };
}

export function buildTxWitnessPanel(
  txid: string,
  vins: Array<{ vin_index: number; coinbase: boolean; txinwitness: unknown }>,
  resMintRows: Array<Record<string, unknown>>,
  block: { block_hash: string; height: bigint | number; rigor_winner: string; rigor_res_minted: number } | null,
  convergencePanel: TxConvergencePanel,
): TxWitnessPanel {
  const inputs = vins.map(buildVinWitnessView);
  const res_mint_proof = buildResMintProofView(txid, resMintRows, block, convergencePanel);
  const proof_status = buildTxProofStatus(convergencePanel, res_mint_proof);

  return {
    inputs,
    res_mint_proof,
    proof_status,
    rpc_readonly_notice: READONLY_RPC_NOTICE,
  };
}
