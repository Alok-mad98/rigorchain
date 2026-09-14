/**
 * Explorer identity helper — display / index storage only.
 *
 * UI name: Observational fingerprint. Historical short ID; RFC-019 retired the draw.
 * Storage/API key remains `consensus_fingerprint` (frozen 104-byte layout).
 * Not a consensus rule, not a block-acceptance predicate, not an auto-reject signal.
 *
 * Does NOT recompute ConvergenceRoot or issuance. Node-authored RPC fields stay
 * the source. This module either:
 *   1. stores a node-published `consensus_fingerprint`, or
 *   2. applies the frozen Hash256 identity over those published fields
 *      (same layout as core/src/consensus/fingerprint.cpp).
 */

import { createHash } from "node:crypto";
import { optionalHash, optionalString } from "./rpcFields";

/** Hash256(C ‖ winner ‖ σ ‖ height ‖ parent nBits) — observational short ID. Draw retired. */
export const DRAW_FINGERPRINT_LABEL = "Observational fingerprint";
export const DRAW_IDENTITY_HEADING = "Block identity";
export const FINGERPRINT_NOT_AVAILABLE = "Not available";
export const FINGERPRINT_NA = "N/A";

const HEX64 = /^[0-9a-f]{64}$/i;
const HEX8 = /^[0-9a-f]{8}$/i;

export function displayFingerprint(fp: string | null | undefined): string {
  const s = (fp ?? "").trim();
  return s ? s : FINGERPRINT_NOT_AVAILABLE;
}

export function fingerprintStatus(fp: string | null | undefined): "Available" | "Not available" {
  return (fp ?? "").trim() ? "Available" : "Not available";
}

export function shortFingerprint(fp: string | null | undefined): string {
  const s = (fp ?? "").trim();
  if (!s) return FINGERPRINT_NA;
  return s.length > 16 ? `${s.slice(0, 8)}…` : s;
}

export function hash256(data: Uint8Array): Buffer {
  const inner = createHash("sha256").update(data).digest();
  return createHash("sha256").update(inner).digest();
}

export function parseBitsToUint32(bits: unknown): number | null {
  if (bits === null || bits === undefined || bits === "") return null;
  if (typeof bits === "number" && Number.isFinite(bits)) {
    return bits >>> 0;
  }
  const s = String(bits).trim().replace(/^0x/i, "");
  if (HEX8.test(s)) return Number.parseInt(s, 16) >>> 0;
  if (/^\d+$/.test(s)) {
    const n = Number(s);
    if (!Number.isFinite(n) || n > 0xffffffff) return null;
    return n >>> 0;
  }
  return null;
}

/** Digest-order / native 32 bytes (HexStr / uint256.begin). */
export function nativeHexToBytes(hex: string): Buffer | null {
  const s = hex.trim().replace(/^0x/i, "");
  if (!HEX64.test(s)) return null;
  return Buffer.from(s, "hex");
}

/** Bitcoin Core GetHex → native begin() bytes (byte reversal). */
export function rpcHashToNative(hex: string): Buffer | null {
  const raw = nativeHexToBytes(hex);
  if (!raw) return null;
  return Buffer.from(raw).reverse();
}

export function computeConsensusFingerprintNative(args: {
  convergenceRootNativeHex: string;
  winnerTxidNativeHex: string;
  sigmaNativeHex: string;
  height: number;
  targetPrev: number;
}): string | null {
  const c = nativeHexToBytes(args.convergenceRootNativeHex);
  const w = nativeHexToBytes(args.winnerTxidNativeHex);
  const s = nativeHexToBytes(args.sigmaNativeHex);
  if (!c || !w || !s) return null;
  if (!Number.isInteger(args.height) || args.height < 0 || args.height > 0xffffffff) return null;
  if (!Number.isInteger(args.targetPrev) || args.targetPrev < 0 || args.targetPrev > 0xffffffff) return null;

  const buf = Buffer.alloc(104);
  c.copy(buf, 0);
  w.copy(buf, 32);
  s.copy(buf, 64);
  buf.writeUInt32LE(args.height >>> 0, 96);
  buf.writeUInt32LE(args.targetPrev >>> 0, 100);
  return hash256(buf).toString("hex");
}

export type FingerprintCollectInput = {
  /** Node-published identity, if the RPC already emits it. */
  published?: unknown;
  convergenceRootRpc?: unknown;
  winnerTxidRpc?: unknown;
  sigmaRpc?: unknown;
  height?: unknown;
  targetPrevBits?: unknown;
};

/**
 * Indexer-layer collect. Prefers a node-published fingerprint.
 * Otherwise Hash256 of published RPC fields. Returns null when inputs are incomplete
 * (genesis / pre-migration rows → UI "Not available").
 */
export function collectConsensusFingerprint(input: FingerprintCollectInput): string | null {
  const published = optionalString(input.published);
  if (published && HEX64.test(published.replace(/^0x/i, ""))) {
    return published.replace(/^0x/i, "").toLowerCase();
  }

  const cRpc = optionalHash(input.convergenceRootRpc);
  const wRpc = optionalHash(input.winnerTxidRpc);
  const sRpc = optionalHash(input.sigmaRpc);
  if (!cRpc || !wRpc || !sRpc) return null;

  const heightNum = Number(input.height);
  if (!Number.isInteger(heightNum) || heightNum < 0 || heightNum > 0xffffffff) return null;

  const targetPrev = parseBitsToUint32(input.targetPrevBits);
  if (targetPrev === null) return null;

  const c = rpcHashToNative(cRpc);
  const w = rpcHashToNative(wRpc);
  const s = rpcHashToNative(sRpc);
  if (!c || !w || !s) return null;

  const buf = Buffer.alloc(104);
  c.copy(buf, 0);
  w.copy(buf, 32);
  s.copy(buf, 64);
  buf.writeUInt32LE(heightNum >>> 0, 96);
  buf.writeUInt32LE(targetPrev >>> 0, 100);
  return hash256(buf).toString("hex");
}

export function winnerSigmaFromCandidates(winnerTxid: string, candidates: unknown): string | null {
  if (!winnerTxid || !Array.isArray(candidates)) return null;
  const want = winnerTxid.toLowerCase();
  for (const raw of candidates) {
    if (!raw || typeof raw !== "object") continue;
    const row = raw as Record<string, unknown>;
    const txid = optionalString(row.txid);
    if (!txid || txid.toLowerCase() !== want) continue;
    return optionalHash(row.h ?? row.sigma ?? row.selection_digest_h);
  }
  return null;
}
