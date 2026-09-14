import type { Pool } from "pg";
import { buildBlockConvergencePanel, buildTxConvergencePanel } from "./convergencePanel";
import { buildConvergeProofPanel } from "./convergeProofPanel";
import {
  buildBlockLineageGraph,
  buildLineageGraph,
  emptyLineageGraph,
  type LineageGraph,
} from "./lineageGraph";
import { buildTxWitnessPanel } from "./witnessPanel";
import { CONVERGENCE_IS_MINT_SQL } from "./rfc020MintSql";

async function loadTxLineageParts(pool: Pool, txid: string) {
  const tx = await pool.query(`SELECT * FROM transactions WHERE txid = $1`, [txid]);
  if (!tx.rowCount) return null;

  const vin = await pool.query(
    `SELECT vin_index, prev_txid, prev_vout, sequence, coinbase, txinwitness, script_sig
     FROM inputs WHERE txid = $1 ORDER BY vin_index ASC`,
    [txid],
  );

  const vout = await pool.query(
    `SELECT vout_index, value_atoms, asset_kind, address, witness_version, program_hex, script_pubkey
     FROM outputs WHERE txid = $1 ORDER BY vout_index ASC`,
    [txid],
  );

  const resMint = await pool.query(`SELECT * FROM res_mints WHERE mint_txid = $1 ORDER BY mint_vout_index ASC`, [
    txid,
  ]);

  const convergence = await pool.query(
    `SELECT ce.*, ${CONVERGENCE_IS_MINT_SQL} AS is_winner
     FROM convergence_events ce
     JOIN blocks b ON b.block_hash = ce.block_hash
     WHERE ce.convergence_txid = $1
     ORDER BY ce.block_height DESC LIMIT 20`,
    [txid],
  );

  const linkedMint = await pool.query(
    `SELECT * FROM res_mints WHERE winner_convergence_txid = $1
     ORDER BY block_height DESC, mint_vout_index ASC LIMIT 5`,
    [txid],
  );

  const blockCtx = await pool.query(
    `SELECT b.block_hash, b.height, b.rigor_res_minted, b.rigor_winner, b.committed_root, b.mismatch
     FROM transactions t JOIN blocks b ON b.block_hash = t.block_hash WHERE t.txid = $1 LIMIT 1`,
    [txid],
  );

  const inclusion = blockCtx.rows[0]
    ? { block_hash: blockCtx.rows[0].block_hash, block_height: blockCtx.rows[0].height }
    : null;
  const convRow = convergence.rows[0] ?? null;
  let blockHash = convRow?.block_hash ? String(convRow.block_hash) : blockCtx.rows[0]?.block_hash;
  if (!blockHash && linkedMint.rows[0]?.block_hash) {
    blockHash = String(linkedMint.rows[0].block_hash);
  }

  let blockPanel = null;
  let candidates: Array<{ convergence_txid: string; draw_eligible: boolean; convergence_root?: string }> = [];
  let blockRow = blockCtx.rows[0] ?? null;
  let winnerRow: Record<string, unknown> | null = null;

  if (blockHash) {
    const b = await pool.query(`SELECT * FROM blocks WHERE block_hash = $1`, [blockHash]);
    if (b.rowCount) {
      blockRow = b.rows[0];
      const cand = await pool.query(
        `SELECT convergence_txid, draw_eligible, convergence_root
         FROM convergence_events WHERE block_hash = $1 ORDER BY convergence_txid ASC`,
        [blockHash],
      );
      candidates = cand.rows;
      const mintRow = await pool.query<{ mint_txid: string }>(
        `SELECT mint_txid FROM res_mints WHERE block_hash = $1 ORDER BY mint_vout_index ASC LIMIT 1`,
        [blockHash],
      );
      const mintTxid = mintRow.rows[0]?.mint_txid ?? null;
      blockPanel = buildBlockConvergencePanel(
        { ...blockRow, mint_txid: mintTxid },
        candidates,
      );
      const winner = (blockRow.rigor_winner || mintTxid || "").trim();
      winnerRow = candidates.find((c) => c.convergence_txid === winner) ?? convRow;
    }
  }

  const convergencePanel = buildTxConvergencePanel(convergence.rows, inclusion, {
    isResMint: (resMint.rowCount ?? 0) > 0,
  });
  const witnessPanel = buildTxWitnessPanel(
    txid,
    vin.rows,
    resMint.rows,
    blockRow,
    convergencePanel,
    convergence.rows,
  );

  const tipRow = await pool.query(`SELECT last_height FROM chain_state ORDER BY updated_at DESC LIMIT 1`);
  const indexerTipHeight = tipRow.rows[0] ? Number(tipRow.rows[0].last_height) : -1;

  const convergeProof = buildConvergeProofPanel({
    txid,
    convergencePanel,
    convergenceRows: convergence.rows,
    resMintRows: resMint.rows,
    linkedMintRows: linkedMint.rows,
    witnessPanel,
    vouts: vout.rows,
    block: blockRow,
    indexerTipHeight,
  });

  let mintResOutputs: Array<{ address: string | null; value_atoms: string | number }> = [];
  const mintTxid = convergeProof.res_mint_linkage.mint_txid;
  if (mintTxid && mintTxid !== txid) {
    const mo = await pool.query(
      `SELECT address, value_atoms FROM outputs WHERE txid = $1 AND asset_kind = 'RES' ORDER BY vout_index ASC`,
      [mintTxid],
    );
    mintResOutputs = mo.rows;
  }

  return {
    txid,
    blockPanel,
    candidates,
    convergeProof,
    block: blockRow,
    winnerRow,
    mintResOutputs,
  };
}

