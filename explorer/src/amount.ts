const POW10 = (n: number): bigint => {
  let r = 1n;
  for (let i = 0; i < n; i++) r *= 10n;
  return r;
};

/**
 * Parse a fixed-decimal string into integer atoms deterministically.
 *
 * Expected shapes from RPC:
 * - ORU: 8 dp max (e.g. "1.00000000")
 * - RES: 2 dp max (e.g. "1.00")
 */
export function fixedDecimalToAtoms(value: string, decimals: number): bigint {
  const s = value.trim();
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
    // Deterministic rejection: RPC should not emit extra dp beyond policy.
    throw new Error(`too many decimals for ${value}, max=${decimals}`);
  }

  const fracPadded = fracPartRaw.padEnd(decimals, "0");

  const intAtoms = BigInt(intPart) * POW10(decimals);
  const fracAtoms = BigInt(fracPadded);
  const atoms = intAtoms + fracAtoms;

  return negative ? -atoms : atoms;
}

export function oruStringToAtoms(oruValue: string): bigint {
  return fixedDecimalToAtoms(oruValue, 8);
}

export function resStringToAtoms(resValue: string): bigint {
  return fixedDecimalToAtoms(resValue, 2);
}

