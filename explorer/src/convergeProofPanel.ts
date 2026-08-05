/** ConvergeProof visualization panel — indexed display only (TASK-EXPLORER-007). */

import type { TxConvergencePanel } from "./convergencePanel";
import {
  atomsToResDisplay,
  buildPcorbItems,
  type PcorbStackItem,
  type ResMintProofView,
  type TxWitnessPanel,
} from "./witnessPanel";

export const CONVERGEPROOF_PROTOCOL_NOTICE =
  "ConvergeProof is a protocol-native proof chain linking a convergence winner event to RES issuance.";

export const CONVERGEPROOF_AUTHORITY_NOTICE =
  "This explorer displays recorded proof data only. Protocol authority remains rigord.";

export type ConvergeProofPanelKind = "res_mint" | "winner_convergence";

export type PcorbStackItemView = PcorbStackItem & { hex_byte_length: number };

export type ConvergeProofPanel = {
  visible: boolean;
  kind: ConvergeProofPanelKind | null;
  winner_txid: string | null;
  convergence_root: string | null;
  sigma_digest: {
    hex: string | null;
    hex_byte_length: number | null;
  };
  pcorb_witness: PcorbStackItemView[] | null;
  res_mint_linkage: {
    mint_txid: string | null;
    winner_txid: string | null;
    block_hash: string | null;
    block_height: number | null;
    confirmations: number | null;
    mint_vout_index: number | null;
    mint_amount_display: string | null;
  };
  res_mint_outputs: Array<{
    vout_index: number;
    amount_display: string;
    address: string | null;
  }>;
  protocol_notice: string;
  authority_notice: string;
};

function hexByteLength(hex: string | null | undefined): number | null {
  if (!hex) return null;
  const h = hex.replace(/^0x/i, "");
  return h.length > 0 ? Math.floor(h.length / 2) : 0;
}

function withByteLengths(items: PcorbStackItem[] | null): PcorbStackItemView[] | null {
  if (!items) return null;
  return items.map((item) => ({ ...item, hex_byte_length: hexByteLength(item.hex) ?? 0 }));
}

function resOutputsFromVout(
  vouts: Array<{ vout_index: number; asset_kind: string; value_atoms: string | number; address: string | null }>,
): ConvergeProofPanel["res_mint_outputs"] {
  return vouts
    .filter((o) => o.asset_kind === "RES")
    .map((o) => ({
      vout_index: o.vout_index,
      amount_display: atomsToResDisplay(o.value_atoms),
      address: o.address,
    }));
}

export function buildConvergeProofPanel(args: {
  txid: string;
  convergencePanel: TxConvergencePanel;
  convergenceRows: Array<Record<string, unknown>>;
  resMintRows: Array<Record<string, unknown>>;
  linkedMintRows: Array<Record<string, unknown>>;
  witnessPanel: TxWitnessPanel;
  vouts: Array<{ vout_index: number; asset_kind: string; value_atoms: string | number; address: string | null }>;
  block: { block_hash: string; height: bigint | number } | null;
  indexerTipHeight: number;
}): ConvergeProofPanel {
  const empty: ConvergeProofPanel = {
    visible: false,
    kind: null,
    winner_txid: null,
    convergence_root: null,
    sigma_digest: { hex: null, hex_byte_length: null },
    pcorb_witness: null,
    res_mint_linkage: {
      mint_txid: null,
      winner_txid: null,
      block_hash: null,
      block_height: null,
      confirmations: null,
      mint_vout_index: null,
      mint_amount_display: null,
    },
    res_mint_outputs: [],
    protocol_notice: CONVERGEPROOF_PROTOCOL_NOTICE,
    authority_notice: CONVERGEPROOF_AUTHORITY_NOTICE,
  };

  const isResMint = args.resMintRows.length > 0 || args.witnessPanel.res_mint_proof.present;
  const isWinner = args.convergencePanel.role === "Winner";
  if (!isResMint && !isWinner) return empty;

  const kind: ConvergeProofPanelKind = isResMint ? "res_mint" : "winner_convergence";
  const convRow = args.convergenceRows[0] ?? null;
  const sigmaHex = convRow?.selection_digest_h ? String(convRow.selection_digest_h) : null;

  const proof: ResMintProofView = args.witnessPanel.res_mint_proof;
  const pcorbRaw = proof.proof_items ?? args.witnessPanel.inputs.find((i) => i.pcorb_items)?.pcorb_items ?? null;
  const pcorb = withByteLengths(pcorbRaw);

  const convergenceRoot =
    proof.convergence_root ??
    (convRow?.convergence_root ? String(convRow.convergence_root) : null) ??
    args.witnessPanel.proof_status.convergence_root;

  const resMintRow = args.resMintRows[0] ?? args.linkedMintRows[0] ?? null;
  const winnerTxid =
    (resMintRow?.winner_convergence_txid ? String(resMintRow.winner_convergence_txid) : null) ??
    proof.winner_convergence_txid ??
    pcorb?.[0]?.hex ??
    (isWinner ? args.txid : null);

  const mintTxid = args.resMintRows.length > 0 ? args.txid : resMintRow ? String(resMintRow.mint_txid) : null;

  const blockHeight =
    (resMintRow?.block_height != null ? Number(resMintRow.block_height) : null) ??
    args.convergencePanel.block_height ??
    (args.block ? Number(args.block.height) : null);

  const blockHash =
    (resMintRow?.block_hash ? String(resMintRow.block_hash) : null) ??
    args.convergencePanel.block_hash ??
    args.block?.block_hash ??
    null;

  const confirmations =
    blockHeight != null && args.indexerTipHeight >= 0
      ? Math.max(0, args.indexerTipHeight - blockHeight + 1)
      : null;

  const mintVoutIndex = resMintRow?.mint_vout_index != null ? Number(resMintRow.mint_vout_index) : proof.mint_vout_index;
  const mintAmountDisplay =
    resMintRow?.mint_amount != null
      ? atomsToResDisplay(String(resMintRow.mint_amount))
      : proof.mint_amount_display;

  const mintOutputs =
    args.resMintRows.length > 0
      ? resOutputsFromVout(args.vouts)
      : mintTxid && mintTxid === args.txid
        ? resOutputsFromVout(args.vouts)
        : [];

  return {
    visible: true,
    kind,
    winner_txid: winnerTxid,
    convergence_root: convergenceRoot,
    sigma_digest: {
      hex: sigmaHex,
      hex_byte_length: hexByteLength(sigmaHex),
    },
    pcorb_witness: pcorb,
    res_mint_linkage: {
      mint_txid: mintTxid,
      winner_txid: winnerTxid,
      block_hash: blockHash,
      block_height: blockHeight,
      confirmations,
      mint_vout_index: mintVoutIndex,
      mint_amount_display: mintAmountDisplay,
    },
    res_mint_outputs: mintOutputs,
    protocol_notice: CONVERGEPROOF_PROTOCOL_NOTICE,
    authority_notice: CONVERGEPROOF_AUTHORITY_NOTICE,
  };
}

/** Re-export for tests — build PCORB from stack. */
export { buildPcorbItems };
