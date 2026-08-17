import fs from "node:fs";
import path from "node:path";
import type { Config } from "./config";
import type { ChainStateRow } from "./db";
import { rpc } from "./rpcCli";
import { ALPHAPLUS_CODENAME, ALPHAPLUS_GENESIS_HASH, ALPHAPLUS_PRODUCT } from "./network";

/** Freeze baseline — display only; never derived or inferred at runtime. */
export const EQUIVALENCE_CLAIMED = false as const;

/** Closed testnet genesis (frozen infrastructure baseline). */
export const FROZEN_TESTNET_GENESIS_HASH =
  "dc14491196188457f5e2d9f5951e38b788a1c714dd606225b7490dafa9ef8bca";

export { ALPHAPLUS_CODENAME, ALPHAPLUS_GENESIS_HASH, ALPHAPLUS_PRODUCT };

const EXPECTED_TESTNET_CHAIN = "test";

export type ComplianceMismatches = {
  root_mismatch: boolean;
  chain_mismatch: boolean;
  genesis_mismatch: boolean;
  indexer_mismatch: boolean;
};

export type CompliancePayload = {
  chain_name: string;
  network_type: string;
  product: string;
  codename: string;
  network: string;
  genesis_hash: string;
  genesis_hash_short: string;
  explorer_version: string;
  build_timestamp: string;
  equivalence_claimed: false;
  mismatches: ComplianceMismatches;
  mismatch_active: boolean;
  mismatch_messages: string[];
  /** Operational (RPC busy / work queue). Not a chain-integrity mismatch. */
  rpc_degraded: boolean;
  rpc_messages: string[];
};

function shortHash(hash: string): string {
  if (!hash || hash.length < 20) return hash;
  return `${hash.slice(0, 12)}…${hash.slice(-6)}`;
}

function readExplorerVersion(): string {
  try {
    const pkgPath = path.join(__dirname, "..", "package.json");
    const raw = fs.readFileSync(pkgPath, "utf8");
    const pkg = JSON.parse(raw) as { version?: string };
    return pkg.version ?? "0.0.0";
  } catch {
    return "0.0.0";
  }
}

function chainDisplayName(config: Config): string {
  if (process.env.EXPLORER_CHAIN_NAME) return process.env.EXPLORER_CHAIN_NAME;
  if (config.network === "mainnet") return ALPHAPLUS_PRODUCT;
  if (config.network === "testnet") return "Rigor testnet (closed)";
  return `Rigor ${config.network}`;
}

function networkTypeLabel(config: Config): string {
  if (process.env.EXPLORER_NETWORK_TYPE) return process.env.EXPLORER_NETWORK_TYPE;
  if (config.network === "mainnet") return ALPHAPLUS_CODENAME;
  if (config.network === "testnet") return "closed testnet";
  return config.network;
}

function expectedGenesisHash(config: Config): string | null {
  const fromEnv = process.env.EXPECTED_GENESIS_HASH?.trim();
  if (fromEnv) return fromEnv;
  if (config.network === "testnet") return FROZEN_TESTNET_GENESIS_HASH;
  if (config.network === "mainnet") {
    return process.env.EXPECTED_MAINNET_GENESIS_HASH?.trim() || ALPHAPLUS_GENESIS_HASH;
  }
  return process.env.EXPECTED_MAINNET_GENESIS_HASH?.trim() ?? null;
}

/** Lag inside one draw window is normal while the indexer polls. */
export const INDEXER_LAG_CRITICAL_BLOCKS = 32;

export function isRpcWorkQueueError(err: unknown): boolean {
  return /Work queue depth exceeded/i.test(String(err));
}

/** Integrity CRITICAL vs operational RPC busy. */
export function isIntegrityIndexerLag(lag: number): boolean {
  return lag > INDEXER_LAG_CRITICAL_BLOCKS;
}

