/**
 * RFC-020 continuous destroy cost. Integer-only, bit-identical to
 * tools/res_continuous_price_schedule.py and consensus/res_issuance.h.
 *
 * Consensus evaluates R · (2^25 + h)^2 ≤ B << 40. The display destroy cost is
 * not a consensus input.
 */

import { ORU_COIN, RES_COIN, atomsToOruDisplay, atomsToResDisplay } from "./amount";

export const RES_DESTROY_COST_GENESIS = 1024n * ORU_COIN;
export const RES_DESTROY_POLE = 1n << 25n;
export const RES_DESTROY_SHIFT = 40n;
export const ORU_FLAT_SUBSIDY = 128n;
export const RES_MAX_SUPPLY = 419430400000000n;
export const RES_DERIVED_MAX_SUPPLY = RES_MAX_SUPPLY;
/** Same as tools/res_continuous_price_schedule.py (600-second blocks). */
export const TARGET_BLOCK_TIME_SECONDS = 600;
export const BLOCKS_PER_YEAR = (365.2425 * 24 * 3600) / TARGET_BLOCK_TIME_SECONDS;
export const RFC020_CHART_END_YEAR = 2000;
export const RFC020_CHART_STEP_YEARS = 4;
export const RFC020_POLE_YEAR = 638;
/** RFC-020 §4.1 table, plus year 0. */
export const RFC020_MILESTONE_YEARS = [0, 1, 4, 10, 16, 32, 50, 100, 250, 500, 638, 1000, 2000] as const;

/** Startup / DB pin. Change this when cap, genesis destroy cost, or the pole formula changes. */
export const ISSUANCE_GENERATION =
  `rfc020:cap=${RES_MAX_SUPPLY}:p0=${RES_DESTROY_COST_GENESIS}:pole=${RES_DESTROY_POLE}:shift=${RES_DESTROY_SHIFT}`;


export type ResCurvePoint = {
  year: number;
  height: number;
  destroy_oru_atoms: string;
  destroy_cost_atoms: string;
  cumulative_res_atoms: string;
};

export function yearToHeight(year: number): number {
  if (!Number.isFinite(year) || year <= 0) return 0;
  return Math.trunc(year * BLOCKS_PER_YEAR);
}

export function heightToYear(height: number): number {
  if (!Number.isFinite(height) || height <= 0) return 0;
  return height / BLOCKS_PER_YEAR;
}

/** Next confirming height after the current tip. Next mint is priced at this h. */
export function confirmHeightForTip(tipHeight: number): number {
  if (!Number.isFinite(tipHeight) || tipHeight < 0) return 0;
  return Math.trunc(tipHeight) + 1;
}

/** Display destroy cost at height. Consensus never evaluates this function. */
export function resDestroyCostForHeight(height: number): bigint {
  if (!Number.isFinite(height) || height < 0) return 0n;
  const h = BigInt(Math.trunc(height));
  const poleH = RES_DESTROY_POLE + h;
  return (RES_DESTROY_COST_GENESIS * poleH * poleH) >> 50n;
}

export function formatRateOruPerRes(atoms: bigint): string {
  if (atoms <= 0n) return "—";
  return `${atomsToOruDisplay(atoms)} destroyed to mint 1 RES`;
}

export function formatDerivedMaxSupply(): string {
  return atomsToResDisplay(RES_MAX_SUPPLY);
}

export function cumulativeResAtHeight(height: number): bigint {
  if (!Number.isFinite(height) || height <= 0) return 0n;
  const h = BigInt(Math.trunc(height));
  return (RES_MAX_SUPPLY * h) / (h + RES_DESTROY_POLE);
}


export function curvePointAtYear(year: number): ResCurvePoint {
  const height = yearToHeight(year);
  const destroy = resDestroyCostForHeight(height).toString();
  return {
    year,
    height,
    destroy_oru_atoms: destroy,
    destroy_cost_atoms: destroy,
    cumulative_res_atoms: cumulativeResAtHeight(height).toString(),
  };
}

export function buildRfc020MilestoneRows(): ResCurvePoint[] {
  return RFC020_MILESTONE_YEARS.map((year) => curvePointAtYear(year));
}

/** Dense samples vs year for the RFC-020 quadratic / closed-form ceiling. */
export function buildRfc020Curve(stepYears = RFC020_CHART_STEP_YEARS): ResCurvePoint[] {
  const years = new Set<number>(RFC020_MILESTONE_YEARS);
  for (let y = 0; y <= RFC020_CHART_END_YEAR; y += stepYears) years.add(y);
  return [...years].sort((a, b) => a - b).map((year) => curvePointAtYear(year));
}

export const UNIT_PRICE_CURVE_CAPTION =
  "Convergence destroys ORU to mint RES. Display destroy_cost(h) is ORU destroyed to mint 1 RES at the confirming height. Not a market value.";

export const CUMULATIVE_ISSUANCE_CAPTION =
  "Ceiling 2²² · h / (h + 2²⁵) RES if every block’s ORU subsidy is destroyed. Actual minted RES equals participation times this ceiling.";

export const RES_DESTROY_AXIS_CAPTION =
  "Horizontal axis: years at 600-second blocks. Pole at year 638 (h = 2²⁵), where the ceiling reaches half of 4,194,304 RES.";

export const NEXT_HEIGHT_COST_TEMPLATE =
  "From block {height}, minting 1 RES destroys {oru} ORU.";

export function nextHeightDestroyCostSentence(height: number, destroyCostAtoms: bigint): string {
  const whole = destroyCostAtoms / ORU_COIN;
  const frac = destroyCostAtoms % ORU_COIN;
  const oru =
    frac === 0n
      ? whole.toString()
      : `${whole}.${frac.toString().padStart(8, "0").replace(/0+$/, "")}`;
  return NEXT_HEIGHT_COST_TEMPLATE.replace("{height}", String(height)).replace("{oru}", oru);
}

export function resIssuanceSchedulePayload(): {
  unit_price_caption: string;
  destroy_caption: string;
  cumulative_issuance_caption: string;
  axis_caption: string;
  derived_max_supply_atoms: string;
  pole_year: number;
  chart_end_year: number;
  blocks_per_year: number;
  curve: ResCurvePoint[];
  milestones: ResCurvePoint[];
} {
  return {
    unit_price_caption: UNIT_PRICE_CURVE_CAPTION,
    destroy_caption: UNIT_PRICE_CURVE_CAPTION,
    cumulative_issuance_caption: CUMULATIVE_ISSUANCE_CAPTION,
    axis_caption: RES_DESTROY_AXIS_CAPTION,
    derived_max_supply_atoms: RES_MAX_SUPPLY.toString(),
    pole_year: RFC020_POLE_YEAR,
    chart_end_year: RFC020_CHART_END_YEAR,
    blocks_per_year: BLOCKS_PER_YEAR,
    curve: buildRfc020Curve(),
    milestones: buildRfc020MilestoneRows(),
  };
}
