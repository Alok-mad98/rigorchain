/** Presentation-only mapping from indexed RPC-imported rows. No local convergence math. */

import { EMPTY_DRAW_ELIGIBLE_ROOT } from "./rpcFields";

export const RPC_AUTHORITY_NOTICE =
  "Convergence data is sourced from rigord and is not recomputed by Explorer.";

/** Legacy alias: coinbase committed root (this block PAYS). Prefer the split labels. */
export const DRAW_ROOT_LABEL = "draw_candidate_root_committed";
/** Coinbase CVGE — the draw this block pays (parent computed). */
export const DRAW_ROOT_COMMITTED_LABEL = "draw_candidate_root_committed";
/** Eligible-set root this block awards; height+1 must commit it. */
export const DRAW_ROOT_COMPUTED_LABEL = "draw_candidate_root_computed";

export function emptyEligibleRootNote(hex: string | null | undefined): string | null {
  const s = String(hex ?? "")
    .toLowerCase()
    .replace(/^0x/, "");
  if (!s) return null;
  if (s === EMPTY_DRAW_ELIGIBLE_ROOT) return "empty eligible set";
  if (/^0+$/.test(s)) return "zero";
  return null;
}

export type BlockConvergenceStatus =
  | "No Convergence Activity"
  | "Convergence Candidate Block"
  | "Convergence Winner Block";

export type TxConvergenceRole = "Candidate" | "Winner" | "Not Participating";

/** RFC-020 display only. API / index still use Candidate / Winner. */
export function displayBlockConvergenceStatus(status: BlockConvergenceStatus): string {
  if (status === "Convergence Winner Block") return "RES Mint Block";
  if (status === "Convergence Candidate Block") return "Convergence Destroy Block";
  return status;
}

export function displayTxConvergenceRole(role: TxConvergenceRole): string {
  if (role === "Winner") return "Mint";
  if (role === "Candidate") return "Destroy";
  return role;
}

export type ConvergenceCandidateRow = {
  convergence_txid: string;
  draw_eligible: boolean;
};

export type BlockConvergencePanel = {
  status: BlockConvergenceStatus;
  candidate_set_size: number;
  eligible_count: number;
  /** Coinbase committed root — draw this block PAYS. */
  draw_candidate_root_committed: string;
  /** Draw this block AWARDS (must be committed at height+1). */
  draw_candidate_root_computed: string;
  /** Alias of draw_candidate_root_committed for older clients. */
  draw_candidate_root: string;
  winner_txid: string | null;
  candidate_txids: string[];
  rpc_authority_notice: string;
};

export type TxConvergencePanel = {
  role: TxConvergenceRole;
  /** Inclusion / award height shown in the UI (not the latest window refresh). */
  block_height: number | null;
  block_hash: string | null;
  /** Latest indexed F-set block that still lists this candidate. */
  window_last_height: number | null;
  winner_reference: { block_hash: string; block_height: number } | null;
  rpc_authority_notice: string;
};

export function buildBlockConvergencePanel(
  block: {
    rigor_winner: string;
    rigor_res_minted?: number | string | null;
    mint_txid?: string | null;
    committed_root?: string;
    computed_root?: string;
    draw_candidate_root?: string;
  },
  candidates: ConvergenceCandidateRow[],
): BlockConvergencePanel {
  const winner = (block.mint_txid || block.rigor_winner || "").trim();
  const minted = Number(block.rigor_res_minted ?? 0) > 0 || Boolean(winner);
  const candidateTxids = [...new Set(candidates.map((c) => c.convergence_txid))].sort();
  const eligibleCount = candidates.filter((c) => c.draw_eligible).length;
  const status: BlockConvergenceStatus =
    candidateTxids.length === 0 && !minted
      ? "No Convergence Activity"
      : minted
        ? "Convergence Winner Block"
        : "Convergence Candidate Block";

  const committed = (block.committed_root || block.draw_candidate_root || "").trim();
  const computed = (block.computed_root || "").trim();

  return {
    status,
    candidate_set_size: candidateTxids.length,
    eligible_count: eligibleCount,
    draw_candidate_root_committed: committed,
    draw_candidate_root_computed: computed,
    draw_candidate_root: committed,
    winner_txid: winner || null,
    candidate_txids: candidateTxids,
    rpc_authority_notice: RPC_AUTHORITY_NOTICE,
  };
}

export function buildTxConvergencePanel(
  events: Array<{
    block_hash: string;
    block_height: bigint | number;
    is_winner: boolean;
  }>,
  inclusion?: { block_hash?: string | null; block_height?: bigint | number | null } | null,
  opts?: { isResMint?: boolean },
): TxConvergencePanel {
  const minted = Boolean(opts?.isResMint) || events.some((e) => e.is_winner);
  if (events.length === 0 && !minted) {
    return {
      role: "Not Participating",
      block_height: null,
      block_hash: null,
      window_last_height: null,
      winner_reference: null,
      rpc_authority_notice: RPC_AUTHORITY_NOTICE,
    };
  }

  const heights = events.map((e) => Number(e.block_height)).filter((n) => Number.isFinite(n));
  const windowLast = heights.length ? Math.max(...heights) : null;
  const inclusionHeight =
    inclusion?.block_height != null
      ? Number(inclusion.block_height)
      : heights.length
        ? Math.min(...heights)
        : null;
  const inclusionHash =
    (inclusion?.block_hash || "").trim() ||
    events.find((e) => Number(e.block_height) === inclusionHeight)?.block_hash ||
    null;

  const winnerEvt = events.find((e) => e.is_winner);
  if (minted) {
    const h = winnerEvt != null ? Number(winnerEvt.block_height) : inclusionHeight;
    const hash = winnerEvt?.block_hash ?? inclusionHash;
    return {
      role: "Winner",
      block_height: h,
      block_hash: hash,
      window_last_height: windowLast,
      winner_reference:
        hash != null && h != null ? { block_hash: hash, block_height: h } : null,
      rpc_authority_notice: RPC_AUTHORITY_NOTICE,
    };
  }

  return {
    role: "Candidate",
    block_height: inclusionHeight,
    block_hash: inclusionHash,
    window_last_height: windowLast,
    winner_reference: null,
    rpc_authority_notice: RPC_AUTHORITY_NOTICE,
  };
}
