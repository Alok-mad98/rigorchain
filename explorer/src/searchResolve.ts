import type { Pool } from "pg";
import { isValidAddressFormat } from "./addressIntelligence";
import { getBlockByHash, getTxByTxid } from "./db";

export type SearchResolution =
  | { kind: "tx"; txid: string }
  | { kind: "block"; hash: string }
  | { kind: "address"; address: string }
  | { kind: "error"; message: string };

const HEX64 = /^[0-9a-f]{64}$/i;

export function classifySearchInput(raw: string): SearchResolution | null {
  const q = raw.trim();
  if (!q) return null;

  if (HEX64.test(q)) {
    return { kind: "tx", txid: q.toLowerCase() };
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return { kind: "error", message: "Unrecognized input. Expected 64-char hex (txid or block hash) or a RIGOR bech32 address (trg1… / rg1…)." };
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
    if (block) return { kind: "block", hash: lower };
    // RR-01: default to tx canonical route when unknown (page shows not-found).
    return { kind: "tx", txid: lower };
  }

  if (isValidAddressFormat(q)) {
    return { kind: "address", address: q };
  }

  return {
    kind: "error",
    message: "Unrecognized input. Expected 64-char hex (txid or block hash) or a RIGOR bech32 address (trg1… / rg1…).",
  };
}

export function canonicalUrlForResolution(base: string, resolution: SearchResolution): string | null {
  const prefix = base ? base : "";
  if (resolution.kind === "tx") return `${prefix}/tx/${encodeURIComponent(resolution.txid)}`;
  if (resolution.kind === "block") return `${prefix}/block/${encodeURIComponent(resolution.hash)}`;
  if (resolution.kind === "address") return `${prefix}/address/${encodeURIComponent(resolution.address)}`;
  return null;
}
