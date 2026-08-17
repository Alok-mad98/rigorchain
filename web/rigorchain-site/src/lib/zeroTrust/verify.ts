import { displayHexToNative, equalBytes, zero32 } from "./bytes";
import { computeConvergenceRoot, computeSigma, isEligible, selectWinner } from "./consensus";
import { ALPHAPLUS_GENESIS_HASH_DISPLAY } from "./constants";
import { displayHex, formatFee, formatOru, formatRes, hexEqual, normalizeHex } from "./format";
import type {
  BlockPublicInput,
  BlockVerificationResult,
  CheckStatus,
  FinalStatus,
  PipelineTrace,
  TxPublicInput,
  TxVerificationResult,
} from "./types";

function combineFinal(checks: CheckStatus[]): FinalStatus {
  const parts = checks.map((s) => (s === "NONE" ? "MATCH" : s));
  if (parts.includes("MISMATCH")) return "INVALID";
  if (parts.includes("INCOMPLETE") || parts.includes("UNKNOWN")) return "INCOMPLETE";
  return "VALID";
}

function genesisStatus(tipHash: string, height: number): CheckStatus {
  if (height === 0) {
    return hexEqual(tipHash, ALPHAPLUS_GENESIS_HASH_DISPLAY) ? "MATCH" : "MISMATCH";
  }
  return "NONE";
}

function parentHashStatus(previousHashDisplay: string | null, height: number): CheckStatus {
  if (height === 0) return "NONE";
  const prev = (previousHashDisplay ?? "").replace(/^0x/i, "");
  if (!prev || /^0+$/i.test(prev)) return "INCOMPLETE";
  return "MATCH";
}

async function runBlockPipeline(input: BlockPublicInput): Promise<{
  genesis: CheckStatus;
  parentHash: CheckStatus;
  convergenceRoot: CheckStatus;
  winner: CheckStatus;
  trace: PipelineTrace;
  notes: string[];
}> {
  const notes: string[] = [];
  const height = input.height;
  const prevHash = input.previousHashDisplay
    ? displayHexToNative(input.previousHashDisplay)
    : height === 0
      ? zero32()
      : null;

  let convergenceRoot = zero32();
  let convergenceRootStatus: CheckStatus = "INCOMPLETE";
  if (input.candidates.length > 0) {
    const txids = input.candidates.map((c) => displayHexToNative(c.txidDisplay));
    convergenceRoot = await computeConvergenceRoot(txids);
    if (input.committedRootDisplay) {
      const committed = displayHexToNative(input.committedRootDisplay);
      convergenceRootStatus = equalBytes(convergenceRoot, committed) ? "MATCH" : "MISMATCH";
      if (convergenceRootStatus === "MISMATCH") {
        notes.push("Computed ConvergenceRoot does not match the published coinbase / committed root.");
      }
    } else {
      notes.push("ConvergenceRoot computed locally from public F. No API root was used as a verdict.");
      convergenceRootStatus = "MATCH";
    }
  } else if (input.txidsDisplay.length >= 2) {
    notes.push("Candidate set F was not published. ConvergenceRoot was not inferred from raw tx order.");
    convergenceRootStatus = "INCOMPLETE";
  } else {
    convergenceRoot = await computeConvergenceRoot([]);
    if (input.committedRootDisplay) {
      const committed = displayHexToNative(input.committedRootDisplay);
      convergenceRootStatus = equalBytes(convergenceRoot, committed) ? "MATCH" : "INCOMPLETE";
    } else {
      convergenceRootStatus = "INCOMPLETE";
    }
    notes.push("Published F is empty or missing — ConvergenceRoot boundary is the all-zero root when |F| < 2.");
  }

  const sigmas: PipelineTrace["sigmas"] = [];
  let winnerTxid: string | null = null;
  let winnerStatus: CheckStatus = "INCOMPLETE";

  if (prevHash && input.candidates.length > 0) {
    const payoutHeight = height;
    const prepared = [];
    for (const c of input.candidates) {
      const txidNative = displayHexToNative(c.txidDisplay);
      const sigma = await computeSigma(txidNative, convergenceRoot, prevHash, payoutHeight);
      const eligible = isEligible(sigma, c.weight);
      prepared.push({ txidNative, sigmaNative: sigma, weight: c.weight, txidDisplay: c.txidDisplay });
      sigmas.push({
        txidDisplay: normalizeHex(c.txidDisplay),
        sigmaDisplay: displayHex(sigma),
        eligible,
        weight: c.weight.toString(),
      });
    }
    const idx = selectWinner(prepared);
    winnerTxid = idx >= 0 ? normalizeHex(prepared[idx].txidDisplay) : null;
    if (input.publishedWinnerDisplay && normalizeHex(input.publishedWinnerDisplay) !== "0".repeat(64)) {
      winnerStatus = winnerTxid && hexEqual(winnerTxid, input.publishedWinnerDisplay) ? "MATCH" : "MISMATCH";
      if (winnerStatus === "MISMATCH") {
        notes.push("Locally selected winner does not match the published rigor_winner field.");
      }
    } else if (winnerTxid) {
      notes.push("Winner selected locally from public F. Published winner was absent or zero.");
      winnerStatus = "MATCH";
    } else {
      winnerStatus = "NONE";
    }
  } else if (input.candidates.length === 0) {
    winnerStatus = input.publishedWinnerDisplay && normalizeHex(input.publishedWinnerDisplay) !== "0".repeat(64)
      ? "MISMATCH"
      : "NONE";
  } else {
    notes.push("Previous block hash missing — Sigma / winner cannot be computed.");
  }

  const genesis = genesisStatus(input.hashDisplay, height);
  const parentHash = parentHashStatus(input.previousHashDisplay, height);
  if (height === 0 && genesis === "MATCH") {
    notes.push("Tip hash matches the published AlphaPlus genesis. Parent hash is empty at height 0.");
  }

  return {
    genesis,
    parentHash,
    convergenceRoot: convergenceRootStatus,
    winner: winnerStatus,
    trace: {
      convergenceRootDisplay: displayHex(convergenceRoot),
      sigmas,
      winnerTxidDisplay: winnerTxid,
      previousHashDisplay: input.previousHashDisplay,
    },
    notes,
  };
}

