/** Convergence lineage graph — indexed presentation only (TASK-EXPLORER-009). */

import { buildBlockConvergencePanel, type BlockConvergencePanel } from "./convergencePanel";
import type { ConvergeProofPanel } from "./convergeProofPanel";
import type { ConvergenceCandidateRow } from "./convergencePanel";
import { atomsToResDisplay } from "./witnessPanel";
import type { PcorbStackItem } from "./witnessPanel";

export const LINEAGE_READONLY_NOTICE =
  "Relationship data is derived from indexed rigord data. No lineage relationships are computed in the browser.";

export const LINEAGE_RPC_NOTICE =
  "Lineage fields mirror rigord-imported convergence_events and res_mints rows. Explorer does not compute eligibility or winners.";

export type LineageVisualState =
  | "Candidate"
  | "Winner"
  | "Minted"
  | "Confirmed"
  | "Orphaned"
  | "Revoked";

export type LineageNodeKind =
  | "candidate"
  | "candidate_set"
  | "winner"
  | "convergence_root"
  | "sigma_digest"
  | "pcorb"
  | "res_mint"
  | "block";

export type LineageGraphNode = {
  id: string;
  kind: LineageNodeKind;
  label: string;
  state: LineageVisualState;
  href: string | null;
  value: string | null;
  meta?: Record<string, unknown>;
};

export type LineageGraphEdge = {
  from: string;
  to: string;
};

export type LineageCandidateEntry = {
  txid: string;
  state: LineageVisualState;
  draw_eligible: boolean;
  href: string;
};

export type LineageGraph = {
  visible: boolean;
  anchor_txid: string | null;
  f_set_size: number;
  candidate_set: LineageCandidateEntry[];
  winner: {
    txid: string | null;
    state: LineageVisualState;
    href: string | null;
  };
  convergence_root: string | null;
  sigma: {
    selection_digest_h: string | null;
  };
  pcorb: {
    present: boolean;
    item_count: number;
    items: Array<PcorbStackItem & { hex_byte_length: number }> | null;
  };
  mint: {
    mint_txid: string | null;
    mint_height: number | null;
    state: LineageVisualState;
    mint_amount_display: string | null;
    href: string | null;
    res_outputs: Array<{
      address: string | null;
      amount_display: string;
      address_href: string | null;
    }>;
  };
  block_context: {
    block_hash: string | null;
    block_height: number | null;
    winner_txid: string | null;
    rigor_res_minted: number | null;
    block_href: string | null;
    mismatch: boolean;
    orphan_display_state: LineageVisualState | null;
  };
  nodes: LineageGraphNode[];
  edges: LineageGraphEdge[];
  legend: Array<{ kind: string; description: string }>;
  readonly_notice: string;
  rpc_authority_notice: string;
  equivalence_claimed: false;
};

export const LINEAGE_LEGEND: LineageGraph["legend"] = [
  { kind: "Candidate", description: "Indexed convergence candidate in |F|" },
  { kind: "Winner", description: "rigor_winner for the block (rigord authority)" },
  { kind: "Root", description: "C (convergence_root) from getconvergenceresult / indexed c_root" },
  { kind: "Sigma", description: "selection_digest_h (sigma digest) on winner row" },
  { kind: "PCORB", description: "Retired body-witness mint proof (not live issuance)" },
  { kind: "Mint", description: "Coinbase OP_RES_MINT output paying the parent-draw winner" },
];

function txHref(txid: string): string {
  return `/tx/${encodeURIComponent(txid)}`;
}

function blockHref(hash: string): string {
  return `/block/${encodeURIComponent(hash)}`;
}

function addressHref(address: string): string {
  return `/address/${encodeURIComponent(address)}`;
}

export function emptyLineageGraph(anchor: string | null = null): LineageGraph {
  return {
    visible: false,
    anchor_txid: anchor,
    f_set_size: 0,
    candidate_set: [],
    winner: { txid: null, state: "Candidate", href: null },
    convergence_root: null,
    sigma: { selection_digest_h: null },
    pcorb: { present: false, item_count: 0, items: null },
    mint: {
      mint_txid: null,
      mint_height: null,
      state: "Candidate",
      mint_amount_display: null,
      href: null,
      res_outputs: [],
    },
    block_context: {
      block_hash: null,
      block_height: null,
      winner_txid: null,
      rigor_res_minted: null,
      block_href: null,
      mismatch: false,
      orphan_display_state: null,
    },
    nodes: [],
    edges: [],
    legend: LINEAGE_LEGEND,
    readonly_notice: LINEAGE_READONLY_NOTICE,
    rpc_authority_notice: LINEAGE_RPC_NOTICE,
    equivalence_claimed: false,
  };
}

