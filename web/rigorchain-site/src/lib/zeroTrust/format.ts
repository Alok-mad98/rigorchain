import { ORU_COIN, RES_BASE } from "./constants";
import { bytesToHex, nativeToDisplayHex } from "./bytes";

export function formatOru(atoms: bigint): string {
  const neg = atoms < 0n;
  const a = neg ? -atoms : atoms;
  const int = a / ORU_COIN;
  const frac = a % ORU_COIN;
  return `${neg ? "-" : ""}${int}.${frac.toString().padStart(8, "0")} ORU`;
}

export function formatRes(atoms: bigint): string {
  const neg = atoms < 0n;
  const a = neg ? -atoms : atoms;
  const int = a / RES_BASE;
  const frac = a % RES_BASE;
  return `${neg ? "-" : ""}${int}.${frac.toString().padStart(2, "0")} RES`;
}

export function formatFee(atoms: bigint | null): string {
  if (atoms == null) return "—";
  return formatOru(atoms);
}

export function digestHex(native: Uint8Array): string {
  return bytesToHex(native);
}

export function displayHex(native: Uint8Array): string {
  return nativeToDisplayHex(native);
}

export function normalizeHex(hex: string): string {
  return hex.trim().toLowerCase().replace(/^0x/, "");
}

export function hexEqual(a: string | null | undefined, b: string | null | undefined): boolean {
  if (!a || !b) return false;
  return normalizeHex(a) === normalizeHex(b);
}
