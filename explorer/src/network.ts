/** AlphaPlus / network display and address HRP. Explorer-only; no consensus math. */

export const ALPHAPLUS_PRODUCT = "RIGOR v0.1.1-alphaplus";
export const ALPHAPLUS_CODENAME = "AlphaPlus Consensus Freeze";
export const ALPHAPLUS_GENESIS_HASH =
  "1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758";

/** Default JSON-RPC ports from core/src/chainparamsbase.cpp — never Bitcoin 8332. */
export const DEFAULT_RPC_PORT: Record<string, number> = {
  mainnet: 8761,
  testnet: 18761,
  regtest: 28761,
  signet: 38761,
  devtest: 48761,
};

export type ExplorerNetwork = "mainnet" | "testnet" | "regtest" | "signet" | "devtest";

export function parseExplorerNetwork(raw: string | undefined): ExplorerNetwork {
  const v = (raw ?? "mainnet").trim().toLowerCase();
  if (v === "testnet" || v === "testnet3") return "testnet";
  if (v === "regtest") return "regtest";
  if (v === "signet") return "signet";
  if (v === "devtest") return "devtest";
  return "mainnet";
}

/**
 * Bech32 HRP used when encoding scriptPubKey → address.
 * Mainnet: rg. Regtest/devtest: trg. Testnet: BECH32_HRP or trg.
 */
export function bech32HrpForNetwork(network: ExplorerNetwork, override?: string | null): string {
  const fromEnv = (override ?? "").trim().toLowerCase();
  if (fromEnv) return fromEnv;
  if (network === "mainnet") return "rg";
  return "trg";
}

export function defaultRpcPort(network: ExplorerNetwork, override?: string | null): number {
  const raw = (override ?? "").trim();
  if (raw) {
    const n = parseInt(raw, 10);
    if (Number.isFinite(n) && n > 0) return n;
  }
  return DEFAULT_RPC_PORT[network] ?? DEFAULT_RPC_PORT.mainnet;
}

/** rigor-cli chain flags. Do not pass -mainnet (unregistered). */
export function rigorCliChainArgs(network: ExplorerNetwork): string[] {
  if (network === "testnet") return ["-testnet"];
  if (network === "regtest") return ["-regtest"];
  if (network === "signet") return ["-signet"];
  if (network === "devtest") return ["-devtest"];
  return [];
}

export function isZeroHash(hex: string | null | undefined): boolean {
  if (!hex) return true;
  return /^0+$/.test(hex.trim().toLowerCase().replace(/^0x/, ""));
}
