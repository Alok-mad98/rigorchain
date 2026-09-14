import { useSystemState } from "../context/SystemStateContext";
import { readRfc020Live } from "../lib/rfc020Status";
import {
  CUMULATIVE_ISSUANCE_CAPTION,
  UNIT_PRICE_CURVE_CAPTION,
  buildRfc020Curve,
} from "../lib/resIssuanceSchedule";

function polyline(values: number[], width: number, height: number, pad: number): string {
  if (values.length === 0) return "";
  const min = Math.min(...values);
  const max = Math.max(...values);
  const dy = max - min || 1;
  return values
    .map((v, i) => {
      const x = pad + (i / (values.length - 1)) * (width - pad * 2);
      const y = pad + (1 - (v - min) / dy) * (height - pad * 2);
      return `${x.toFixed(2)},${y.toFixed(2)}`;
    })
    .join(" ");
}

function Curve({ values, label }: { values: number[]; label: string }) {
  const width = 720;
  const height = 220;
  return (
    <svg className="res-curve" viewBox={`0 0 ${width} ${height}`} role="img" aria-label={label}>
      <polyline fill="none" stroke="currentColor" strokeWidth="1.5" points={polyline(values, width, height, 16)} />
    </svg>
  );
}

export function ResIssuanceCharts() {
  const { state } = useSystemState();
  const live = readRfc020Live(state.convergence_status);
  const rows = buildRfc020Curve();
  const prices = rows.map((r) => Number(r.destroyCostAtoms));
  const cumulative = rows.map((r) => Number(r.cumulativeResAtoms));

  return (
    <section className="card prose-section">
      <h2>ORU destroyed to mint 1 RES</h2>
      <p className="muted">{UNIT_PRICE_CURVE_CAPTION}</p>
      {live.confirmHeight != null && (
        <p>
          Live confirming height h = {live.confirmHeight}: current destruction cost = {live.destroyCostOru} ORU per
          1 RES. Next height ({live.nextHeight ?? "—"}) is {live.destroyCostNextOru} ORU.
        </p>
      )}
      <Curve values={prices} label={UNIT_PRICE_CURVE_CAPTION} />
      <p className="muted">
        Sampled on the RFC-020 year curve. Consensus prices every integer height.
        Vertical axis: destroy_cost(h) ORU atoms per 1.00000000 RES.
      </p>

      <h2>Derived issuance ceiling</h2>
      <p className="muted">{CUMULATIVE_ISSUANCE_CAPTION}</p>
      <Curve values={cumulative} label={CUMULATIVE_ISSUANCE_CAPTION} />
      <p className="muted">
        Same sampled heights. Vertical axis: cumulative RES atoms at the derived ceiling.
      </p>
    </section>
  );
}
