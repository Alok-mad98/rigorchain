/**
 * SVG polylines for the RFC-020 destroy-to-mint curve and closed-form ceiling.
 * Presentation only. Years match tools/res_continuous_price_schedule.py.
 */

import {
  CUMULATIVE_ISSUANCE_CAPTION,
  RFC020_CHART_END_YEAR,
  RFC020_MILESTONE_YEARS,
  RFC020_POLE_YEAR,
  RES_DESTROY_AXIS_CAPTION,
  UNIT_PRICE_CURVE_CAPTION,
  type ResCurvePoint,
} from "./resIssuanceSchedule";

export const RES_CHART_AXIS_CAPTION = RES_DESTROY_AXIS_CAPTION;

const WIDTH = 720;
const HEIGHT = 248;
const PAD_X = 16;
const PAD_T = 18;
const PAD_B = 28;

function xOfYear(year: number, endYear: number, innerW: number): number {
  return PAD_X + (year / endYear) * innerW;
}

function yearGrid(endYear: number, innerW: number, innerH: number): string {
  const parts: string[] = [];
  for (const year of RFC020_MILESTONE_YEARS) {
    const x = xOfYear(year, endYear, innerW).toFixed(2);
    const major = year === 0 || year === RFC020_POLE_YEAR || year === endYear || year === 1000;
    parts.push(
      `<line data-year="${year}" x1="${x}" y1="${PAD_T}" x2="${x}" y2="${PAD_T + innerH}" stroke="${major ? "#666" : "#333"}" stroke-width="${major ? "1" : "0.4"}"/>`,
    );
  }
  const ticks = [0, 250, 500, 638, 1000, 2000];
  for (const year of ticks) {
    const x = xOfYear(year, endYear, innerW).toFixed(2);
    parts.push(
      `<text x="${x}" y="${HEIGHT - 8}" fill="#bbb" font-size="10" text-anchor="middle">${year}</text>`,
    );
  }
  return parts.join("");
}

function yearMark(
  year: number | null,
  endYear: number,
  innerW: number,
  innerH: number,
  label: string,
): string {
  if (year == null || !Number.isFinite(year)) return "";
  const y = Math.max(0, Math.min(endYear, year));
  const x = xOfYear(y, endYear, innerW);
  const labelX = Math.min(x + 4, WIDTH - PAD_X - 72);
  return (
    `<line data-mark="${label}" x1="${x.toFixed(2)}" y1="${PAD_T}" x2="${x.toFixed(2)}" y2="${PAD_T + innerH}" stroke="#ffffff" stroke-width="2"/>` +
    `<text x="${labelX.toFixed(2)}" y="${PAD_T + 12}" fill="#ffffff" font-size="11">${label}</text>`
  );
}

export function renderYearSeriesSvg(args: {
  points: ResCurvePoint[];
  valueOf: (p: ResCurvePoint) => number;
  label: string;
  currentYear: number | null;
  width?: number;
  height?: number;
}): string {
  const points = args.points;
  if (points.length < 2) return "";
  const width = args.width ?? WIDTH;
  const height = args.height ?? HEIGHT;
  const endYear = RFC020_CHART_END_YEAR;
  const innerW = width - PAD_X * 2;
  const innerH = height - PAD_T - PAD_B;
  const values = points.map(args.valueOf);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const dy = max - min || 1;
  const polyline = points
    .map((p) => {
      const x = xOfYear(p.year, endYear, innerW);
      const y = PAD_T + (1 - (args.valueOf(p) - min) / dy) * innerH;
      return `${x.toFixed(2)},${y.toFixed(2)}`;
    })
    .join(" ");
  return `<svg class="res-curve" viewBox="0 0 ${width} ${height}" role="img" aria-label="${args.label}">
    ${yearGrid(endYear, innerW, innerH)}
    ${yearMark(RFC020_POLE_YEAR, endYear, innerW, innerH, "pole 638")}
    ${yearMark(args.currentYear, endYear, innerW, innerH, "now")}
    <polyline fill="none" stroke="#ffffff" stroke-width="1.5" points="${polyline}" />
  </svg>`;
}

export function renderUnitPriceChartSvg(
  points: ResCurvePoint[],
  currentYear: number | null = null,
  width = WIDTH,
  height = HEIGHT,
): string {
  return renderYearSeriesSvg({
    points,
    valueOf: (p) => Number(p.destroy_oru_atoms),
    label: UNIT_PRICE_CURVE_CAPTION,
    currentYear,
    width,
    height,
  });
}

export function renderCumulativeIssuanceChartSvg(
  points: ResCurvePoint[],
  currentYear: number | null = null,
  width = WIDTH,
  height = HEIGHT,
): string {
  return renderYearSeriesSvg({
    points,
    valueOf: (p) => Number(p.cumulative_res_atoms),
    label: CUMULATIVE_ISSUANCE_CAPTION,
    currentYear,
    width,
    height,
  });
}

export function renderResIssuanceChartsHtml(points: ResCurvePoint[], currentYear: number | null = null): string {
  return `<div class="status-grid status-grid--charts">
    <section class="status-card">
      <h2>ORU destroyed to mint 1 RES</h2>
      <p class="muted">${UNIT_PRICE_CURVE_CAPTION}</p>
      ${renderUnitPriceChartSvg(points, currentYear)}
      <p class="muted">${RES_CHART_AXIS_CAPTION} Vertical axis: ORU atoms destroyed to mint 1.00000000 RES.</p>
    </section>
    <section class="status-card">
      <h2>RES mint ceiling</h2>
      <p class="muted">${CUMULATIVE_ISSUANCE_CAPTION}</p>
      ${renderCumulativeIssuanceChartSvg(points, currentYear)}
      <p class="muted">${RES_CHART_AXIS_CAPTION} Vertical axis: cumulative RES atoms at the closed-form ceiling.</p>
    </section>
  </div>`;
}
