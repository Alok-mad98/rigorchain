import type { Pool } from "pg";
import type { Config } from "../config";
import { buildCompliancePayload, EQUIVALENCE_CLAIMED } from "../compliance";
import { getOrCreateChainState } from "../db";
import { rpc } from "../rpcCli";

/** FVR-M compliance display — read-only, never computed at runtime. */
export const DRAW_STATUS_DISPLAY = "DRAW: PERMANENTLY IMPOSSIBLE (FVR-M)";

export const RPC_AUTHORITY_DISCLAIMER =
  "All values are authoritative from rigord RPC. No local consensus computation is performed.";

export type StatusDashboard = {
  generated_at: string;
  rpc_authority_disclaimer: string;
  chain_identity: Record<string, unknown>;
  block_health: Record<string, unknown>;
  convergence_health: Record<string, unknown>;
  economic_snapshot: Record<string, unknown>;
  mining_activity: Record<string, unknown>;
  system_integrity: Record<string, unknown>;
};

function atomsToOruDisplay(atoms: string | bigint): string {
  const a = BigInt(String(atoms));
  const base = 100000000n;
  return `${a / base}.${(a % base).toString().padStart(8, "0")} ORU`;
}

function atomsToResDisplay(atoms: string | bigint): string {
  const a = BigInt(String(atoms));
  const base = 100n;
  return `${a / base}.${(a % base).toString().padStart(2, "0")} RES`;
}

/** Average block interval from indexed `blocks.time` (last N rows). */
export function averageBlockTimeSeconds(times: number[]): number | null {
  if (times.length < 2) return null;
  const sorted = [...times].sort((a, b) => a - b);
  let total = 0;
  for (let i = 1; i < sorted.length; i++) total += sorted[i] - sorted[i - 1];
  return total / (sorted.length - 1);
}

/** Histogram of |F| sizes from per-block candidate counts. */
export function fSetDistribution(counts: number[]): Record<string, number> {
  const dist: Record<string, number> = {};
  for (const n of counts) {
    const key = String(n);
    dist[key] = (dist[key] ?? 0) + 1;
  }
  return dist;
}

async function fetchWalletObserverStatus(): Promise<Record<string, unknown>> {
  const base = process.env.WALLET_OBSERVER_URL?.trim();
  if (!base) {
    return { status: "not_configured", message: "WALLET_OBSERVER_URL not set" };
  }
  try {
    const res = await fetch(`${base.replace(/\/$/, "")}/api/observer/chain`, {
      signal: AbortSignal.timeout(5000),
    });
    if (!res.ok) return { status: "unreachable", http_status: res.status };
    const body = (await res.json()) as { ok?: boolean; syncLag?: number | null };
    return {
      status: body.ok ? "ok" : "error",
      sync_lag_blocks: body.syncLag ?? null,
    };
  } catch (e) {
    return { status: "unreachable", error: String(e) };
  }
}

