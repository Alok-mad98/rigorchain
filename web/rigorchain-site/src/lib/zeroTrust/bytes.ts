/**
 * Binary helpers for RIGOR zero-trust verification.
 * Consensus math uses Uint8Array only — never hex-string concatenation.
 */

const HEX_RE = /^[0-9a-fA-F]+$/;

export function hexToBytes(hex: string): Uint8Array {
  const clean = hex.trim().replace(/^0x/i, "");
  if (clean.length === 0 || clean.length % 2 !== 0 || !HEX_RE.test(clean)) {
    throw new Error("invalid hex");
  }
  const out = new Uint8Array(clean.length / 2);
  for (let i = 0; i < out.length; i++) {
    out[i] = parseInt(clean.slice(i * 2, i * 2 + 2), 16);
  }
  return out;
}

export function bytesToHex(bytes: Uint8Array): string {
  let out = "";
  for (let i = 0; i < bytes.length; i++) {
    out += bytes[i].toString(16).padStart(2, "0");
  }
  return out;
}

/** Little-endian integer value of a byte string (native uint256 order). */
export function bytesToBigInt(bytes: Uint8Array): bigint {
  let n = 0n;
  for (let i = bytes.length - 1; i >= 0; i--) {
    n = (n << 8n) | BigInt(bytes[i]);
  }
  return n;
}

export function compareBytes(a: Uint8Array, b: Uint8Array): number {
  const n = Math.min(a.length, b.length);
  for (let i = 0; i < n; i++) {
    if (a[i] !== b[i]) return a[i] < b[i] ? -1 : 1;
  }
  if (a.length === b.length) return 0;
  return a.length < b.length ? -1 : 1;
}

export function equalBytes(a: Uint8Array, b: Uint8Array): boolean {
  return compareBytes(a, b) === 0;
}

export function xorBytes(a: Uint8Array, b: Uint8Array): Uint8Array {
  const n = Math.min(a.length, b.length);
  const out = new Uint8Array(n);
  for (let i = 0; i < n; i++) out[i] = a[i] ^ b[i];
  return out;
}

export function concatBytes(...parts: Uint8Array[]): Uint8Array {
  let len = 0;
  for (const p of parts) len += p.length;
  const out = new Uint8Array(len);
  let off = 0;
  for (const p of parts) {
    out.set(p, off);
    off += p.length;
  }
  return out;
}

export function reverseBytes(bytes: Uint8Array): Uint8Array {
  const out = new Uint8Array(bytes.length);
  for (let i = 0; i < bytes.length; i++) out[i] = bytes[bytes.length - 1 - i];
  return out;
}

/** RPC / explorer display hex (GetHex) → uint256 native bytes. */
export function displayHexToNative(hex: string): Uint8Array {
  const bytes = hexToBytes(hex);
  if (bytes.length !== 32) {
    throw new Error(`expected 32-byte hash, got ${bytes.length}`);
  }
  return reverseBytes(bytes);
}

/** uint256 native bytes → RPC display hex. */
export function nativeToDisplayHex(native: Uint8Array): string {
  if (native.length !== 32) {
    throw new Error(`expected 32-byte hash, got ${native.length}`);
  }
  return bytesToHex(reverseBytes(native));
}

export function requireLen32(bytes: Uint8Array, label: string): Uint8Array {
  if (bytes.length !== 32) {
    throw new Error(`${label} must be 32 bytes`);
  }
  return bytes;
}

export function u32ToLeBytes(value: number): Uint8Array {
  if (!Number.isInteger(value) || value < 0 || value > 0xffffffff) {
    throw new Error("height must be a uint32");
  }
  const out = new Uint8Array(4);
  out[0] = value & 0xff;
  out[1] = (value >>> 8) & 0xff;
  out[2] = (value >>> 16) & 0xff;
  out[3] = (value >>> 24) & 0xff;
  return out;
}

export function zero32(): Uint8Array {
  return new Uint8Array(32);
}
