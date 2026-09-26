/** Defensive RPC field reads. Missing keys must not crash the importer or UI. */

export function asString(x: unknown): string {
  if (x === null || x === undefined) return "";
  return typeof x === "string" ? x : String(x);
}

export function optionalString(x: unknown): string | null {
  if (x === null || x === undefined) return null;
  const s = typeof x === "string" ? x : String(x);
  return s.length === 0 ? null : s;
}

export function optionalBigInt(x: unknown): bigint | null {
  if (x === null || x === undefined || x === "") return null;
  try {
    return BigInt(x as string | number | bigint);
  } catch {
    return null;
  }
}

export function displayOrNA(x: unknown): string {
  if (x === null || x === undefined || x === "") return "N/A";
  return String(x);
}

export function displayOrNotAvailable(x: unknown): string {
  if (x === null || x === undefined || x === "") return "Not available";
  return String(x);
}

/** Treat empty / all-zero hashes as absent (no winner, no bind). */
export function optionalHash(x: unknown): string | null {
  const s = optionalString(x);
  if (!s) return null;
  if (/^0+$/i.test(s.replace(/^0x/i, ""))) return null;
  return s;
}

/** Reverse a hex byte string (txid display ↔ native 32-byte witness push). */
export function reverseHexBytes(hex: string): string {
  const s = hex.toLowerCase().replace(/^0x/i, "");
  if (s.length % 2 !== 0 || /[^0-9a-f]/i.test(s)) return s;
  const bytes = s.match(/../g);
  if (!bytes) return s;
  return bytes.reverse().join("");
}

/**
 * Native 32-byte hex vs RPC display GetHex.
 * Accept either endianness so parent-winner matching does not drop the mint.
 */
export function witnessPushMatchesTxid(push: unknown, displayTxid: unknown): boolean {
  const p = asString(push).toLowerCase().replace(/^0x/i, "");
  const t = optionalHash(displayTxid)?.toLowerCase().replace(/^0x/i, "") ?? "";
  if (!p || !t) return false;
  return p === t || reverseHexBytes(p) === t;
}

/** Convert a 32-byte native witness push to RPC display txid order. */
export function witness32ToDisplayTxid(push: unknown): string {
  const p = asString(push).toLowerCase().replace(/^0x/i, "");
  if (p.length === 64) return reverseHexBytes(p);
  return p;
}

/** HashConcatenatedSortedTxids([]) — empty eligible set. Not the zero uint256. */
export const EMPTY_DRAW_ELIGIBLE_ROOT =
  "56944c5d3f98413ef45cf54545538103cc9f298e0575820ad3591376e2e0f65d";

function isZeroHex(hex: string): boolean {
  return /^0+$/i.test(hex.replace(/^0x/i, ""));
}

/**
 * Genesis RPC leaves draw_candidate_root_computed as uint256() (no pprev / no draw).
 * Height 1 coinbase commits the empty eligible-set root, which is what CreateNewBlock
 * pays when the parent awarded nothing. Treat parent-zero as that empty root.
 */
export function parentAwardRootForPayLink(parentComputed: string): string {
  const s = parentComputed.toLowerCase().replace(/^0x/i, "");
  if (!s || isZeroHex(s)) return EMPTY_DRAW_ELIGIBLE_ROOT;
  return s;
}

/**
 * A-8 pay-link check: this block's coinbase CVGE committed root is the draw it PAYS
 * (parent award). `draw_candidate_root_computed` is the draw this block AWARDS
 * (must appear as committed on height+1). Same-block committed ≠ computed is expected
 * on award and payout blocks and is not a consensus mismatch.
 */
export function isA8PayRootMismatch(
  thisCommitted: string,
  parentComputed: string | null | undefined,
  height: number,
): boolean {
  if (height <= 0) return false;
  if (parentComputed == null || parentComputed === "") return false;
  const paid = thisCommitted.toLowerCase().replace(/^0x/i, "");
  return paid !== parentAwardRootForPayLink(parentComputed);
}
