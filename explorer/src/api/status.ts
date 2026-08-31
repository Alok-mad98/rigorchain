import type { Pool } from "pg";
import type { Config } from "../config";
import { buildCompliancePayload, EQUIVALENCE_CLAIMED } from "../compliance";
import { getOrCreateChainState } from "../db";
import { CIRCULATING_ORU_SQL_PREDICATE } from "../circulating";
import { atomsToOruDisplay, atomsToResDisplay } from "../amount";
import { rpc } from "../rpcCli";
import { getRfc018FeeBurnTip } from "../rfc018FeeBurnStore";
import { rfc018FeeBurnView } from "../rfc018FeeBurn";

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

/** getblockchaininfo amounts are ValueFromAmount decimals; getconvergencestats ORU is integer atoms. */
function formatOruAmount(value: unknown, asAtoms = false): string | null {
  if (value == null || value === "") return null;
  if (asAtoms) {
    try {
      return atomsToOruDisplay(String(Math.trunc(Number(value))));
    } catch {
      return null;
    }
  }
  const n = typeof value === "number" ? value : Number(value);
  if (!Number.isFinite(n)) return null;
  return `${n.toFixed(8)} ORU`;
}

function formatResAtoms(value: unknown): string | null {
  if (value == null || value === "") return null;
  try {
    return atomsToResDisplay(String(Math.trunc(Number(value))));
  } catch {
    return null;
  }
}

/** Display-only copy of consensus coinbase maturity. Not a consensus rule. */
export const COINBASE_MATURITY_DISPLAY = 100;

/** Coinbase is transferable when confirmations >= COINBASE_MATURITY (tip - height + 1). */
export function isCoinbaseMature(tipHeight: number, outputHeight: number, maturity = COINBASE_MATURITY_DISPLAY): boolean {
  if (!Number.isInteger(tipHeight) || !Number.isInteger(outputHeight)) return false;
  if (tipHeight < 0 || outputHeight < 0 || outputHeight > tipHeight) return false;
  return tipHeight - outputHeight + 1 >= maturity;
}

/** Average block interval from indexed `blocks.time` (last N rows). */
export function averageBlockTimeSeconds(times: number[]): number | null {
  if (times.length < 2) return null;
  const sorted = [...times].sort((a, b) => a - b);
  let total = 0;
  for (let i = 1; i < sorted.length; i++) total += sorted[i] - sorted[i - 1];
  return total / (sorted.length - 1);
}

