const POW10 = (n: number): bigint => {
  let r = 1n;
  for (let i = 0; i < n; i++) r *= 10n;
  return r;
};

/** ORU: 1 ORU = 1e8 atoms. */
export const ORU_DISPLAY_DECIMALS = 8;
export const ORU_COIN = 100000000n;

/** RES: 1 RES = 1e8 atoms. Same eight-decimal scale as ORU; identity is the asset name. */
export const RES_DISPLAY_DECIMALS = 8;
export const RES_COIN = 100000000n;

/**
 * Expand JSON/RPC scientific amounts (`1e-8`, `1.024e-5`) into fixed decimals.
 * Does not use IEEE floats — UniValue ValueFromAmount may emit this form for tiny outputs.
 */
export function expandScientificDecimal(value: string): string {
  const s = value.trim();
  const m = s.match(/^([+-]?)(\d+)(?:\.(\d+))?[eE]([+-]?\d+)$/);
  if (!m) return s;
  const sign = m[1] === "-" ? "-" : "";
  const intDigits = m[2];
  const fracDigits = m[3] ?? "";
  const exp = Number(m[4]);
  if (!Number.isSafeInteger(exp)) throw new Error(`invalid exponent: ${value}`);
  const digits = intDigits + fracDigits;
  const point = intDigits.length + exp;
  if (point <= 0) return `${sign}0.${"0".repeat(-point)}${digits}`;
  if (point >= digits.length) return `${sign}${digits}${"0".repeat(point - digits.length)}`;
  return `${sign}${digits.slice(0, point)}.${digits.slice(point)}`;
}

/**
 * Parse a fixed-decimal string into integer atoms deterministically.
 *
 * Expected shapes from RPC:
 * - ORU: 8 dp max (e.g. "1.00000000")
 * - RES: 8 dp max (e.g. "1.00000000" or "1e-8")
 */
export function fixedDecimalToAtoms(value: string, decimals: number): bigint {
  const s = expandScientificDecimal(value.trim());
  if (!s) throw new Error("empty decimal string");

  const negative = s.startsWith("-");
  const unsigned = negative ? s.slice(1) : s;

  const parts = unsigned.split(".");
  if (parts.length > 2) throw new Error(`invalid decimal: ${value}`);

  const intPart = parts[0] ?? "0";
  const fracPartRaw = parts[1] ?? "";

  if (!/^[0-9]+$/.test(intPart)) throw new Error(`invalid integer part: ${value}`);
  if (!/^[0-9]*$/.test(fracPartRaw)) throw new Error(`invalid fractional part: ${value}`);

  if (fracPartRaw.length > decimals) {
    throw new Error(`too many decimals for ${value}, max=${decimals}`);
  }

  const fracPadded = fracPartRaw.padEnd(decimals, "0");

  const intAtoms = BigInt(intPart) * POW10(decimals);
  const fracAtoms = BigInt(fracPadded);
  const atoms = intAtoms + fracAtoms;

  return negative ? -atoms : atoms;
}

export function formatFixedAtoms(atoms: bigint, decimals: number): string {
  const base = POW10(decimals);
  const neg = atoms < 0n;
  const a = neg ? -atoms : atoms;
  const int = a / base;
  const frac = a % base;
  return `${neg ? "-" : ""}${int}.${frac.toString().padStart(decimals, "0")}`;
}

export function oruStringToAtoms(oruValue: string): bigint {
  return fixedDecimalToAtoms(oruValue, ORU_DISPLAY_DECIMALS);
}

export function resStringToAtoms(resValue: string): bigint {
  return fixedDecimalToAtoms(resValue, RES_DISPLAY_DECIMALS);
}

export function atomsToOruDisplay(atoms: string | bigint | number): string {
  return `${formatFixedAtoms(BigInt(String(atoms)), ORU_DISPLAY_DECIMALS)} ORU`;
}

export function atomsToResDisplay(atoms: string | bigint | number): string {
  return `${formatFixedAtoms(BigInt(String(atoms)), RES_DISPLAY_DECIMALS)} RES`;
}
