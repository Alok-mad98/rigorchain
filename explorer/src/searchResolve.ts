import type { Pool } from "pg";
import { isValidAddressFormat } from "./addressIntelligence";
import { displayFingerprint } from "./consensusFingerprint";
import { getBlockByHash, getBlockHashByHeight, getTxByTxid, getTxidByWtxid } from "./db";

export const WTXID_INFORMATIONAL =
  "wtxid is informational only. Use /tx/{txid} with canonical txid.";
export const HASH_NOT_INDEXED = "No indexed transaction or block matches that hash.";

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

export type SearchLookups = {
  getTxByTxid: (hex: string) => Promise<unknown | null>;
  getBlockByHash: (hex: string) => Promise<unknown | null>;
  getTxidByWtxid: (hex: string) => Promise<string | null>;
  getBlockHashByHeight: (height: string) => Promise<string | null>;
};

const HEX64 = /^[0-9a-f]{64}$/i;
const BLOCK_HEIGHT = /^\d{1,12}$/;
const UNRECOGNIZED =
  "Unrecognized input. Expected a block height, 64-char hex (txid or block hash), or a bech32 address for this explorer network.";

export function parseBlockHeightQuery(raw: string): string | null {
  const q = raw.trim();
  if (!BLOCK_HEIGHT.test(q)) return null;
  return q;
}

export function isHex64(raw: string): boolean {
  return HEX64.test(raw.trim());
}

/** Height (1–12 digits) or 64-char block hash. Other strings are not block route keys. */
export function parseBlockRouteParam(raw: string): { kind: "hash"; hash: string } | { kind: "height"; height: string } | null {
  const q = raw.trim();
  if (!q) return null;
  if (HEX64.test(q)) return { kind: "hash", hash: q.toLowerCase() };
  const height = parseBlockHeightQuery(q);
  if (height != null) return { kind: "height", height };
  return null;
}

export function classifySearchInput(raw: string): SearchResolution | null {
  const q = raw.trim();
  if (!q) return null;

  if (HEX64.test(q) || parseBlockHeightQuery(q) != null) {
    return null;
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return { kind: "error", message: UNRECOGNIZED };
}

export async function resolveSearchInputWith(lookups: SearchLookups, raw: string): Promise<SearchResolution> {
  const q = raw.trim();
  if (!q) {
    return { kind: "error", message: "Search input is empty." };
  }

  if (HEX64.test(q)) {
    const lower = q.toLowerCase();
    const tx = await lookups.getTxByTxid(lower);
    if (tx) return { kind: "tx", txid: lower };
    const block = await lookups.getBlockByHash(lower);
    if (block) return blockResolution(lower, block);
    const canonicalTxid = await lookups.getTxidByWtxid(lower);
    if (canonicalTxid) {
      return { kind: "error", message: `${WTXID_INFORMATIONAL} Canonical txid: ${canonicalTxid}` };
    }
    return { kind: "error", message: HASH_NOT_INDEXED };
  }

  const height = parseBlockHeightQuery(q);
  if (height != null) {
    const hash = await lookups.getBlockHashByHeight(height);
    if (hash) {
      const block = await lookups.getBlockByHash(hash);
      return blockResolution(hash, block);
    }
    return { kind: "error", message: `Block height ${height} not found.` };
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return { kind: "error", message: UNRECOGNIZED };
}

/** Indexed txid first, then block hash. Unknown 64-hex is not-found, never a fake tx route. Wtxid is rejected. */
export async function resolveSearchInput(pool: Pool, raw: string): Promise<SearchResolution> {
  return resolveSearchInputWith(
    {
      getTxByTxid: (hex) => getTxByTxid(pool, hex),
      getBlockByHash: (hex) => getBlockByHash(pool, hex),
      getTxidByWtxid: (hex) => getTxidByWtxid(pool, hex),
      getBlockHashByHeight: (height) => getBlockHashByHeight(pool, height),
    },
    raw,
  );
}

export async function loadBlockByRouteParam(pool: Pool, raw: string): Promise<{ hash: string; row: any } | null> {
  const parsed = parseBlockRouteParam(raw);
  if (!parsed) return null;
  if (parsed.kind === "hash") {
    const row = await getBlockByHash(pool, parsed.hash);
    return row ? { hash: parsed.hash, row } : null;
  }
  const hash = await getBlockHashByHeight(pool, parsed.height);
  if (!hash) return null;
  const row = await getBlockByHash(pool, hash);
  return row ? { hash, row } : null;
}

export function canonicalUrlForResolution(base: string, resolution: SearchResolution): string | null {
  const prefix = base ? base : "";
  if (resolution.kind === "tx") return `${prefix}/tx/${encodeURIComponent(resolution.txid)}`;
  if (resolution.kind === "block") return `${prefix}/block/${encodeURIComponent(resolution.hash)}`;
  if (resolution.kind === "address") return `${prefix}/address/${encodeURIComponent(resolution.address)}`;
  return null;
}
