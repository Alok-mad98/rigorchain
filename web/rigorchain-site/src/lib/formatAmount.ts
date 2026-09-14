/** Format protocol atoms (1e-8) as a decimal string. */
export function formatAtoms(atoms: number | bigint | string | null | undefined): string {
  if (atoms === null || atoms === undefined || atoms === "") return "—";
  try {
    const n = typeof atoms === "bigint" ? atoms : BigInt(Math.trunc(Number(atoms)));
    if (n < 0n) return "—";
    const whole = n / 100_000_000n;
    const frac = n % 100_000_000n;
    if (frac === 0n) return whole.toString();
    return `${whole}.${frac.toString().padStart(8, "0").replace(/0+$/, "")}`;
  } catch {
    return "—";
  }
}

export function pickNumber(obj: Record<string, unknown> | null, ...keys: string[]): number | null {
  if (!obj) return null;
  for (const k of keys) {
    const v = obj[k];
    if (typeof v === "number" && Number.isFinite(v)) return v;
    if (typeof v === "string" && v !== "" && Number.isFinite(Number(v))) return Number(v);
  }
  return null;
}

export function pickString(obj: Record<string, unknown> | null, ...keys: string[]): string | null {
  if (!obj) return null;
  for (const k of keys) {
    const v = obj[k];
    if (v !== undefined && v !== null && v !== "") return String(v);
  }
  return null;
}
