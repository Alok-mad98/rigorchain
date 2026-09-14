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

const DEFAULT_EXPLORER_URL = "https://explorer.rigorchain.com";

function baseUrl(): string | null {
  const url = import.meta.env.VITE_EXPLORER_API_URL?.trim() || DEFAULT_EXPLORER_URL;
  return url.replace(/\/$/, "");
}

async function explorerGet<T>(path: string): Promise<ExplorerFetchResult<T>> {
  const fetched_at = new Date().toISOString();
  const base = baseUrl();

  if (!base) {
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
  block_hash?: string;
  height?: number;
  amount?: string | number;
  confirmations?: number;
  [key: string]: unknown;
};

export type ResMintsResponse = {
  mints?: ResMintRow[];
  [key: string]: unknown;
};

export function getStatus() {
  return explorerGet<ApiStatus>("/api/status");
}

export function getBlock(hash: string) {
  return explorerGet<Record<string, unknown>>(`/api/block/${encodeURIComponent(hash)}`);
}

export function getBlocks(limit = 50) {
  return explorerGet<{ blocks?: Record<string, unknown>[] }>(`/api/blocks?limit=${encodeURIComponent(String(limit))}`);
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
  return Boolean(baseUrl());
}

export function explorerBaseUrl(): string | null {
  return baseUrl();
}
