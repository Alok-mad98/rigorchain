import { Pool } from "pg";
import type { Config } from "./config";
import { buildBlockConvergencePanel, buildTxConvergencePanel } from "./convergencePanel";
import { buildConvergeProofPanel } from "./convergeProofPanel";
import { getLineageForBlock, getLineageForTxid } from "./lineageDb";
import { buildTxWitnessPanel } from "./witnessPanel";
import { computeTxFee, formatOruAtoms } from "./fee";
import { classifyTxKind, pickPrimaryOruOutput, txKindLabel } from "./txSummary";
import { displayFingerprint, fingerprintStatus, shortFingerprint } from "./consensusFingerprint";
import { atomsToOruDisplay, atomsToResDisplay } from "./addressIntelligence";
import { displayOrNA, displayOrNotAvailable } from "./rpcFields";

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

const MIGRATION_FILES = [
  "001_init.sql",
  "002_alphaplus_compat.sql",
  "003_c_root.sql",
  "004_consensus_fingerprint.sql",
];

export async function ensureSchema(pool: Pool, config: Config): Promise<void> {
  const fs = await import("node:fs/promises");
  const path = await import("node:path");

  await pool.query(`
    CREATE TABLE IF NOT EXISTS schema_migrations (
      id TEXT PRIMARY KEY,
      applied_at TIMESTAMPTZ NOT NULL DEFAULT now()
    )
  `);

  const existing = await pool.query(`SELECT to_regclass('public.chain_state') as ok`);
  if (existing.rows[0]?.ok) {
    await pool.query(
      `INSERT INTO schema_migrations(id) VALUES ('001_init.sql') ON CONFLICT (id) DO NOTHING`,
    );
  }

  for (const file of MIGRATION_FILES) {
    const applied = await pool.query(`SELECT 1 FROM schema_migrations WHERE id = $1`, [file]);
    if (applied.rowCount) continue;

    const sql = await fs.readFile(path.join(config.schemaMigrationsPath, file), "utf8");
    await pool.query(sql);
    await pool.query(`INSERT INTO schema_migrations(id) VALUES ($1)`, [file]);
  }
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
      SELECT b.height, b.block_hash, b.time, b.tx_count,
             b.rigor_winner,
             b.c_root,
             b.consensus_fingerprint,
             b.draw_candidate_root,
             b.committed_root,
             b.computed_root,
             COALESCE((
               SELECT COUNT(*)::int
               FROM convergence_events ce
               WHERE ce.block_hash = b.block_hash
             ), 0) AS candidate_set_size,
             COALESCE((
               SELECT SUM(o.value_atoms)::text
               FROM transactions t
               JOIN outputs o ON o.txid = t.txid AND o.asset_kind = 'ORU'
               WHERE t.block_hash = b.block_hash
                 AND t.block_tx_index = 0
             ), '0') AS oru_minted_atoms
      FROM blocks b
      ORDER BY b.height DESC
      LIMIT $1
    `,
    [limit],
  );
  return res.rows.map((row) => {
    const atoms = BigInt(row.oru_minted_atoms ?? "0");
    return {
      ...row,
      oru_minted: formatOruAtoms(atoms),
      oru_minted_display: `${formatOruAtoms(atoms)} ORU`,
      consensus_fingerprint: row.consensus_fingerprint ?? null,
      consensus_fingerprint_short: shortFingerprint(row.consensus_fingerprint),
      consensus_fingerprint_display: displayFingerprint(row.consensus_fingerprint),
      consensus_fingerprint_status: fingerprintStatus(row.consensus_fingerprint),
    };
  });
}

export async function getBlockHashByHeight(pool: Pool, height: string | number | bigint): Promise<string | null> {
  const res = await pool.query(
    `SELECT block_hash FROM blocks WHERE height = $1::bigint LIMIT 1`,
    [String(height)],
  );
  return res.rows[0]?.block_hash ?? null;
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
      SELECT txid, wtxid, block_tx_index, is_coinbase
      FROM transactions
      WHERE block_hash = $1
      ORDER BY block_tx_index ASC
    `,
    [hash],
  );

  const txOuts = await pool.query(
    `
      SELECT txid, vout_index, value_atoms, asset_kind, address, witness_version
      FROM outputs
      WHERE txid = ANY($1::text[])
      ORDER BY txid ASC, vout_index ASC
    `,
    [txs.rows.map((t) => t.txid)],
  );
  const outsByTx = new Map<string, typeof txOuts.rows>();
  for (const o of txOuts.rows) {
    const list = outsByTx.get(o.txid) ?? [];
    list.push(o);
    outsByTx.set(o.txid, list);
  }

  const block = b.rows[0];
  const resMinted = Number(block.rigor_res_minted ?? 0);
  const mintRows = await pool.query<{ mint_txid: string; mint_amount: string }>(
    `SELECT mint_txid, mint_amount::text AS mint_amount FROM res_mints WHERE block_hash = $1`,
    [hash],
  );
  const mintTxids = new Set(mintRows.rows.map((r) => r.mint_txid));

  const enrichedTxs = txs.rows.map((t) => {
    const outs = outsByTx.get(t.txid) ?? [];
    const primary = pickPrimaryOruOutput(outs);
    const hasRes = outs.some((o) => o.asset_kind === "RES");
    const resAtoms = outs
      .filter((o) => o.asset_kind === "RES")
      .reduce((acc, o) => acc + BigInt(o.value_atoms), 0n);
    const isResMint = mintTxids.has(t.txid) || (resMinted === 1 && hasRes && !t.is_coinbase);
    const kind = classifyTxKind({
      isCoinbase: Boolean(t.is_coinbase),
      primaryRole: primary?.role ?? null,
      hasOru: outs.some((o) => o.asset_kind === "ORU"),
      hasRes,
      isResMint,
    });
    const showRes = isResMint && resAtoms > 0n;
    return {
      txid: t.txid,
      wtxid: t.wtxid,
      block_tx_index: t.block_tx_index,
      is_coinbase: Boolean(t.is_coinbase),
      tx_kind: kind,
      tx_kind_label: txKindLabel(kind),
      amount_atoms: showRes ? resAtoms.toString() : primary ? String(primary.output.value_atoms) : null,
      amount_display: showRes
        ? atomsToResDisplay(resAtoms)
        : primary
          ? formatOruAtoms(BigInt(primary.output.value_atoms))
          : null,
      amount_role: showRes ? "output" : primary?.role ?? null,
      asset_kind: showRes ? "RES" : primary ? "ORU" : hasRes ? "RES" : null,
      receiver: showRes
        ? (outs.find((o) => o.asset_kind === "RES")?.address ?? null)
        : primary?.output.address ?? null,
    };
  });

  const candidates = await pool.query(
    `
      SELECT convergence_txid, draw_eligible, convergence_root
      FROM convergence_events
      WHERE block_hash = $1
      ORDER BY convergence_txid ASC
    `,
    [hash],
  );

  const convergence_panel = buildBlockConvergencePanel(block, candidates.rows);

  // Legacy field: winner row for audit consumers.
  const winnerRow = candidates.rows.find((c) => c.convergence_txid === (block.rigor_winner || "").trim()) ?? null;

  const lineage_graph = await getLineageForBlock(pool, hash);

  const coinbaseTx = txs.rows[0] ?? null;
  let oruSubsidyAtoms = 0n;
  if (coinbaseTx?.txid) {
    const coinOuts = await pool.query(
      `SELECT value_atoms FROM outputs WHERE txid = $1 AND asset_kind = 'ORU'`,
      [coinbaseTx.txid],
    );
    for (const o of coinOuts.rows) oruSubsidyAtoms += BigInt(o.value_atoms);
  }

  let previous_block_hash: string | null = null;
  try {
    const h = BigInt(block.height);
    if (h > 0n) {
      const prev = await pool.query(`SELECT block_hash FROM blocks WHERE height = $1 LIMIT 1`, [h - 1n]);
      previous_block_hash = prev.rows[0]?.block_hash ?? null;
    }
  } catch {
    previous_block_hash = null;
  }

  let bodyResAtoms = 0n;
  for (const t of txs.rows) {
    if (t.is_coinbase) continue;
    for (const o of outsByTx.get(t.txid) ?? []) {
      if (o.asset_kind === "RES") bodyResAtoms += BigInt(o.value_atoms);
    }
  }
  const indexedMintAtoms = mintRows.rows.reduce((acc, r) => acc + BigInt(r.mint_amount ?? 0), 0n);
  const mintAtoms = indexedMintAtoms > 0n ? indexedMintAtoms : bodyResAtoms;
  const res_minted_display = mintAtoms > 0n ? atomsToResDisplay(mintAtoms) : resMinted ? String(resMinted) : "0";

  const winnerTxid = (block.rigor_winner || "").trim();
  let res_award_note: string | null = null;
  let res_payout_block_hash: string | null = null;
  if (winnerTxid) {
    const payout = await pool.query<{ block_height: string; block_hash: string; mint_amount: string }>(
      `
        SELECT block_height::text AS block_height, block_hash, mint_amount::text AS mint_amount
        FROM res_mints
        WHERE winner_convergence_txid = $1
        ORDER BY block_height ASC
        LIMIT 1
      `,
      [winnerTxid],
    );
    if (payout.rows[0]) {
      const p = payout.rows[0];
      res_award_note = `${atomsToResDisplay(p.mint_amount)} paid in block ${p.block_height}`;
      res_payout_block_hash = p.block_hash;
    } else if (resMinted === 0) {
      res_award_note = "Winner selected this block; RES mint is in the next block";
    }
  }

  const fp = block.consensus_fingerprint ?? null;
  const summary = {
    height: block.height,
    hash: block.block_hash,
    previous_block_hash,
    consensus_fingerprint: fp,
    consensus_fingerprint_display: displayFingerprint(fp),
    consensus_fingerprint_status: fingerprintStatus(fp),
    time: block.time,
    coinbase_txid: coinbaseTx?.txid ?? null,
    oru_subsidy_atoms: oruSubsidyAtoms.toString(),
    oru_subsidy_display: formatOruAtoms(oruSubsidyAtoms),
    rigor_res_minted: resMinted,
    res_minted_display,
    res_award_note,
    res_payout_block_hash,
    bind_height: block.bind_height ?? null,
    bind_height_display: displayOrNotAvailable(block.bind_height),
    bind_hash_display: displayOrNotAvailable(block.bind_hash),
    res_probability_target: block.res_probability_target ?? null,
    res_probability_target_display: displayOrNA(block.res_probability_target),
  };

  return {
    block: {
      ...block,
      previous_block_hash,
      consensus_fingerprint: fp,
      consensus_fingerprint_display: displayFingerprint(fp),
      consensus_fingerprint_status: fingerprintStatus(fp),
      bind_height_display: summary.bind_height_display,
      bind_hash_display: summary.bind_hash_display,
      res_probability_target_display: summary.res_probability_target_display,
    },
    summary,
    txs: enrichedTxs,
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

  const inclusion = blockCtx.rows[0]
    ? { block_hash: blockCtx.rows[0].block_hash, block_height: blockCtx.rows[0].height }
    : null;
  const convergence_panel = buildTxConvergencePanel(convergence.rows, inclusion);

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

  const isCoinbase = Boolean(tx.rows[0].is_coinbase);
  let oruIn: bigint | null = 0n;
  let involvesRes = vout.rows.some((o) => o.asset_kind === "RES") || resMint.rows.length > 0;
  const enrichedVin: any[] = [];
  for (const input of vin.rows) {
    let prevAddress: string | null = null;
    let prevAsset: string | null = null;
    let prevAtoms: string | null = null;
    if (input.prev_txid != null && input.prev_vout != null) {
      const prev = await pool.query(
        `SELECT address, asset_kind, value_atoms FROM outputs WHERE txid = $1 AND vout_index = $2`,
        [input.prev_txid, input.prev_vout],
      );
      const row = prev.rows[0];
      if (!row) {
        oruIn = null;
      } else {
        prevAddress = row.address ?? null;
        prevAsset = row.asset_kind;
        prevAtoms = String(row.value_atoms);
        if (row.asset_kind === "RES") involvesRes = true;
        if (row.asset_kind === "ORU" && oruIn !== null) oruIn += BigInt(row.value_atoms);
      }
    }
    enrichedVin.push({ ...input, prev_address: prevAddress, prev_asset_kind: prevAsset, prev_value_atoms: prevAtoms });
  }

  const oruOut = vout.rows
    .filter((o) => o.asset_kind === "ORU")
    .reduce((acc, o) => acc + BigInt(o.value_atoms), 0n);
  const fee = computeTxFee({
    isCoinbase,
    oruInputAtoms: isCoinbase ? 0n : oruIn,
    oruOutputAtoms: oruOut,
    involvesRes: isCoinbase ? false : involvesRes,
  });

  const primary = pickPrimaryOruOutput(vout.rows);
  const firstSender = enrichedVin.find((v) => v.prev_address)?.prev_address ?? (isCoinbase ? "Coinbase" : null);
  const primaryAtoms = primary ? BigInt(primary.output.value_atoms) : 0n;

  const summary = {
    txid,
    is_coinbase: isCoinbase,
    sender: isCoinbase ? "Coinbase" : firstSender,
    receiver: primary?.output.address ?? null,
    amount_atoms: primary ? primaryAtoms.toString() : oruOut.toString(),
    amount_display: formatOruAtoms(primary ? primaryAtoms : oruOut),
    amount_role: primary?.role ?? null,
    amount_total_atoms: oruOut.toString(),
    amount_total_display: formatOruAtoms(oruOut),
    fee,
    subsidy_display: isCoinbase ? formatOruAtoms(oruOut) : null,
  };

  return {
    tx: tx.rows[0],
    vin: enrichedVin,
    vout: vout.rows,
    summary,
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
      SELECT
        ce.convergence_txid,
        MIN(ce.block_height)::text AS included_height,
        MAX(ce.block_height)::text AS last_window_height,
        (
          SELECT MIN(b.height)::text
            FROM blocks b
           WHERE b.rigor_winner = ce.convergence_txid
             AND b.rigor_winner <> ''
        ) AS award_height,
        (
          SELECT MIN(rm.block_height)::text
            FROM res_mints rm
           WHERE rm.winner_convergence_txid = ce.convergence_txid
        ) AS mint_height,
        BOOL_OR(ce.draw_eligible) AS ever_eligible,
        COALESCE((
          SELECT SUM(rm.mint_amount)
            FROM res_mints rm
           WHERE rm.winner_convergence_txid = ce.convergence_txid
        ), MAX(ce.mint_amount), 0)::text AS mint_amount_atoms,
        COALESCE((
          SELECT SUM(o.value_atoms)
            FROM outputs o
           WHERE o.txid = ce.convergence_txid
             AND o.asset_kind = 'ORU'
             AND o.witness_version = 1
        ), 0)::text AS lock_atoms
      FROM convergence_events ce
      GROUP BY ce.convergence_txid
      HAVING ($2::boolean = FALSE) OR BOOL_OR(ce.draw_eligible)
      ORDER BY
        COALESCE(
          (SELECT MIN(rm.block_height) FROM res_mints rm WHERE rm.winner_convergence_txid = ce.convergence_txid),
          (SELECT MIN(b.height) FROM blocks b WHERE b.rigor_winner = ce.convergence_txid AND b.rigor_winner <> ''),
          MIN(ce.block_height)
        ) DESC,
        ce.convergence_txid ASC
      LIMIT $1
    `,
    [limit, eligibleOnly],
  );
  return res.rows.map((row) => {
    const mintAtoms = String(row.mint_amount_atoms ?? "0");
    const minted = BigInt(mintAtoms) > 0n;
    const eligible = Boolean(row.ever_eligible);
    let result = "No RES";
    if (minted) result = `Minted ${atomsToResDisplay(mintAtoms)}`;
    else if (eligible) result = "Eligible, no mint";
    return {
      convergence_txid: row.convergence_txid,
      included_height: row.included_height,
      last_window_height: row.last_window_height,
      award_height: row.award_height ?? null,
      mint_height: row.mint_height ?? null,
      // Primary height for RES events: draw (中签) when awarded, else inclusion.
      height: row.award_height ?? row.included_height,
      draw_eligible: eligible,
      lock_display: atomsToOruDisplay(String(row.lock_atoms ?? "0")),
      mint_amount: minted ? atomsToResDisplay(mintAtoms) : "0",
      result,
    };
  });
}

/** @deprecated Use getAddressIntelligence from addressDb.ts */
export async function getAddressBalances(pool: Pool, address: string): Promise<any> {
  const { getAddressIntelligence } = await import("./addressDb");
  const { loadConfigFromEnv } = await import("./config");
  const row = await getAddressIntelligence(pool, loadConfigFromEnv(), address, 1, 50);
  if (!row) return { oru_utxos: [], res_outputs: [] };
  return row;
}

