/**
 * RFC-020 continuous destroy cost. Integer-only, matches
 * tools/res_continuous_price_schedule.py.
 * destroy_cost(h) is a protocol mint cost, not a market quote.
 */

export const ORU_COIN = 100_000_000n;
export const RES_COIN = 100_000_000n;
export const RES_DESTROY_COST_GENESIS = 1024n * ORU_COIN;
export const RES_DESTROY_POLE = 1n << 25n;
export const RES_DESTROY_SHIFT = 40n;
export const ORU_FLAT_SUBSIDY = 128n;
export const RES_MAX_SUPPLY = 419_430_400_000_000n;
export const RES_DERIVED_MAX_SUPPLY = RES_MAX_SUPPLY;
export const TARGET_BLOCK_TIME_SECONDS = 600;
export const BLOCKS_PER_YEAR = (365.2425 * 24 * 3600) / TARGET_BLOCK_TIME_SECONDS;
export const RFC020_CHART_END_YEAR = 2000;
export const RFC020_CHART_STEP_YEARS = 4;
export const RFC020_POLE_YEAR = 638;
export const RFC020_MILESTONE_YEARS = [0, 1, 4, 10, 16, 32, 50, 100, 250, 500, 638, 1000, 2000] as const;

export const UNIT_PRICE_CURVE_CAPTION =
  "Convergence destroys ORU to mint RES. Display destroy_cost(h) is ORU destroyed to mint 1 RES at confirming height h — not a market quote and not the destroyed amount B.";

export const CUMULATIVE_ISSUANCE_CAPTION =
  "Closed-form ceiling cap · h / (h + 2^25) if every block’s ORU subsidy were destroyed. Actual minted RES equals participation times this ceiling.";

export function yearToHeight(year: number): number {
  if (!Number.isFinite(year) || year <= 0) return 0;
  return Math.trunc(year * BLOCKS_PER_YEAR);
}

export function resDestroyCostForHeight(height: number): bigint {
  if (!Number.isFinite(height) || height < 0) return 0n;
  const h = BigInt(Math.trunc(height));
  const poleH = RES_DESTROY_POLE + h;
  return (RES_DESTROY_COST_GENESIS * poleH * poleH) >> 50n;
}

export function cumulativeResAtHeight(height: number): bigint {
  if (!Number.isFinite(height) || height <= 0) return 0n;
  const h = BigInt(Math.trunc(height));
  return (RES_MAX_SUPPLY * h) / (h + RES_DESTROY_POLE);
}

export type ResCurvePoint = {
  year: number;
  height: number;
  destroyCostAtoms: bigint;
  cumulativeResAtoms: bigint;
};

export function curvePointAtYear(year: number): ResCurvePoint {
  const height = yearToHeight(year);
  return {
    year,
    height,
    destroyCostAtoms: resDestroyCostForHeight(height),
    cumulativeResAtoms: cumulativeResAtHeight(height),
  };
}

export function buildRfc020Curve(stepYears = RFC020_CHART_STEP_YEARS): ResCurvePoint[] {
  const years = new Set<number>(RFC020_MILESTONE_YEARS);
  for (let y = 0; y <= RFC020_CHART_END_YEAR; y += stepYears) years.add(y);
  return [...years].sort((a, b) => a - b).map((year) => curvePointAtYear(year));
}

export function nextHeightDestroyCostSentence(height: number, destroyCostAtoms: bigint): string {
  const whole = destroyCostAtoms / ORU_COIN;
  const frac = destroyCostAtoms % ORU_COIN;
  const oru =
    frac === 0n
      ? whole.toString()
      : `${whole}.${frac.toString().padStart(8, "0").replace(/0+$/, "")}`;
  return `From block ${height} minting 1 RES destroys ${oru} ORU.`;
}