export async function verifyBlock(input: BlockPublicInput): Promise<BlockVerificationResult> {
  const pipeline = await runBlockPipeline(input);
  const final = combineFinal([
    pipeline.genesis,
    pipeline.parentHash,
    pipeline.convergenceRoot,
    pipeline.winner === "NONE" ? "MATCH" : pipeline.winner,
  ]);
  return {
    mode: "block",
    height: input.height,
    genesis: pipeline.genesis,
    parentHash: pipeline.parentHash,
    convergenceRoot: pipeline.convergenceRoot,
    winner: pipeline.winner,
    final: final === "VALID" ? "VALID" : final,
    trace: pipeline.trace,
    notes: pipeline.notes,
    publicSource: input.source,
  };
}

export async function verifyTransaction(
  tx: TxPublicInput,
  block: BlockPublicInput | null,
): Promise<TxVerificationResult> {
  if (!block) {
    return {
      mode: "tx",
      transaction: "INCOMPLETE",
      blockHeight: tx.height,
      oru: formatOru(tx.amounts.oruAtoms),
      fee: formatFee(tx.amounts.feeAtoms),
      res: formatRes(tx.amounts.resAtoms),
      final: "INCOMPLETE",
      genesis: "UNKNOWN",
      parentHash: "INCOMPLETE",
      convergenceRoot: "INCOMPLETE",
      winner: "INCOMPLETE",
      trace: null,
      notes: ["Public block data for this TXID was not available. Paste raw block JSON for air-gapped verification."],
      publicSource: tx.source,
    };
  }

  const pipeline = await verifyBlock(block);
  const included = block.txidsDisplay.some((id) => hexEqual(id, tx.txidDisplay));
  const checks: CheckStatus[] = [
    pipeline.genesis,
    pipeline.parentHash,
    pipeline.convergenceRoot,
    pipeline.winner,
  ];
  const transaction: FinalStatus = !included
    ? "INVALID"
    : checks.includes("MISMATCH")
      ? "INVALID"
      : "VALID";

  const notes = [...pipeline.notes];
  if (!included) {
    notes.unshift("TXID is not present in the public transaction list of the referenced block.");
  }

  return {
    mode: "tx",
    transaction,
    blockHeight: block.height,
    oru: formatOru(tx.amounts.oruAtoms),
    fee: formatFee(tx.amounts.feeAtoms),
    res: formatRes(tx.amounts.resAtoms),
    final: transaction,
    genesis: pipeline.genesis,
    parentHash: pipeline.parentHash,
    convergenceRoot: pipeline.convergenceRoot,
    winner: pipeline.winner,
    trace: pipeline.trace,
    notes,
    publicSource: tx.source,
  };
}

