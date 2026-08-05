import { Pool } from "pg";
import type { Config } from "./config";
import { buildBlockConvergencePanel, buildTxConvergencePanel } from "./convergencePanel";
import { buildConvergeProofPanel } from "./convergeProofPanel";
import { getLineageForBlock, getLineageForTxid } from "./lineageDb";
import { buildTxWitnessPanel } from "./witnessPanel";

export type ChainStateRow = {
  indexer_name: string;
  network: string;
  last_height: bigint;
  last_block_hash: string | null;
  critical_root_mismatch: boolean;
  updated_at: Date;
};

export function createPool(config: Config): Pool {
  return new Pool({
    connectionString: config.pgUrl,
    // Determinism note:
    // - keep transaction serialization under our control by using explicit SQL.
    max: 5,
  });
}

export async function ensureSchema(pool: Pool, config: Config): Promise<void> {
  // Deterministic: apply migrations exactly once by checking for chain_state existence.
  const check = await pool.query(`SELECT to_regclass('public.chain_state') as ok`);
  const ok = check.rows[0]?.ok;
  if (ok) return;

  const fs = await import("node:fs/promises");
  const path = await import("node:path");
  const initPath = path.join(config.schemaMigrationsPath, "001_init.sql");
  const sql = await fs.readFile(initPath, "utf8");
  await pool.query(sql);
}

export async function getOrCreateChainState(pool: Pool, config: Config): Promise<ChainStateRow> {
  const network = config.network;
  const res = await pool.query<ChainStateRow>(
    `
      SELECT indexer_name, network, last_height, last_block_hash, critical_root_mismatch, updated_at
      FROM chain_state WHERE indexer_name = $1
    `,
    [config.indexerName],
  );
  if (res.rowCount && res.rows[0]) return res.rows[0];

  await pool.query(
    `
      INSERT INTO chain_state(indexer_name, network, last_height, last_block_hash, critical_root_mismatch, updated_at)
      VALUES($1, $2, -1, NULL, FALSE, now())
    `,
    [config.indexerName, network],
  );

  const res2 = await pool.query<ChainStateRow>(
    `
      SELECT indexer_name, network, last_height, last_block_hash, critical_root_mismatch, updated_at
      FROM chain_state WHERE indexer_name = $1
    `,
    [config.indexerName],
  );
  return res2.rows[0];
}

export async function updateChainState(pool: Pool, config: Config, patch: Partial<Pick<ChainStateRow, "last_height" | "last_block_hash" | "critical_root_mismatch">>): Promise<void> {
  const cur = await getOrCreateChainState(pool, config);
  const nextLastHeight = patch.last_height ?? cur.last_height;
  const nextLastBlockHash = patch.last_block_hash ?? cur.last_block_hash;
  const nextCritical = patch.critical_root_mismatch ?? cur.critical_root_mismatch;

  await pool.query(
    `
      UPDATE chain_state
      SET last_height = $2,
          last_block_hash = $3,
          critical_root_mismatch = $4,
          updated_at = now()
      WHERE indexer_name = $1
    `,
    [config.indexerName, nextLastHeight, nextLastBlockHash, nextCritical],
  );
}

export async function truncateFromHeight(pool: Pool, height: bigint): Promise<void> {
  // Cascade delete via FK graph.
  await pool.query(`DELETE FROM blocks WHERE height >= $1`, [height]);
}

// --------------------
// Query helpers for API
// --------------------

export async function listBlocks(pool: Pool, limit: number): Promise<any[]> {
  const res = await pool.query(
    `
      SELECT height, block_hash, time, tx_count,
             rigor_res_minted, rigor_winner,
             draw_candidate_root,
             res_probability_target
      FROM blocks
      ORDER BY height DESC
      LIMIT $1
    `,
    [limit],
  );
  return res.rows;
}

export async function getBlockByHash(pool: Pool, hash: string): Promise<any | null> {
  const b = await pool.query(
    `
      SELECT *
      FROM blocks
      WHERE block_hash = $1
    `,
    [hash],
  );
  if (!b.rowCount) return null;

  const txs = await pool.query(
    `
      SELECT txid, wtxid, block_tx_index
      FROM transactions
      WHERE block_hash = $1
      ORDER BY block_tx_index ASC
    `,
    [hash],
  );

  const candidates = await pool.query(
    `
      SELECT convergence_txid, draw_eligible, convergence_root
      FROM convergence_events
      WHERE block_hash = $1
      ORDER BY convergence_txid ASC
    `,
    [hash],
  );

  const block = b.rows[0];
  const convergence_panel = buildBlockConvergencePanel(block, candidates.rows);

  // Legacy field: winner row for audit consumers.
  const winnerRow = candidates.rows.find((c) => c.convergence_txid === (block.rigor_winner || "").trim()) ?? null;

  const lineage_graph = await getLineageForBlock(pool, hash);

  return {
    block,
    txs: txs.rows,
    convergence: winnerRow,
    convergence_panel,
    lineage_graph,
  };
}

