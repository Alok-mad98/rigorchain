/**
 * Public-data fetchers. Values are INPUTS only.
 * Never treat explorer/RPC "valid", Draw Fingerprint, or winner fields as a verdict.
 */

import { getBlock, getTx } from "../explorerClient";
import { isRpcConfigured, rpcCall } from "../../../lib/rigordRpcClient";
import { normalizeHex } from "./format";
import { parseRawBlockJson, parseRawTxAmounts } from "./verify";
import type { BlockPublicInput, PublicDataSource, TxPublicInput } from "./types";

function asRecord(v: unknown): Record<string, unknown> | null {
  return v && typeof v === "object" ? (v as Record<string, unknown>) : null;
}

function pickString(...vals: unknown[]): string | null {
  for (const v of vals) {
    if (typeof v === "string" && v.trim()) return v.trim();
  }
  return null;
}

function atomsFromUnknown(v: unknown): bigint {
  if (v == null || v === "") return 0n;
  try {
    return BigInt(String(v).split(".")[0]);
  } catch {
    return 0n;
  }
}

export async function loadPublicTransaction(txid: string): Promise<TxPublicInput> {
  const id = normalizeHex(txid);
  if (isRpcConfigured()) {
    const rpc = await rpcCall<Record<string, unknown>>("getrawtransaction", [id, true]);
    if (rpc.result) {
      const amounts = parseRawTxAmounts(rpc.result);
      return {
        txidDisplay: String(rpc.result.txid ?? id),
        blockHashDisplay: pickString(rpc.result.blockhash),
        height: rpc.result.height != null ? Number(rpc.result.height) : null,
        amounts,
        source: "rpc",
      };
    }
  }

  const explorer = await getTx(id);
  const body = asRecord(explorer.body);
  if (!body) {
    throw new Error(explorer.error ?? "transaction public data unavailable");
  }
  const tx = asRecord(body.tx) ?? body;
  const vout = Array.isArray(body.vout) ? (body.vout as Record<string, unknown>[]) : [];
  let oru = 0n;
  let res = 0n;
  for (const o of vout) {
    const atoms = atomsFromUnknown(o.value_atoms);
    if (o.asset_kind === "RES") res += atoms;
    else if (o.asset_kind === "ORU") oru += atoms;
  }
  const blockCtx = asRecord(body.convergence_panel);
  return {
    txidDisplay: String(tx.txid ?? id),
    blockHashDisplay: pickString(tx.block_hash, blockCtx?.block_hash),
    height: tx.block_height != null ? Number(tx.block_height) : blockCtx?.block_height != null ? Number(blockCtx.block_height) : null,
    amounts: { oruAtoms: oru, resAtoms: res, feeAtoms: null },
    source: "explorer",
  };
}

function blockFromExplorerBody(body: Record<string, unknown>, source: PublicDataSource): BlockPublicInput {
  const block = asRecord(body.block) ?? body;
  const panel = asRecord(body.convergence_panel);
  const summary = asRecord(body.summary);
  const txs = Array.isArray(body.txs) ? (body.txs as Record<string, unknown>[]) : [];
  const txids = txs.map((t) => String(t.txid ?? "")).filter(Boolean);
  const candidates: BlockPublicInput["candidates"] = [];
  const listed = panel && Array.isArray(panel.candidate_txids) ? (panel.candidate_txids as unknown[]) : [];
  for (const id of listed) {
    candidates.push({ txidDisplay: String(id), weight: 1n });
  }
  return {
    hashDisplay: String(block.block_hash ?? block.hash ?? ""),
    height: Number(block.height ?? 0),
    previousHashDisplay: pickString(
      block.previousblockhash,
      block.previous_block_hash,
      block.prev_hash,
      summary?.previous_block_hash,
    ),
    txidsDisplay: txids,
    candidates,
    committedRootDisplay: pickString(panel?.convergence_root, block.committed_root, block.draw_candidate_root),
    publishedWinnerDisplay: pickString(panel?.winner_txid, block.rigor_winner),
    source,
  };
}

export async function loadPublicBlock(hash: string): Promise<BlockPublicInput> {
  const id = normalizeHex(hash);

  if (isRpcConfigured()) {
    const rpc = await rpcCall<Record<string, unknown>>("getblock", [id, 2]);
    if (rpc.result) {
      const parsed = parseRawBlockJson(JSON.stringify(rpc.result));
      parsed.source = "rpc";
      return parsed;
    }
  }

  const explorer = await getBlock(id);
  const body = asRecord(explorer.body);
  if (!body) {
    throw new Error(explorer.error ?? "block public data unavailable");
  }
  return blockFromExplorerBody(body, "explorer");
}
