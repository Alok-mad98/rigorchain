/** Production network display and address HRP. Explorer-only; no consensus math. */

export const RIGOR_PRODUCT = "RIGOR v0.1.3";
export const RIGOR_CODENAME = "Production Release";
/** `Consensus::CHAIN_NAME` in rigor_consensus_lock.h — not the product tag. */
export const RIGOR_CHAIN_NAME = "RIGOR";
export const MAINNET_GENESIS_HASH =
  "f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717";

/** Node testnet genesis (chainparams.cpp). Display/compliance only. */
export const TESTNET_GENESIS_HASH =
  "0aca439758fba7fa5190b7e1cf37f9447516ef4cade07a49e7d484ea5b67b9ce";

/** Default JSON-RPC ports from core/src/chainparamsbase.cpp — never Bitcoin 8332. */
export const DEFAULT_RPC_PORT: Record<string, number> = {
  mainnet: 8761,
  testnet: 18761,
  regtest: 28761,
  devtest: 48761,
};

export type ExplorerNetwork = "mainnet" | "testnet" | "regtest" | "devtest";

/**
 * Bech32 HRP locked to node chainparams / rigor_consensus_lock.h.
 * Mainnet is `rg` (not `rgr`). `rgrt` is regtest only.
 */
export const BECH32_HRP: Record<ExplorerNetwork, string> = {
  mainnet: "rg",
  testnet: "trg",
  regtest: "rgrt",
  devtest: "dvt",
};

/** getblockchaininfo.chain — CBaseChainParams network IDs, not explorer labels. */
export const RPC_CHAIN_ID: Record<ExplorerNetwork, string> = {
  mainnet: "main",
  testnet: "test",
  regtest: "regtest",
  devtest: "devtest",
};

/**
 * Frozen genesis for public networks. regtest/devtest are not frozen;
 * pin them with EXPECTED_GENESIS_HASH when you need a specific datadir.
 */
export function expectedGenesisForNetwork(
  network: ExplorerNetwork,
  env: NodeJS.ProcessEnv = process.env,
): string | null {
  const pinned = env.EXPECTED_GENESIS_HASH?.trim();
  if (pinned) return pinned;
  if (network === "mainnet") {
    return env.EXPECTED_MAINNET_GENESIS_HASH?.trim() || MAINNET_GENESIS_HASH;
  }
  if (network === "testnet") return TESTNET_GENESIS_HASH;
  return null;
}

export function parseExplorerNetwork(raw: string | undefined): ExplorerNetwork {
  const v = (raw ?? "mainnet").trim().toLowerCase();
  if (v === "mainnet" || v === "main") return "mainnet";
  if (v === "testnet" || v === "testnet3" || v === "test") return "testnet";
  if (v === "regtest") return "regtest";
  if (v === "devtest") return "devtest";
  throw new Error(`Unknown RIGOR_NETWORK="${raw}". Expected mainnet|testnet|regtest|devtest.`);
}

/** Startup/config: never silently default an unset or empty network to mainnet. */
export function requireExplorerNetwork(raw: string | undefined): ExplorerNetwork {
  if (raw == null || !String(raw).trim()) {
    throw new Error(
      "RIGOR_NETWORK is required (mainnet|testnet|regtest|devtest). Refusing to default to mainnet.",
    );
  }
  return parseExplorerNetwork(raw);
}

/**
 * Bech32 HRP used when encoding scriptPubKey → address.
 * Must match chainparams.cpp. Do not change protocol HRP here.
 */
export function bech32HrpForNetwork(network: ExplorerNetwork, override?: string | null): string {
  const expected = BECH32_HRP[network];
  const fromEnv = (override ?? "").trim().toLowerCase();
  if (fromEnv && fromEnv !== expected) {
    throw new Error(
      `BECH32_HRP="${fromEnv}" does not match ${network} canonical HRP "${expected}".`,
    );
  }
  return expected;
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
  if (network === "devtest") return ["-devtest"];
  return [];
}

export function isZeroHash(hex: string | null | undefined): boolean {
  if (!hex) return true;
  return /^0+$/.test(hex.trim().toLowerCase().replace(/^0x/, ""));
}

/** @deprecated Use RIGOR_PRODUCT. Kept so leftover imports fail closed on identity. */
export const ALPHAPLUS_PRODUCT = RIGOR_PRODUCT;
/** @deprecated Use RIGOR_CODENAME. */
export const ALPHAPLUS_CODENAME = RIGOR_CODENAME;
/** @deprecated Use MAINNET_GENESIS_HASH. */
export const ALPHAPLUS_GENESIS_HASH = MAINNET_GENESIS_HASH;
