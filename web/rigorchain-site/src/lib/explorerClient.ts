/**
 * Read-only explorer HTTP client.
 * No mock data; failures exposed explicitly.
 */

export type ExplorerFetchResult<T> = {
  body: T | null;
  status: number;
  error: string | null;
  fetched_at: string;
};

const EXPLORER_TIMEOUT_MS = 10000;

const DEFAULT_EXPLORER_URL = "http://explorer.rigorchain.com";
const SAME_ORIGIN = new Set(["same-origin", "/", "."]);

function envExplorerUrl(): string {
  return (import.meta.env.VITE_EXPLORER_API_URL ?? "").trim();
}

function baseUrl(): string | null {
  const raw = envExplorerUrl();
  if (SAME_ORIGIN.has(raw)) return "";
  if (raw) return raw.replace(/\/$/, "");
  return DEFAULT_EXPLORER_URL.replace(/\/$/, "");
}

async function explorerGet<T>(path: string): Promise<ExplorerFetchResult<T>> {
  const fetched_at = new Date().toISOString();
  const base = baseUrl();

  // "" is same-origin (VITE_EXPLORER_API_URL=same-origin). Only null means unset.
  if (base === null) {
    return {
      body: null,
      status: 0,
      error: "VITE_EXPLORER_API_URL not configured",
      fetched_at,
    };
  }

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), EXPLORER_TIMEOUT_MS);

  try {
    const res = await fetch(`${base}${path}`, { signal: controller.signal });
    const status = res.status;

    if (!res.ok) {
      return {
        body: null,
        status,
        error: `HTTP ${status}`,
        fetched_at,
      };
    }

    const body = (await res.json()) as T;
    return { body, status, error: null, fetched_at };
  } catch (e) {
    const message =
      e instanceof Error && e.name === "AbortError"
        ? `Explorer timeout (${EXPLORER_TIMEOUT_MS}ms)`
        : e instanceof Error
          ? e.message
          : String(e);
    return { body: null, status: 0, error: message, fetched_at };
  } finally {
    clearTimeout(timer);
  }
}

export type ApiStatus = {
  equivalence_claimed?: boolean;
  generated_at?: string;
  rpc_authority_disclaimer?: string;
  [key: string]: unknown;
};

export type ResMintRow = {
  txid?: string;
  mint_txid?: string;
  block_hash?: string;
  height?: number | string;
  block_height?: number | string;
  amount?: string | number;
  mint_amount?: string | number;
  confirmations?: number | string;
  [key: string]: unknown;
};

export type ResMintsResponse = {
  mints?: ResMintRow[];
  [key: string]: unknown;
};

export type ExplorerBlockRow = {
  height?: number | string;
  block_hash?: string;
  hash?: string;
  oru_minted?: string | number;
  oru_minted_display?: string;
  oru_minted_atoms?: string | number;
  converge_burn_display?: string;
  converge_burn_atoms?: string | number;
  tx_count?: number | string;
  [key: string]: unknown;
};

export type BlocksResponse = {
  blocks?: ExplorerBlockRow[];
  [key: string]: unknown;
};

export function getStatus() {
  return explorerGet<ApiStatus>("/api/status");
}

export function getBlock(hash: string) {
  return explorerGet<Record<string, unknown>>(`/api/block/${encodeURIComponent(hash)}`);
}

export function getBlocks(limit = 50) {
  return explorerGet<BlocksResponse>(`/api/blocks?limit=${encodeURIComponent(String(limit))}`);
}

export function getTx(txid: string) {
  return explorerGet<Record<string, unknown>>(`/api/tx/${encodeURIComponent(txid)}`);
}

export function getResMints() {
  return explorerGet<ResMintsResponse>("/api/res/mints");
}

export function getLineage(txid: string) {
  return explorerGet<Record<string, unknown>>(`/api/lineage/${encodeURIComponent(txid)}`);
}

export function isExplorerConfigured(): boolean {
  if (SAME_ORIGIN.has(envExplorerUrl())) return true;
  return baseUrl() !== null;
}

export function explorerBaseUrl(): string | null {
  return baseUrl();
}
