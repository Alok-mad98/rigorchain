/** Fail-closed explorer identity. Observer-only; does not compute consensus. */

import type { Config } from "./config";
import type { Pool } from "pg";
import { getOrCreateChainState, stampIssuanceGeneration } from "./db";
import { ISSUANCE_GENERATION } from "./resIssuanceSchedule";
import {
  BECH32_HRP,
  RPC_CHAIN_ID,
  expectedGenesisForNetwork,
  type ExplorerNetwork,
} from "./network";
import { rpc } from "./rpcCli";

export class StartupClosedError extends Error {
  readonly errors: string[];
  constructor(errors: string[]) {
    super(`Explorer startup refused:\n- ${errors.join("\n- ")}`);
    this.name = "StartupClosedError";
    this.errors = errors;
  }
}

export type NodeIdentityProbe = {
  getblockchaininfo: (config: Config) => Promise<{ chain?: unknown }>;
  getblockhash: (config: Config, height: number | string) => Promise<unknown>;
};

export type StartupIdentityInput = {
  network: ExplorerNetwork;
  bech32Hrp: string;
  nodeChain: string | null;
  nodeGenesis: string | null;
  expectedGenesis: string | null;
  dbNetwork: string | null;
};

export type IssuanceGenerationInput = {
  storedGeneration: string | null;
  lastHeight: number;
  expectedGeneration: string;
};

/** Refuse old indexer rows that still advertise RFC-019 613454 / frozen 1024. */
export function evaluateIssuanceGeneration(input: IssuanceGenerationInput): { ok: boolean; errors: string[]; shouldStamp: boolean } {
  const stored = input.storedGeneration?.trim() || null;
  const expected = input.expectedGeneration;
  const emptyIndex = !Number.isFinite(input.lastHeight) || input.lastHeight < 0;

  if (stored !== expected) {
    if (emptyIndex) {
      return { ok: true, errors: [], shouldStamp: true };
    }
    const why = stored
      ? `db="${stored}"; explorer expects "${expected}"`
      : `Indexer DB has no issuance_generation but last_height=${input.lastHeight}`;
    return {
      ok: false,
      errors: [
        `Issuance generation mismatch: ${why}. Reindex under ${expected}; do not serve 1024 / 613454 cache pages.`,
      ],
      shouldStamp: false,
    };
  }

  return { ok: true, errors: [], shouldStamp: false };
}

function shortHash(hash: string): string {
  if (!hash || hash.length < 20) return hash;
  return `${hash.slice(0, 12)}…${hash.slice(-6)}`;
}

function normHash(h: string | null): string | null {
  if (h == null) return null;
  const s = h.trim().toLowerCase().replace(/^0x/, "");
  return s || null;
}

/** Pure identity check. Unit-tested; startup and compliance share these rules. */
export function evaluateStartupIdentity(input: StartupIdentityInput): { ok: boolean; errors: string[] } {
  const errors: string[] = [];
  const canonicalHrp = BECH32_HRP[input.network];
  if (input.bech32Hrp !== canonicalHrp) {
    errors.push(
      `HRP mismatch: explorer="${input.bech32Hrp}"; ${input.network} requires "${canonicalHrp}".`,
    );
  }

  const expectedChain = RPC_CHAIN_ID[input.network];
  if (!input.nodeChain) {
    errors.push("Node chain id missing (getblockchaininfo.chain).");
  } else if (input.nodeChain !== expectedChain) {
    errors.push(
      `STARTUP FAILED. NETWORK IDENTITY MISMATCH. Chain mismatch: node chain="${input.nodeChain}"; RIGOR_NETWORK=${input.network} expects "${expectedChain}".`,
    );
  }

  const nodeGenesis = normHash(input.nodeGenesis);
  if (!nodeGenesis) {
    errors.push("Node genesis hash missing (getblockhash 0).");
  }

  const expectedGenesis = normHash(input.expectedGenesis);
  if (expectedGenesis && nodeGenesis && expectedGenesis !== nodeGenesis) {
    errors.push(
      `Genesis mismatch: node reports ${shortHash(nodeGenesis)}; expected ${shortHash(expectedGenesis)}.`,
    );
  }

  if (input.dbNetwork && input.dbNetwork !== input.network) {
    errors.push(
      `Indexer DB network="${input.dbNetwork}" does not match RIGOR_NETWORK=${input.network}. Use a separate database or reindex.`,
    );
  }

  return { ok: errors.length === 0, errors };
}

export async function assertFailClosedStartup(
  config: Config,
  pool: Pool,
  probe: NodeIdentityProbe = rpc,
): Promise<void> {
  let nodeChain: string | null = null;
  let nodeGenesis: string | null = null;
  try {
    const info = await probe.getblockchaininfo(config);
    nodeChain = typeof info?.chain === "string" ? info.chain : null;
  } catch (e) {
    throw new StartupClosedError([`Node RPC unreachable for getblockchaininfo: ${String(e)}`]);
  }
  try {
    const genesis = await probe.getblockhash(config, 0);
    nodeGenesis = typeof genesis === "string" ? genesis : genesis != null ? String(genesis) : null;
  } catch (e) {
    throw new StartupClosedError([`Node RPC unreachable for getblockhash 0: ${String(e)}`]);
  }

  const chain = await getOrCreateChainState(pool, config);
  const result = evaluateStartupIdentity({
    network: config.network,
    bech32Hrp: config.bech32Hrp,
    nodeChain,
    nodeGenesis,
    expectedGenesis: expectedGenesisForNetwork(config.network),
    dbNetwork: chain.network ?? null,
  });
  const issuance = evaluateIssuanceGeneration({
    storedGeneration: chain.issuance_generation ?? null,
    lastHeight: Number(chain.last_height),
    expectedGeneration: ISSUANCE_GENERATION,
  });
  const errors = [...result.errors, ...issuance.errors];
  if (errors.length > 0) throw new StartupClosedError(errors);
  if (issuance.shouldStamp) {
    await stampIssuanceGeneration(pool, config, ISSUANCE_GENERATION);
  }
}
