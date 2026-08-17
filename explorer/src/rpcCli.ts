import { execFile } from "node:child_process";
import { promisify } from "node:util";
import type { Config } from "./config";
import { rigorCliChainArgs } from "./network";

const execFileAsync = promisify(execFile);

export type RpcMethod =
  | "getblockhash"
  | "getblock"
  | "getrawtransaction"
  | "getconvergenceresult"
  | "getblockchaininfo"
  | "getconvergencestats"
  | "getconvergencestatus"
  | "getruntimesupply"
  | "getnetworkinfo"
  | "getrawmempool";

function sleepMs(ms: number): Promise<void> {
  return new Promise((r) => setTimeout(r, ms));
}

async function runCli(config: Config, args: string[]): Promise<string> {
  const cliArgs = [
    ...rigorCliChainArgs(config.network),
    `-datadir=${config.rigorDatadir}`,
    `-rpcport=${config.rpcPort}`,
    ...(config.rpcUser ? [`-rpcuser=${config.rpcUser}`] : []),
    ...(config.rpcPassword ? [`-rpcpassword=${config.rpcPassword}`] : []),
    ...args,
  ];

  const { stdout, stderr } = await execFileAsync(config.rigorCliBin, cliArgs, {
    maxBuffer: 50 * 1024 * 1024,
  });
  const out = (stdout ?? "").toString();
  if (stderr && stderr.toString().trim().length > 0) {
    // Deterministic behavior: stderr is diagnostic only.
  }
  return out.trim();
}

/** rigor-cli parses each RPC arg as JSON; encoding differs by method (see execFile argv tests). */
function formatCliParam(method: RpcMethod, p: string | number | boolean): string {
  if (typeof p === "number") return String(p);
  if (typeof p === "boolean") return p ? "true" : "false";
  if (/^-?\d+$/.test(p)) return p;
  if (method === "getconvergenceresult") {
    return JSON.stringify(p);
  }
  return p;
}

function maybeParseJson(out: string): unknown {
  if (!out) return null;
  const trimmed = out.trim();
  if (trimmed.startsWith("{") || trimmed.startsWith("[") || trimmed === "null" || trimmed === "true" || trimmed === "false") {
    return JSON.parse(trimmed) as unknown;
  }
  return trimmed;
}

export async function rpcCliCall(config: Config, method: RpcMethod, params: Array<string | number | boolean> = [], opts?: { attempts?: number}): Promise<any> {
  const attempts = opts?.attempts ?? 6;
  let lastErr: unknown = null;

  for (let i = 0; i < attempts; i++) {
    // rigor-cli parses each trailing arg as JSON (same as bitcoin-cli).
    const rpcArgs = [
      method,
      ...params.map((p) => formatCliParam(method, p)),
    ];
    try {
      const out = await runCli(config, rpcArgs);
      return maybeParseJson(out);
    } catch (e) {
      lastErr = e;
      // Retrying a full work queue adds more RPC load and can stall Explorer pages.
      if (/Work queue depth exceeded/i.test(String(e))) {
        throw e;
      }
      // Deterministic backoff: no randomness.
      const backoff = 250 * (i + 1);
      await sleepMs(backoff);
    }
  }

  throw lastErr ?? new Error(`RPC ${method} failed`);
}

export const rpc = {
  getblockhash: (config: Config, height: number | string) => rpcCliCall(config, "getblockhash", [height]),
  getblock: (config: Config, hash: string, verbosity: number = 1) => rpcCliCall(config, "getblock", [hash, verbosity]),
  getrawtransaction: (config: Config, txid: string, verbose: boolean = true) => rpcCliCall(config, "getrawtransaction", [txid, verbose]),
  getconvergenceresult: async (config: Config, hashOrHeight: string) => {
    try {
      return await rpcCliCall(config, "getconvergenceresult", [hashOrHeight]);
    } catch {
      return {};
    }
  },
  getblockchaininfo: (config: Config) => rpcCliCall(config, "getblockchaininfo", []),
  getconvergencestats: (config: Config) => rpcCliCall(config, "getconvergencestats", []),
  getconvergencestatus: (config: Config) => rpcCliCall(config, "getconvergencestatus", []),
  getruntimesupply: (config: Config) => rpcCliCall(config, "getruntimesupply", []),
  getnetworkinfo: (config: Config) => rpcCliCall(config, "getnetworkinfo", []),
  getrawmempool: (config: Config) => rpcCliCall(config, "getrawmempool", []),
};