export function buildLineageGraph(args: {
  anchorTxid: string;
  blockPanel: BlockConvergencePanel | null;
  candidates: ConvergenceCandidateRow[];
  convergeProof: ConvergeProofPanel;
  block: {
    block_hash: string;
    height: bigint | number;
    rigor_winner: string;
    rigor_res_minted: number;
    mismatch?: boolean;
  } | null;
  winnerConvergenceRow: Record<string, unknown> | null;
  mintResOutputs: Array<{ address: string | null; value_atoms: string | number }>;
}): LineageGraph {
  const winnerTxid = (args.block?.rigor_winner || args.blockPanel?.winner_txid || "").trim() || null;
  const candidateTxids = (
    args.blockPanel?.candidate_txids ?? args.candidates.map((c) => c.convergence_txid)
  ).sort();
  const fSetSize = args.blockPanel?.candidate_set_size ?? candidateTxids.length;

  const participates =
    candidateTxids.includes(args.anchorTxid) ||
    args.convergeProof.visible ||
    winnerTxid === args.anchorTxid;

  if (!participates && fSetSize === 0) {
    return emptyLineageGraph(args.anchorTxid);
  }

  const root =
    args.blockPanel?.c_root ??
    args.blockPanel?.convergence_root ??
    args.convergeProof.convergence_root ??
    (args.winnerConvergenceRow?.convergence_root ? String(args.winnerConvergenceRow.convergence_root) : null);

  const sigmaHex =
    args.convergeProof.sigma_digest?.hex ??
    (args.winnerConvergenceRow?.selection_digest_h
      ? String(args.winnerConvergenceRow.selection_digest_h)
      : null);

  const mintTxid = args.convergeProof.res_mint_linkage.mint_txid;
  const mintHeight = args.convergeProof.res_mint_linkage.block_height;
  const mintAmount = args.convergeProof.res_mint_linkage.mint_amount_display;

  const blockHash = args.block?.block_hash ?? args.convergeProof.res_mint_linkage.block_hash;
  const blockHeight =
    args.block != null
      ? Number(args.block.height)
      : args.convergeProof.res_mint_linkage.block_height;

  const mismatch = Boolean(args.block?.mismatch);
  const orphanState: LineageVisualState | null = mismatch ? "Orphaned" : null;

  const mintState: LineageVisualState = mintTxid
    ? mintHeight != null && blockHeight != null
      ? "Confirmed"
      : "Minted"
    : "Candidate";

  const candidateSet: LineageCandidateEntry[] = candidateTxids.map((txid) => ({
    txid,
    state: txid === winnerTxid ? "Winner" : "Candidate",
    draw_eligible: args.candidates.find((c) => c.convergence_txid === txid)?.draw_eligible ?? true,
    href: txHref(txid),
  }));

  const resOutputs =
    args.convergeProof.res_mint_outputs.length > 0
      ? args.convergeProof.res_mint_outputs.map((o) => ({
          address: o.address,
          amount_display: o.amount_display,
          address_href: o.address ? addressHref(o.address) : null,
        }))
      : args.mintResOutputs.map((o) => ({
          address: o.address,
          amount_display: atomsToResDisplay(o.value_atoms),
          address_href: o.address ? addressHref(o.address) : null,
        }));

  const nodes: LineageGraphNode[] = [];
  const edges: LineageGraphEdge[] = [];

  if (fSetSize > 0) {
    nodes.push({
      id: "candidate_set",
      kind: "candidate_set",
      label: `|F| = ${fSetSize}`,
      state: "Candidate",
      href: blockHash ? blockHref(blockHash) : null,
      value: String(fSetSize),
      meta: { f_set_size: fSetSize },
    });
  }

  for (const c of candidateSet) {
    const id = `candidate:${c.txid}`;
    nodes.push({
      id,
      kind: "candidate",
      label: "Candidate Transaction",
      state: c.state,
      href: c.href,
      value: c.txid,
      meta: { draw_eligible: c.draw_eligible },
    });
    if (fSetSize > 0) {
      edges.push({ from: "candidate_set", to: id });
    }
    if (winnerTxid) {
      edges.push({ from: id, to: `winner:${winnerTxid}` });
    }
  }

  if (winnerTxid) {
    const winnerId = `winner:${winnerTxid}`;
    nodes.push({
      id: winnerId,
      kind: "winner",
      label: "Winner Transaction",
      state: "Winner",
      href: txHref(winnerTxid),
      value: winnerTxid,
    });

    const rootId = "convergence_root";
    nodes.push({
      id: rootId,
      kind: "convergence_root",
      label: "C (convergence_root)",
      state: "Winner",
      href: blockHash ? blockHref(blockHash) : null,
      value: root,
    });
    edges.push({ from: winnerId, to: rootId });

    const sigmaId = "sigma_digest";
    nodes.push({
      id: sigmaId,
      kind: "sigma_digest",
      label: "Sigma Digest",
      state: "Winner",
      href: null,
      value: sigmaHex,
      meta: { selection_digest_h: sigmaHex },
    });
    edges.push({ from: rootId, to: sigmaId });

    if (mintTxid) {
      const mintId = `mint:${mintTxid}`;
      nodes.push({
        id: mintId,
        kind: "res_mint",
        label: "Coinbase OP_RES_MINT",
        state: mintState,
        href: txHref(mintTxid),
        value: mintTxid,
        meta: { mint_height: mintHeight, mint_amount_display: mintAmount },
      });
      edges.push({ from: sigmaId, to: mintId });
    }
  }

  if (blockHash) {
    nodes.push({
      id: "block_context",
      kind: "block",
      label: "Block Context",
      state: orphanState ?? "Confirmed",
      href: blockHref(blockHash),
      value: blockHash,
      meta: { block_height: blockHeight, rigor_res_minted: args.block?.rigor_res_minted ?? null },
    });
  }

  return {
    visible: true,
    anchor_txid: args.anchorTxid,
    f_set_size: fSetSize,
    candidate_set: candidateSet,
    winner: {
      txid: winnerTxid,
      state: winnerTxid ? "Winner" : "Candidate",
      href: winnerTxid ? txHref(winnerTxid) : null,
    },
    convergence_root: root,
    sigma: { selection_digest_h: sigmaHex },
    pcorb: {
      present: false,
      item_count: 0,
      items: null,
    },
    mint: {
      mint_txid: mintTxid,
      mint_height: mintHeight,
      state: mintState,
      mint_amount_display: mintAmount,
      href: mintTxid ? txHref(mintTxid) : null,
      res_outputs: resOutputs,
    },
    block_context: {
      block_hash: blockHash,
      block_height: blockHeight ?? null,
      winner_txid: winnerTxid,
      rigor_res_minted: args.block?.rigor_res_minted ?? null,
      block_href: blockHash ? blockHref(blockHash) : null,
      mismatch,
      orphan_display_state: orphanState,
    },
    nodes,
    edges,
    legend: LINEAGE_LEGEND,
    readonly_notice: LINEAGE_READONLY_NOTICE,
    rpc_authority_notice: LINEAGE_RPC_NOTICE,
    equivalence_claimed: false,
  };
}

