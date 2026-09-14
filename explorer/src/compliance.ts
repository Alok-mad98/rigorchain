import fs from "node:fs";
import path from "node:path";
import type { Config } from "./config";
import type { ChainStateRow } from "./db";
import { rpc } from "./rpcCli";
import {
  MAINNET_GENESIS_HASH,
  RIGOR_CHAIN_NAME,
  RIGOR_CODENAME,
  RIGOR_PRODUCT,
  RPC_CHAIN_ID,
  TESTNET_GENESIS_HASH,
  expectedGenesisForNetwork,
} from "./network";

/** Freeze baseline — display only; never derived or inferred at runtime. */
export const EQUIVALENCE_CLAIMED = false as const;

/** Node testnet genesis. Display/compliance only. */
export const FROZEN_TESTNET_GENESIS_HASH = TESTNET_GENESIS_HASH;

export {
  RIGOR_CODENAME,
  RIGOR_PRODUCT,
  MAINNET_GENESIS_HASH,
  RIGOR_CODENAME as ALPHAPLUS_CODENAME,
  MAINNET_GENESIS_HASH as ALPHAPLUS_GENESIS_HASH,
  RIGOR_PRODUCT as ALPHAPLUS_PRODUCT,
};

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
  /** Operational catch-up. Not a chain-integrity mismatch. */
  catchup_active: boolean;
  catchup_messages: string[];
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
  return RIGOR_CHAIN_NAME;
}

function networkTypeLabel(config: Config): string {
  if (process.env.EXPLORER_NETWORK_TYPE) return process.env.EXPLORER_NETWORK_TYPE;
  return config.network;
}

/** RFC-019 retired the 32-block draw window. Catch-up lag is never integrity CRITICAL. */
export const INDEXER_LAG_CRITICAL_BLOCKS = 0;

export function isRpcWorkQueueError(err: unknown): boolean {
  return /Work queue depth exceeded/i.test(String(err));
}

/** Lag alone is not a fork. Tip-hash mismatch at the indexed height is. */
export function isIntegrityIndexerLag(_lag: number): boolean {
  return false;
}

export function isCatchupIndexerLag(lag: number): boolean {
  return Number.isFinite(lag) && lag > 0;
}

export async function buildCompliancePayload(config: Config, chainState: ChainStateRow): Promise<CompliancePayload> {
  const mismatch_messages: string[] = [];
  const rpc_messages: string[] = [];
  const catchup_messages: string[] = [];
  let chain_mismatch = false;
  let genesis_mismatch = false;
  let indexer_mismatch = false;
  let rpc_degraded = false;
  let catchup_active = false;

  const root_mismatch = Boolean(chainState.critical_root_mismatch);

  const expectedGenesis = expectedGenesisForNetwork(config.network);
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

    const expectedChain = RPC_CHAIN_ID[config.network];
    if (nodeChain && nodeChain !== expectedChain) {
      chain_mismatch = true;
      mismatch_messages.push(
        `Chain mismatch: node chain="${nodeChain}"; expected "${expectedChain}" for RIGOR_NETWORK=${config.network}.`,
      );
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
    if (isCatchupIndexerLag(lag)) {
      catchup_active = true;
      catchup_messages.push(
        `Indexer catching up: node height ${nodeHeight}, indexer height ${indexerHeight} (${lag} blocks behind). Not a chain fork.`,
      );
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
    product: RIGOR_PRODUCT,
    codename: RIGOR_CODENAME,
    network: chainState.network,
    genesis_hash,
    genesis_hash_short: shortHash(genesis_hash),
    explorer_version: readExplorerVersion(),
    build_timestamp: process.env.EXPLORER_BUILD_TIMESTAMP ?? "development",
    equivalence_claimed: EQUIVALENCE_CLAIMED,
    mismatches,
    mismatch_active,
    mismatch_messages,
    catchup_active,
    catchup_messages,
    rpc_degraded,
    rpc_messages,
  };
}
