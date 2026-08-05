/**
 * Read-only JSON-RPC client for rigord (via public proxy).
 * No retry loops that mask failure — errors surface to UI.
 */

export type RpcFetchResult<T> = {
  result: T | null;
  error: string | null;
  latency_ms: number;
  fetched_at: string;
};

const RPC_TIMEOUT_MS = 5000;

const BLOCKED_METHODS = new Set([
  "sendtoaddress",
  "sendmany",
  "sendrawtransaction",
  "submitblock",
  "generatetoaddress",
  "signrawtransactionwithkey",
  "walletpassphrase",
]);

function rpcUrl(): string | null {
  const url = import.meta.env.VITE_RPC_PROXY_URL?.trim();
  return url || null;
}

export async function rpcCall<T>(
  method: string,
  params: unknown[] = [],
): Promise<RpcFetchResult<T>> {
  const fetched_at = new Date().toISOString();
  const start = performance.now();

  if (BLOCKED_METHODS.has(method)) {
    return {
      result: null,
      error: `Blocked method: ${method}`,
      latency_ms: 0,
      fetched_at,
    };
  }

  const url = rpcUrl();
  if (!url) {
    return {
      result: null,
      error: "VITE_RPC_PROXY_URL not configured",
      latency_ms: 0,
      fetched_at,
    };
  }

  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), RPC_TIMEOUT_MS);

  try {
    const res = await fetch(url, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ jsonrpc: "2.0", id: 1, method, params }),
      signal: controller.signal,
    });

    const latency_ms = Math.round(performance.now() - start);
    const body = (await res.json()) as {
      result?: T;
      error?: { message?: string; code?: number };
    };

    if (!res.ok) {
      return {
        result: null,
        error: `HTTP ${res.status}`,
        latency_ms,
        fetched_at,
      };
    }

    if (body.error) {
      return {
        result: null,
        error: body.error.message ?? `RPC error ${body.error.code}`,
        latency_ms,
        fetched_at,
      };
    }

    return { result: body.result ?? null, error: null, latency_ms, fetched_at };
  } catch (e) {
    const latency_ms = Math.round(performance.now() - start);
    const message =
      e instanceof Error && e.name === "AbortError"
        ? `RPC timeout (${RPC_TIMEOUT_MS}ms)`
        : e instanceof Error
          ? e.message
          : String(e);
    return { result: null, error: message, latency_ms, fetched_at };
  } finally {
    clearTimeout(timer);
  }
}

export type BlockchainInfo = {
  blocks?: number;
  bestblockhash?: string;
  chain?: string;
  initialblockdownload?: boolean;
  verificationprogress?: number;
  difficulty?: number;
};

export type NetworkInfo = {
  connections?: number;
  networkactive?: boolean;
  version?: number;
};

export async function getblockchaininfo() {
  return rpcCall<BlockchainInfo>("getblockchaininfo");
}

export async function getnetworkinfo() {
  return rpcCall<NetworkInfo>("getnetworkinfo");
}

export async function getconvergencestatus() {
  return rpcCall<Record<string, unknown>>("getconvergencestatus");
}

/** Optional soft-fail — caller treats error as degraded, not global offline */
export async function getconvergenceresult(params: Record<string, unknown> = {}) {
  return rpcCall<Record<string, unknown>>("getconvergenceresult", [params]);
}

export async function getresmintstatus() {
  return rpcCall<Record<string, unknown>>("getresmintstatus");
}

export function isRpcConfigured(): boolean {
  return Boolean(rpcUrl());
}