export async function getLineageForTxid(pool: Pool, txid: string): Promise<LineageGraph | null> {
  const parts = await loadTxLineageParts(pool, txid);
  if (!parts) return null;

  const graph = buildLineageGraph({
    anchorTxid: txid,
    blockPanel: parts.blockPanel,
    candidates: parts.candidates,
    convergeProof: parts.convergeProof,
    block: parts.block
      ? {
          block_hash: parts.block.block_hash,
          height: parts.block.height,
          rigor_winner: parts.block.rigor_winner,
          rigor_res_minted: parts.block.rigor_res_minted,
          mismatch: parts.block.mismatch,
        }
      : null,
    winnerConvergenceRow: parts.winnerRow,
    mintResOutputs: parts.mintResOutputs,
  });

  return graph;
}

export async function getLineageForBlock(pool: Pool, blockHash: string): Promise<LineageGraph | null> {
  const b = await pool.query(`SELECT * FROM blocks WHERE block_hash = $1`, [blockHash]);
  if (!b.rowCount) return null;

  const block = b.rows[0];
  const candidates = await pool.query(
    `SELECT convergence_txid, draw_eligible, convergence_root
     FROM convergence_events WHERE block_hash = $1 ORDER BY convergence_txid ASC`,
    [blockHash],
  );
  if (candidates.rowCount === 0) {
    return emptyLineageGraph(null);
  }

  const mintRow = await pool.query<{ mint_txid: string }>(
    `SELECT mint_txid FROM res_mints WHERE block_hash = $1 ORDER BY mint_vout_index ASC LIMIT 1`,
    [blockHash],
  );
  const winnerTxid = (block.rigor_winner || mintRow.rows[0]?.mint_txid || "").trim();
  const anchor = winnerTxid || candidates.rows[0].convergence_txid;
  const parts = await loadTxLineageParts(pool, anchor);
  if (!parts) return null;

  const winnerRow =
    candidates.rows.find((c) => c.convergence_txid === winnerTxid) ?? candidates.rows[0] ?? null;

  return buildBlockLineageGraph(
    blockHash,
    block,
    candidates.rows,
    parts.convergeProof,
    winnerRow,
    parts.mintResOutputs,
  );
}