export async function buildCompliancePayload(config: Config, chainState: ChainStateRow): Promise<CompliancePayload> {
  const mismatch_messages: string[] = [];
  const rpc_messages: string[] = [];
  let chain_mismatch = false;
  let genesis_mismatch = false;
  let indexer_mismatch = false;
  let rpc_degraded = false;

  const root_mismatch = Boolean(chainState.critical_root_mismatch);

  const expectedGenesis = expectedGenesisHash(config);
  let genesis_hash = expectedGenesis ?? "";
  let nodeChain: string | null = null;
  let nodeHeight = -1;
  let nodeHashAtIndexerHeight: string | null = null;

  try {
    const blockchainInfo = await rpc.getblockchaininfo(config);
    nodeChain = typeof blockchainInfo?.chain === "string" ? blockchainInfo.chain : null;
    nodeHeight = Number(blockchainInfo?.blocks ?? -1);

    try {
      const genesisFromRpc = await rpc.getblockhash(config, 0);
      genesis_hash = typeof genesisFromRpc === "string" ? genesisFromRpc : String(genesisFromRpc ?? genesis_hash);
      if (expectedGenesis && genesis_hash && genesis_hash !== expectedGenesis) {
        genesis_mismatch = true;
        mismatch_messages.push(
          `Genesis mismatch: node reports ${shortHash(genesis_hash)}; expected ${shortHash(expectedGenesis)}.`,
        );
      }
    } catch (e) {
      rpc_degraded = true;
      rpc_messages.push(
        isRpcWorkQueueError(e)
          ? `Node RPC work queue is full (genesis hash check skipped).`
          : `RPC unavailable for genesis hash check: ${String(e)}`,
      );
    }

    if (config.network === "testnet" && nodeChain && nodeChain !== EXPECTED_TESTNET_CHAIN) {
      chain_mismatch = true;
      mismatch_messages.push(`Chain mismatch: node chain="${nodeChain}"; expected "${EXPECTED_TESTNET_CHAIN}".`);
    }
  } catch (e) {
    rpc_degraded = true;
    const detail = String(e);
    rpc_messages.push(
      isRpcWorkQueueError(e)
        ? `Node RPC work queue is full (getblockhash/getblockchaininfo skipped). This is load, not a chain fork. ${detail}`
        : `RPC unavailable for compliance checks: ${detail}`,
    );
  }

  const indexerHeight = Number(chainState.last_height);
  const indexerTipHash = chainState.last_block_hash;

  if (nodeHeight >= 0 && indexerHeight >= 0) {
    const lag = Math.max(0, nodeHeight - indexerHeight);
    if (lag > 0 && isIntegrityIndexerLag(lag)) {
      indexer_mismatch = true;
      mismatch_messages.push(`Indexer lag: node height ${nodeHeight}, indexer height ${indexerHeight} (${lag} blocks behind).`);
    }
  }

  if (indexerHeight >= 0 && indexerTipHash && nodeHeight >= indexerHeight) {
    try {
      nodeHashAtIndexerHeight = await rpc.getblockhash(config, indexerHeight);
      if (nodeHashAtIndexerHeight && nodeHashAtIndexerHeight !== indexerTipHash) {
        indexer_mismatch = true;
        mismatch_messages.push(
          `Indexer tip mismatch at height ${indexerHeight}: DB hash ${shortHash(indexerTipHash)} ≠ node ${shortHash(nodeHashAtIndexerHeight)}.`,
        );
      }
    } catch (e) {
      rpc_degraded = true;
      rpc_messages.push(
        isRpcWorkQueueError(e)
          ? `Node RPC work queue is full (indexer tip hash check skipped).`
          : `RPC unavailable for indexer tip check: ${String(e)}`,
      );
    }
  }

  if (root_mismatch) {
    mismatch_messages.unshift(
      "Root mismatch: this block's draw_candidate_root_committed ≠ parent draw_candidate_root_computed (A-8 pay-link).",
    );
  }

  const mismatches: ComplianceMismatches = {
    root_mismatch,
    chain_mismatch,
    genesis_mismatch,
    indexer_mismatch,
  };

  const mismatch_active = root_mismatch || chain_mismatch || genesis_mismatch || indexer_mismatch;

  return {
    chain_name: chainDisplayName(config),
    network_type: networkTypeLabel(config),
    product: config.network === "mainnet" ? ALPHAPLUS_PRODUCT : chainDisplayName(config),
    codename: config.network === "mainnet" ? ALPHAPLUS_CODENAME : networkTypeLabel(config),
    network: chainState.network,
    genesis_hash,
    genesis_hash_short: shortHash(genesis_hash),
    explorer_version: readExplorerVersion(),
    build_timestamp: process.env.EXPLORER_BUILD_TIMESTAMP ?? "development",
    equivalence_claimed: EQUIVALENCE_CLAIMED,
    mismatches,
    mismatch_active,
    mismatch_messages,
    rpc_degraded,
    rpc_messages,
  };
}