export async function getTxByTxid(pool: Pool, txid: string): Promise<any | null> {
  const tx = await pool.query(`SELECT * FROM transactions WHERE txid = $1`, [txid]);
  if (!tx.rowCount) return null;

  const vin = await pool.query(
    `
      SELECT vin_index, prev_txid, prev_vout, sequence, coinbase, txinwitness, script_sig
      FROM inputs
      WHERE txid = $1
      ORDER BY vin_index ASC
    `,
    [txid],
  );

  const vout = await pool.query(
    `
      SELECT vout_index, value_atoms, asset_kind, address, witness_version, program_hex, script_pubkey
      FROM outputs
      WHERE txid = $1
      ORDER BY vout_index ASC
    `,
    [txid],
  );

  const resMint = await pool.query(
    `
      SELECT *
      FROM res_mints
      WHERE mint_txid = $1
      ORDER BY mint_vout_index ASC
    `,
    [txid],
  );

  const convergence = await pool.query(
    `
      SELECT ce.*,
             (ce.convergence_txid = b.rigor_winner AND b.rigor_winner <> '') AS is_winner
      FROM convergence_events ce
      JOIN blocks b ON b.block_hash = ce.block_hash
      WHERE ce.convergence_txid = $1
      ORDER BY ce.block_height DESC, ce.convergence_txid ASC
      LIMIT 20
    `,
    [txid],
  );

  const convergence_panel = buildTxConvergencePanel(convergence.rows);

  const blockCtx = await pool.query(
    `
      SELECT b.block_hash, b.height, b.rigor_res_minted, b.rigor_winner, b.committed_root
      FROM transactions t
      JOIN blocks b ON b.block_hash = t.block_hash
      WHERE t.txid = $1
      LIMIT 1
    `,
    [txid],
  );

  const witness_panel = buildTxWitnessPanel(
    txid,
    vin.rows,
    resMint.rows,
    blockCtx.rows[0] ?? null,
    convergence_panel,
    convergence.rows,
  );

  const linkedMint = await pool.query(
    `
      SELECT *
      FROM res_mints
      WHERE winner_convergence_txid = $1
      ORDER BY block_height DESC, mint_vout_index ASC
      LIMIT 5
    `,
    [txid],
  );

  const tipRow = await pool.query(`SELECT last_height FROM chain_state ORDER BY updated_at DESC LIMIT 1`);
  const indexerTipHeight = tipRow.rows[0] ? Number(tipRow.rows[0].last_height) : -1;

  const converge_proof_panel = buildConvergeProofPanel({
    txid,
    convergencePanel: convergence_panel,
    convergenceRows: convergence.rows,
    resMintRows: resMint.rows,
    linkedMintRows: linkedMint.rows,
    witnessPanel: witness_panel,
    vouts: vout.rows,
    block: blockCtx.rows[0] ?? null,
    indexerTipHeight,
  });

  const lineage_graph = await getLineageForTxid(pool, txid);

  return {
    tx: tx.rows[0],
    vin: vin.rows,
    vout: vout.rows,
    resMint: resMint.rows,
    convergence: convergence.rows,
    convergence_panel,
    witness_panel,
    converge_proof_panel,
    lineage_graph,
  };
}

export async function listResMints(pool: Pool, limit: number): Promise<any[]> {
  const res = await pool.query(
    `
      SELECT rm.block_height, b.block_hash, rm.mint_txid, rm.mint_vout_index, rm.mint_amount,
             rm.winner_convergence_txid,
             rm.bind_hash, rm.bind_height,
             rm.convergence_root,
             rm.draw_eligible,
             rm.res_probability_target,
             rm.committed_root, rm.computed_root,
             rm.proof_items
      FROM res_mints rm
      JOIN blocks b ON b.block_hash = rm.block_hash
      ORDER BY rm.block_height DESC
      LIMIT $1
    `,
    [limit],
  );
  return res.rows;
}

export async function listResEvents(pool: Pool, limit: number, eligibleOnly: boolean): Promise<any[]> {
  const res = await pool.query(
    `
      SELECT ce.*
      FROM convergence_events ce
      WHERE ($2::boolean = FALSE) OR (ce.draw_eligible = TRUE)
      ORDER BY ce.block_height DESC, ce.convergence_txid ASC
      LIMIT $1
    `,
    [limit, eligibleOnly],
  );
  return res.rows;
}

/** @deprecated Use getAddressIntelligence from addressDb.ts */
export async function getAddressBalances(pool: Pool, address: string): Promise<any> {
  const { getAddressIntelligence } = await import("./addressDb");
  const { loadConfigFromEnv } = await import("./config");
  const row = await getAddressIntelligence(pool, loadConfigFromEnv(), address, 1, 50);
  if (!row) return { oru_utxos: [], res_outputs: [] };
  return row;
}

