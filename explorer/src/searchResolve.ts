import type { Pool } from "pg";
import { isValidAddressFormat } from "./addressIntelligence";
import { displayFingerprint } from "./consensusFingerprint";
import { getBlockByHash, getBlockHashByHeight, getTxByTxid } from "./db";

function blockResolution(hash: string, row: any): SearchResolution {
  const fp = row?.summary?.consensus_fingerprint ?? row?.block?.consensus_fingerprint ?? null;
  return {
    kind: "block",
    hash,
    consensus_fingerprint: fp,
    consensus_fingerprint_display: displayFingerprint(fp),
  };
}

export type SearchResolution =
  | { kind: "tx"; txid: string }
  | {
      kind: "block";
      hash: string;
      consensus_fingerprint?: string | null;
      consensus_fingerprint_display?: string;
    }
  | { kind: "address"; address: string }
  | { kind: "error"; message: string };

const HEX64 = /^[0-9a-f]{64}$/i;
const BLOCK_HEIGHT = /^\d{1,12}$/;
const UNRECOGNIZED =
  "Unrecognized input. Expected a block height, 64-char hex (txid or block hash), or a RIGOR bech32 address (trg1… / rg1…).";

export function parseBlockHeightQuery(raw: string): string | null {
  const q = raw.trim();
  if (!BLOCK_HEIGHT.test(q)) return null;
  return q;
}

export function classifySearchInput(raw: string): SearchResolution | null {
  const q = raw.trim();
  if (!q) return null;

  if (HEX64.test(q)) {
    return { kind: "tx", txid: q.toLowerCase() };
  }

  if (parseBlockHeightQuery(q) != null) {
    return null;
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return { kind: "error", message: UNRECOGNIZED };
}

/** Disambiguate 64-char hex using indexed rows (RR-01 txid preferred, RR-02 block hash). */
export async function resolveSearchInput(pool: Pool, raw: string): Promise<SearchResolution> {
  const q = raw.trim();
  if (!q) {
    return { kind: "error", message: "Search input is empty." };
  }

  if (HEX64.test(q)) {
    const lower = q.toLowerCase();
    const tx = await getTxByTxid(pool, lower);
    if (tx) return { kind: "tx", txid: lower };
    const block = await getBlockByHash(pool, lower);
    if (block) return blockResolution(lower, block);
    // RR-01: default to tx canonical route when unknown (page shows not-found).
    return { kind: "tx", txid: lower };
  }

  const height = parseBlockHeightQuery(q);
  if (height != null) {
    const hash = await getBlockHashByHeight(pool, height);
    if (hash) {
      const block = await getBlockByHash(pool, hash);
      return blockResolution(hash, block);
    }
    return { kind: "error", message: `Block height ${height} not found.` };
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return { kind: "error", message: UNRECOGNIZED };
}

export function canonicalUrlForResolution(base: string, resolution: SearchResolution): string | null {
  const prefix = base ? base : "";
  if (resolution.kind === "tx") return `${prefix}/tx/${encodeURIComponent(resolution.txid)}`;
  if (resolution.kind === "block") return `${prefix}/block/${encodeURIComponent(resolution.hash)}`;
  if (resolution.kind === "address") return `${prefix}/address/${encodeURIComponent(resolution.address)}`;
  return null;
}