export function parseRawBlockJson(raw: string): BlockPublicInput {
  const obj = JSON.parse(raw) as Record<string, unknown>;
  const hash = String(obj.hash ?? obj.block_hash ?? "");
  if (!hash) throw new Error("raw block JSON missing hash");
  const height = Number(obj.height ?? 0);
  const previous = obj.previousblockhash != null ? String(obj.previousblockhash) : null;

  const txField = obj.tx;
  const txids: string[] = [];
  const candidates: BlockPublicInput["candidates"] = [];

  if (Array.isArray(obj.convergence_txids)) {
    for (const id of obj.convergence_txids) {
      candidates.push({ txidDisplay: String(id), weight: 1n });
    }
  }

  if (Array.isArray(obj.candidates)) {
    candidates.length = 0;
    for (const c of obj.candidates as Record<string, unknown>[]) {
      const txid = String(c.txid ?? c.convergence_txid ?? "");
      const weight = BigInt(String(c.weight ?? c.fee ?? c.convergence_amount ?? 1));
      if (txid) candidates.push({ txidDisplay: txid, weight });
    }
  }

  const feeByTxid = new Map<string, bigint>();
  if (Array.isArray(txField)) {
    for (const tx of txField) {
      if (typeof tx === "string") {
        txids.push(tx);
      } else if (tx && typeof tx === "object") {
        const rec = tx as Record<string, unknown>;
        const txid = String(rec.txid ?? "");
        if (txid) txids.push(txid);
        if (txid && rec.fee != null) {
          feeByTxid.set(normalizeHex(txid), BigInt(Math.round(Math.abs(Number(rec.fee)) * 100_000_000)));
        }
      }
    }
  }
  for (const c of candidates) {
    const fee = feeByTxid.get(normalizeHex(c.txidDisplay));
    if (fee != null && fee > 0n && c.weight === 1n) c.weight = fee;
  }

  const committed =
    (obj.draw_candidate_root as string | undefined) ??
    (obj.convergence_root as string | undefined) ??
    (obj.committed_root as string | undefined) ??
    null;

  const publishedWinner = obj.rigor_winner != null ? String(obj.rigor_winner) : null;

  return {
    hashDisplay: hash,
    height,
    previousHashDisplay: previous,
    txidsDisplay: txids,
    candidates,
    committedRootDisplay: committed,
    publishedWinnerDisplay: publishedWinner,
    source: "user",
  };
}

export function parseRawTxAmounts(tx: Record<string, unknown>): { oruAtoms: bigint; resAtoms: bigint; feeAtoms: bigint | null } {
  let oru = 0n;
  let res = 0n;
  const vout = tx.vout;
  if (Array.isArray(vout)) {
    for (const o of vout as Record<string, unknown>[]) {
      if (o.value != null) {
        const coins = String(o.value);
        oru += BigInt(Math.round(Number(coins) * Number(100_000_000)));
      }
    }
  }
  let fee: bigint | null = null;
  if (tx.fee != null) {
    fee = BigInt(Math.round(Number(tx.fee) * Number(100_000_000)));
  }
  return { oruAtoms: oru, resAtoms: res, feeAtoms: fee };
}