/** Blocks until the newest candidate ages out of the sliding draw window. Display only. */
export function drawWindowRemaining(args: {
  tipHeight: number;
  windowSize: number;
  sourceHeights: number[];
}): { remaining: number | null; completesAt: number | null } {
  if (!Number.isFinite(args.tipHeight) || args.tipHeight < 0) {
    return { remaining: null, completesAt: null };
  }
  if (!Number.isFinite(args.windowSize) || args.windowSize <= 0 || args.sourceHeights.length === 0) {
    return { remaining: null, completesAt: null };
  }
  const completesAt = Math.max(...args.sourceHeights.map((s) => Number(s) + args.windowSize - 1));
  if (!Number.isFinite(completesAt)) return { remaining: null, completesAt: null };
  return { remaining: Math.max(0, completesAt - args.tipHeight), completesAt };
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
  let convStats: Record<string, unknown> = {};
  let convStatus: Record<string, unknown> = {};
  let runtimeSupply: Record<string, unknown> = {};
  let mempoolTxids: string[] = [];
  let rpcError: string | null = null;

  try {
    const [bci, net, mempool, stats, status, supply] = await Promise.all([
      rpc.getblockchaininfo(config),
      rpc.getnetworkinfo(config),
      rpc.getrawmempool(config),
      rpc.getconvergencestats(config).catch(() => ({})),
      rpc.getconvergencestatus(config).catch(() => ({})),
      rpc.getruntimesupply(config).catch(() => ({})),
    ]);
    blockchainInfo = (bci ?? {}) as Record<string, unknown>;
    networkInfo = (net ?? {}) as Record<string, unknown>;
    convStats = (stats ?? {}) as Record<string, unknown>;
    convStatus = (status ?? {}) as Record<string, unknown>;
    runtimeSupply = (supply ?? {}) as Record<string, unknown>;
    if (Array.isArray(mempool)) mempoolTxids = mempool as string[];
    else if (mempool && typeof mempool === "object") mempoolTxids = Object.keys(mempool as object);
  } catch (e) {
    rpcError = String(e);
  }

  const nodeHeight = Number(blockchainInfo.blocks ?? -1);
  const indexerHeight = Number(chainState.last_height);

  let drawRemaining: { remaining: number | null; completesAt: number | null } = {
    remaining: null,
    completesAt: null,
  };
  let tipCRoot: string | null = null;
  let tipDrawCommitted: string | null = null;
  let tipDrawComputed: string | null = null;
  try {
    const tipKey = nodeHeight >= 0 ? String(nodeHeight) : "";
    const draw = tipKey ? await rpc.getconvergenceresult(config, tipKey) : {};
    const cands = Array.isArray(draw?.candidates) ? draw.candidates : [];
    tipCRoot = typeof draw?.convergence_root === "string" ? draw.convergence_root : null;
    tipDrawCommitted =
      typeof draw?.draw_candidate_root_committed === "string"
        ? draw.draw_candidate_root_committed
        : null;
    tipDrawComputed =
      typeof draw?.draw_candidate_root_computed === "string"
        ? draw.draw_candidate_root_computed
        : null;
    drawRemaining = drawWindowRemaining({
      tipHeight: Number(draw?.height ?? nodeHeight),
      windowSize: Number(draw?.window_size ?? 0),
      sourceHeights: cands.map((c: { source_height?: number }) => Number(c.source_height)).filter((n: number) => Number.isFinite(n)),
    });
  } catch {
    /* leave null — page shows — */
  }

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
        (SELECT COALESCE(SUM(amount_atoms), 0) FROM oru_utxos WHERE ${CIRCULATING_ORU_SQL_PREDICATE}) AS oru_utxo_atoms,
        (SELECT COUNT(*)::int FROM oru_utxos WHERE ${CIRCULATING_ORU_SQL_PREDICATE}) AS oru_utxo_count,
        (SELECT COALESCE(SUM(u.amount_atoms), 0)
           FROM oru_utxos u
           JOIN outputs o ON o.txid = u.txid AND o.vout_index = u.vout_index
           JOIN transactions t ON t.txid = u.txid
          WHERE u.spent = FALSE
            AND COALESCE(o.witness_version, 0) = 0
            AND (
              t.block_tx_index <> 0
              OR ((SELECT COALESCE(MAX(height), 0) FROM blocks) - u.height + 1) >= 100
            )
        ) AS oru_mature_transferable_atoms,
        (SELECT COUNT(*)::int
           FROM oru_utxos u
           JOIN outputs o ON o.txid = u.txid AND o.vout_index = u.vout_index
           JOIN transactions t ON t.txid = u.txid
          WHERE u.spent = FALSE
            AND COALESCE(o.witness_version, 0) = 0
            AND (
              t.block_tx_index <> 0
              OR ((SELECT COALESCE(MAX(height), 0) FROM blocks) - u.height + 1) >= 100
            )
        ) AS oru_mature_transferable_count,
        (SELECT COALESCE(SUM(u.amount_atoms), 0)
           FROM oru_utxos u
           JOIN outputs o ON o.txid = u.txid AND o.vout_index = u.vout_index
           JOIN transactions t ON t.txid = u.txid
          WHERE u.spent = FALSE
            AND COALESCE(o.witness_version, 0) = 0
            AND t.block_tx_index = 0
            AND ((SELECT COALESCE(MAX(height), 0) FROM blocks) - u.height + 1) < 100
        ) AS oru_immature_coinbase_atoms,
        (SELECT COALESCE(SUM(o.value_atoms), 0)
           FROM transactions t
           JOIN outputs o ON o.txid = t.txid AND o.asset_kind = 'ORU'
          WHERE t.block_tx_index = 0) AS oru_coinbase_total_atoms,
        (SELECT COALESCE(SUM(o.value_atoms), 0)
           FROM transactions t
           JOIN outputs o ON o.txid = t.txid AND o.asset_kind = 'ORU'
          WHERE t.block_tx_index = 0
            AND t.block_hash = (SELECT block_hash FROM blocks ORDER BY height DESC LIMIT 1)
        ) AS oru_subsidy_tip_atoms,
        (SELECT COUNT(*)::int FROM convergence_events) AS convergence_event_count,
        (SELECT COUNT(*)::int FROM convergence_events WHERE draw_eligible) AS convergence_eligible_count,
        (SELECT COALESCE(SUM(mint_amount), 0) FROM res_mints) AS res_mint_total_atoms,
        (SELECT COUNT(*)::int FROM res_mints) AS res_mint_count,
        (SELECT COALESCE(SUM(rm.mint_amount), 0)
           FROM res_mints rm
           JOIN blocks b ON b.block_hash = rm.block_hash
          WHERE b.height > GREATEST(0, (SELECT COALESCE(MAX(height), 0) FROM blocks) - 100)
        ) AS res_mint_last_100_atoms,
        (SELECT MAX(block_height) FROM res_mints) AS last_res_mint_height,
        (SELECT mint_txid FROM res_mints ORDER BY block_height DESC, mint_vout_index ASC LIMIT 1) AS last_res_mint_txid,
        (SELECT winner_convergence_txid FROM res_mints ORDER BY block_height DESC, mint_vout_index ASC LIMIT 1) AS last_winner_convergence_txid
    `,
  );
  const eco = economic.rows[0] ?? {};
  let feeBurnTip: Awaited<ReturnType<typeof getRfc018FeeBurnTip>> = null;
  try {
    feeBurnTip = await getRfc018FeeBurnTip(pool);
  } catch {
    feeBurnTip = null;
  }
  const rfc018FeeBurn = {
    ...rfc018FeeBurnView(
      feeBurnTip?.burned_oru ?? 0n,
      feeBurnTip?.cumulative_burned_oru ?? 0n,
      atomsToOruDisplay,
    ),
    indexed: feeBurnTip != null,
    height: feeBurnTip != null ? feeBurnTip.height.toString() : null,
  };
  const totalIssuedDisplay =
    formatOruAmount(runtimeSupply.oru_subsidy_cumulative_derived) ??
    formatOruAmount(runtimeSupply.oru_supply_cumulative);
  const circulatingDisplay = atomsToOruDisplay(String(eco.oru_utxo_atoms ?? "0"));
  const convergenceDestroyedDisplay = formatOruAmount(blockchainInfo.cumulative_convergence);

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
      chain_name: compliance.chain_name,
      product: compliance.product,
      codename: compliance.codename,
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
      c_root: tipCRoot,
      draw_candidate_root_committed: tipDrawCommitted,
      draw_candidate_root_computed: tipDrawComputed,
      draw_candidate_root: tipDrawCommitted,
      f_set_distribution_last_100_blocks: fSetDistribution(fSizes),
      winner_blocks_last_100: winnerBlocks,
      winner_frequency_last_100: blocks.length > 0 ? winnerBlocks / blocks.length : null,
      convergence_root_stability: {
        distinct_committed_roots_last_100: rootChanges,
        mismatch_blocks_last_100: mismatchBlocks,
        stable: mismatchBlocks === 0,
      },
      active_convergences: convStats.active_convergences ?? null,
      pending_windows: convStats.pending_windows ?? null,
      source: "indexed convergence_events + getconvergencestats",
    },
    economic_snapshot: {
      tip_height: nodeHeight >= 0 ? nodeHeight : indexerHeight,
      network_res_minted_display:
        formatResAtoms(convStats.total_res_minted) ?? atomsToResDisplay(String(eco.res_mint_total_atoms ?? "0")),
      oru_burned_lifetime_display: convergenceDestroyedDisplay,
      cumulative_convergence_display: convergenceDestroyedDisplay,
      rfc018_fee_burn: rfc018FeeBurn,
      supply_chart: {
        total_issued_oru: {
          display: totalIssuedDisplay,
          source: "getruntimesupply.oru_subsidy_cumulative_derived (Σ R); ledger S_ORU is not this series",
        },
        cumulative_convergence: {
          display: convergenceDestroyedDisplay,
          rpc: blockchainInfo.cumulative_convergence ?? null,
          source: "getblockchaininfo.cumulative_convergence (Convergence destroyed ORU)",
        },
        rfc018_fee_burn: {
          block_burned_oru: rfc018FeeBurn.block_burned_oru,
          cumulative_burned_oru: rfc018FeeBurn.cumulative_burned_oru,
          block_burned_oru_display: rfc018FeeBurn.block_burned_oru_display,
          cumulative_burned_oru_display: rfc018FeeBurn.cumulative_burned_oru_display,
          source: "explorer rfc018_fee_burn_stats (not consensus)",
        },
        oru_circulating_spendable: {
          atoms: String(eco.oru_utxo_atoms ?? "0"),
          display: circulatingDisplay,
          source: "indexed oru_utxos with CIRCULATING_ORU_SQL_PREDICATE (OP_CONVERGE excluded)",
        },
        never_merged: true,
        note: "Total issued, Convergence destroyed, Deflationary Fee Burn, and circulating spendable are four separate series. Do not add or subtract them into one explorer figure.",
        equivalence_claimed: EQUIVALENCE_CLAIMED,
      },
      oru_in_current_window_display: formatOruAmount(convStats.active_oru, true),
      oru_since_last_mint_display: formatOruAmount(blockchainInfo.current_convergence_sum),
      oru_indexed_utxo_total_display: atomsToOruDisplay(String(eco.oru_utxo_atoms ?? "0")),
      oru_circulating_spendable_display: atomsToOruDisplay(String(eco.oru_utxo_atoms ?? "0")),
      oru_utxo_count: eco.oru_utxo_count ?? 0,
      oru_mature_transferable_display: atomsToOruDisplay(String(eco.oru_mature_transferable_atoms ?? "0")),
      oru_mature_transferable_count: eco.oru_mature_transferable_count ?? 0,
      oru_immature_coinbase_display: atomsToOruDisplay(String(eco.oru_immature_coinbase_atoms ?? "0")),
      oru_coinbase_total_display: atomsToOruDisplay(String(eco.oru_coinbase_total_atoms ?? "0")),
      oru_subsidy_tip_display:
        formatOruAmount(runtimeSupply.oru_subsidy_block) ?? atomsToOruDisplay(String(eco.oru_subsidy_tip_atoms ?? "0")),
      oru_supply_cumulative_display: formatOruAmount(runtimeSupply.oru_supply_cumulative),
      oru_supply_parent_display: formatOruAmount(runtimeSupply.oru_supply_parent),
      oru_burn_block_display: formatOruAmount(runtimeSupply.oru_converge_burn_block),
      convergence_event_count: eco.convergence_event_count ?? 0,
      convergence_eligible_count: eco.convergence_eligible_count ?? 0,
      active_convergences: convStats.active_convergences ?? null,
      pending_windows: convStats.pending_windows ?? null,
      res_mint_rate_last_100_blocks_display: atomsToResDisplay(String(eco.res_mint_last_100_atoms ?? "0")),
      res_mint_count: eco.res_mint_count ?? 0,
      last_res_height: convStatus.last_res_height ?? eco.last_res_mint_height ?? null,
      last_res_mint_txid: eco.last_res_mint_txid ?? null,
      last_winner_txid: convStatus.last_winner_txid ?? eco.last_winner_convergence_txid ?? null,
      window_progress: convStatus.window_progress ?? null,
      window_progress_percent: convStatus.window_progress_percent ?? null,
      draw_blocks_remaining: drawRemaining.remaining,
      draw_completes_at_height: drawRemaining.completesAt,
      oru_indexed_utxo_total_atoms: String(eco.oru_utxo_atoms ?? "0"),
      oru_cumulative_convergence_rpc: blockchainInfo.cumulative_convergence ?? null,
      convergence_fee_absorption_window_rpc: blockchainInfo.current_convergence_sum ?? null,
      res_minted_total_atoms: String(eco.res_mint_total_atoms ?? "0"),
      res_minted_total_display: atomsToResDisplay(String(eco.res_mint_total_atoms ?? "0")),
      res_mint_rate_last_100_blocks_atoms: String(eco.res_mint_last_100_atoms ?? "0"),
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
        rpc_degraded: compliance.rpc_degraded,
        rpc_messages: compliance.rpc_messages,
      },
    },
  };
}