export async function buildStatusDashboard(config: Config, pool: Pool): Promise<StatusDashboard> {
  const chainState = await getOrCreateChainState(pool, config);
  const compliance = await buildCompliancePayload(config, chainState);

  let blockchainInfo: Record<string, unknown> = {};
  let networkInfo: Record<string, unknown> = {};
  let mempoolTxids: string[] = [];
  let rpcError: string | null = null;

  try {
    const [bci, net, mempool] = await Promise.all([
      rpc.getblockchaininfo(config),
      rpc.getnetworkinfo(config),
      rpc.getrawmempool(config),
    ]);
    blockchainInfo = (bci ?? {}) as Record<string, unknown>;
    networkInfo = (net ?? {}) as Record<string, unknown>;
    if (Array.isArray(mempool)) mempoolTxids = mempool as string[];
    else if (mempool && typeof mempool === "object") mempoolTxids = Object.keys(mempool as object);
  } catch (e) {
    rpcError = String(e);
  }

  const nodeHeight = Number(blockchainInfo.blocks ?? -1);
  const indexerHeight = Number(chainState.last_height);

  const blockWindow = await pool.query(
    `
      SELECT height, time, bits, opus_hash, rigor_winner, rigor_res_minted,
             committed_root, computed_root, mismatch, draw_candidate_root
      FROM blocks
      ORDER BY height DESC
      LIMIT 100
    `,
  );

  const blocks = blockWindow.rows;
  const times = blocks.map((b) => Number(b.time)).filter((t) => Number.isFinite(t));
  const avgBlockTime = averageBlockTimeSeconds(times);

  const tipBlock = blocks[0] ?? null;

  const fCounts = await pool.query(
    `
      SELECT b.height, COUNT(ce.convergence_txid)::int AS f_size
      FROM blocks b
      LEFT JOIN convergence_events ce ON ce.block_hash = b.block_hash
      WHERE b.height > GREATEST(0, (SELECT COALESCE(MAX(height), 0) FROM blocks) - 100)
      GROUP BY b.height
      ORDER BY b.height DESC
    `,
  );
  const fSizes = fCounts.rows.map((r) => Number(r.f_size ?? 0));
  const winnerBlocks = blocks.filter((b) => String(b.rigor_winner ?? "").trim() !== "").length;
  const rootChanges = new Set(blocks.map((b) => String(b.committed_root ?? ""))).size;
  const mismatchBlocks = blocks.filter((b) => b.mismatch).length;

  const economic = await pool.query(
    `
      SELECT
        (SELECT COALESCE(SUM(amount_atoms), 0) FROM oru_utxos WHERE spent = FALSE) AS oru_utxo_atoms,
        (SELECT COALESCE(SUM(mint_amount), 0) FROM res_mints) AS res_mint_total_atoms,
        (SELECT COALESCE(SUM(rm.mint_amount), 0)
           FROM res_mints rm
           JOIN blocks b ON b.block_hash = rm.block_hash
          WHERE b.height > GREATEST(0, (SELECT COALESCE(MAX(height), 0) FROM blocks) - 100)
        ) AS res_mint_last_100_atoms
    `,
  );
  const eco = economic.rows[0] ?? {};

  const reorgWindow = await pool.query(
    `
      SELECT COUNT(*)::int AS mismatch_blocks
      FROM blocks
      WHERE mismatch = TRUE
        AND height > GREATEST(0, (SELECT COALESCE(MAX(height), 0) FROM blocks) - 1000)
    `,
  );

  const indexerUpdatedAt = chainState.updated_at instanceof Date ? chainState.updated_at.getTime() : Date.now();
  const indexerLagSeconds = Math.max(0, Math.floor((Date.now() - indexerUpdatedAt) / 1000));

  const walletObserver = await fetchWalletObserverStatus();

  const networkLabel =
    compliance.network === "testnet"
      ? "testnet"
      : compliance.network === "mainnet"
        ? "mainnet"
        : compliance.network;

  return {
    generated_at: new Date().toISOString(),
    rpc_authority_disclaimer: RPC_AUTHORITY_DISCLAIMER,
    chain_identity: {
      chain_name: "RIGOR",
      network: networkLabel,
      network_type: compliance.network_type,
      genesis_hash: compliance.genesis_hash,
      genesis_hash_short: compliance.genesis_hash_short,
      node_subversion: networkInfo.subversion ?? null,
      node_version: networkInfo.version ?? null,
      explorer_version: compliance.explorer_version,
      equivalence_claimed: EQUIVALENCE_CLAIMED,
    },
    block_health: {
      current_height_node: nodeHeight >= 0 ? nodeHeight : null,
      current_height_indexer: indexerHeight >= 0 ? indexerHeight : null,
      best_block_hash: blockchainInfo.bestblockhash ?? chainState.last_block_hash,
      avg_block_time_seconds_last_100: avgBlockTime,
      difficulty: blockchainInfo.difficulty ?? null,
      difficulty_label: "ASERT / OpuHash (node-reported)",
      bits: tipBlock?.bits ?? null,
      opus_hash: tipBlock?.opus_hash ?? null,
      orphan_rate: null,
      orphan_rate_note: "Orphan blocks are not persisted in indexer schema",
      reorg_signal_blocks_mismatch_last_1000: reorgWindow.rows[0]?.mismatch_blocks ?? 0,
      reorg_count_note:
        "Full reorg count is not persisted; mismatch block count shown as integrity signal only",
      initialblockdownload: blockchainInfo.initialblockdownload ?? null,
      verificationprogress: blockchainInfo.verificationprogress ?? null,
    },
    convergence_health: {
      draw_status: DRAW_STATUS_DISPLAY,
      f_set_distribution_last_100_blocks: fSetDistribution(fSizes),
      winner_blocks_last_100: winnerBlocks,
      winner_frequency_last_100: blocks.length > 0 ? winnerBlocks / blocks.length : null,
      convergence_root_stability: {
        distinct_committed_roots_last_100: rootChanges,
        mismatch_blocks_last_100: mismatchBlocks,
        stable: mismatchBlocks === 0,
      },
      source: "indexed convergence_events + blocks (imported from getconvergenceresult)",
    },
    economic_snapshot: {
      oru_indexed_utxo_total_atoms: String(eco.oru_utxo_atoms ?? "0"),
      oru_indexed_utxo_total_display: atomsToOruDisplay(String(eco.oru_utxo_atoms ?? "0")),
      oru_cumulative_convergence_rpc: blockchainInfo.cumulative_convergence ?? null,
      convergence_fee_absorption_window_rpc: blockchainInfo.current_convergence_sum ?? null,
      convergence_fee_label: "convergence-fee absorption (current window, rigord RPC)",
      res_minted_total_atoms: String(eco.res_mint_total_atoms ?? "0"),
      res_minted_total_display: atomsToResDisplay(String(eco.res_mint_total_atoms ?? "0")),
      res_mint_rate_last_100_blocks_atoms: String(eco.res_mint_last_100_atoms ?? "0"),
      res_mint_rate_last_100_blocks_display: atomsToResDisplay(String(eco.res_mint_last_100_atoms ?? "0")),
      source_note: "ORU UTXO sum from indexed oru_utxos; RES from indexed res_mints; convergence fees from getblockchaininfo",
    },
    mining_activity: {
      mempool_tx_count: mempoolTxids.length,
      active_miners: null,
      pending_blocks_active_jobs: null,
      stale_job_count: null,
      availability_note:
        "Mempool from rigord getrawmempool; Stratum/share-validator metrics not wired in explorer MVP",
    },
    system_integrity: {
      rpc_authority: "rigord: authoritative",
      rpc_error: rpcError,
      indexer_sync_lag_blocks: nodeHeight >= 0 && indexerHeight >= 0 ? Math.max(0, nodeHeight - indexerHeight) : null,
      indexer_sync_lag_seconds: indexerLagSeconds,
      indexer_last_updated_at: chainState.updated_at,
      wallet_observer: walletObserver,
      explorer_compliance: {
        frozen: true,
        equivalence_claimed: EQUIVALENCE_CLAIMED,
        mismatch_active: compliance.mismatch_active,
        mismatches: compliance.mismatches,
        mismatch_messages: compliance.mismatch_messages,
      },
    },
  };
}
