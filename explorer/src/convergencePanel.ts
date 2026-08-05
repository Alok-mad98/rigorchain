/** Presentation-only mapping from indexed RPC-imported rows. No local convergence math. */

export const RPC_AUTHORITY_NOTICE =
  "Convergence data is sourced from rigord and is not recomputed by Explorer.";

export type BlockConvergenceStatus =
  | "No Convergence Activity"
  | "Convergence Candidate Block"
  | "Convergence Winner Block";

export type TxConvergenceRole = "Candidate" | "Winner" | "Not Participating";

export type ConvergenceCandidateRow = {
  convergence_txid: string;
  draw_eligible: boolean;
  convergence_root?: string;
};

export type BlockConvergencePanel = {
  status: BlockConvergenceStatus;
  candidate_set_size: number;
  convergence_root: string;
  winner_txid: string | null;
  candidate_txids: string[];
  rpc_authority_notice: string;
};

export type TxConvergencePanel = {
  role: TxConvergenceRole;
  block_height: number | null;
  block_hash: string | null;
  winner_reference: { block_hash: string; block_height: number } | null;
  rpc_authority_notice: string;
};

export function buildBlockConvergencePanel(
  block: { rigor_winner: string; committed_root: string },
  candidates: ConvergenceCandidateRow[],
): BlockConvergencePanel {
  const winner = (block.rigor_winner || "").trim();
  const candidateTxids = candidates.map((c) => c.convergence_txid).sort();
  const status: BlockConvergenceStatus =
    candidateTxids.length === 0
      ? "No Convergence Activity"
      : winner
        ? "Convergence Winner Block"
        : "Convergence Candidate Block";

  const winnerRow = candidates.find((c) => c.convergence_txid === winner);
  const root = winnerRow?.convergence_root ?? block.committed_root;

  return {
    status,
    candidate_set_size: candidateTxids.length,
    convergence_root: root,
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
): TxConvergencePanel {
  if (events.length === 0) {
    return {
      role: "Not Participating",
      block_height: null,
      block_hash: null,
      winner_reference: null,
      rpc_authority_notice: RPC_AUTHORITY_NOTICE,
    };
  }

  const winnerEvt = events.find((e) => e.is_winner);
  if (winnerEvt) {
    const h = Number(winnerEvt.block_height);
    return {
      role: "Winner",
      block_height: h,
      block_hash: winnerEvt.block_hash,
      winner_reference: { block_hash: winnerEvt.block_hash, block_height: h },
      rpc_authority_notice: RPC_AUTHORITY_NOTICE,
    };
  }

  const evt = events[0];
  return {
    role: "Candidate",
    block_height: Number(evt.block_height),
    block_hash: evt.block_hash,
    winner_reference: null,
    rpc_authority_notice: RPC_AUTHORITY_NOTICE,
  };
}