/** Block-anchored lineage (uses winner tx as proof chain anchor). */
export function buildBlockLineageGraph(
  blockHash: string,
  block: {
    rigor_winner: string;
    rigor_res_minted: number;
    height: bigint | number;
    mismatch?: boolean;
    committed_root: string;
    draw_candidate_root?: string;
    c_root?: string | null;
  },
  candidates: ConvergenceCandidateRow[],
  convergeProof: ConvergeProofPanel,
  winnerRow: Record<string, unknown> | null,
  mintResOutputs: Array<{ address: string | null; value_atoms: string | number }>,
): LineageGraph {
  const winnerTxid = (block.rigor_winner || "").trim();
  const anchor = winnerTxid || candidates[0]?.convergence_txid || blockHash;
  const blockPanel = buildBlockConvergencePanel(
    {
      rigor_winner: winnerTxid,
      committed_root: block.committed_root,
      draw_candidate_root: block.draw_candidate_root,
      c_root: block.c_root ?? null,
    },
    candidates,
  );

  return buildLineageGraph({
    anchorTxid: anchor,
    blockPanel,
    candidates,
    convergeProof,
    block: {
      block_hash: blockHash,
      height: block.height,
      rigor_winner: block.rigor_winner,
      rigor_res_minted: block.rigor_res_minted,
      mismatch: block.mismatch,
    },
    winnerConvergenceRow: winnerRow,
    mintResOutputs,
  });
}