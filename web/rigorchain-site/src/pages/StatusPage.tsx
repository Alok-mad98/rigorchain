import { Link } from "react-router-dom";
import { useSystemState } from "../context/SystemStateContext";
import {
  EQUIVALENCE_CLAIMED,
  FREEZE_STATUS,
  PROOF034_STATUS,
  PROOF034_TARGET_HEIGHT,
  RELEASE_STAGE,
  RIGOR_VERSION,
} from "../version";

const PROOF034_TARGET = PROOF034_TARGET_HEIGHT;

const MILESTONES = [
  { id: "M0", label: "Procedure established", status: "COMPLETE" },
  { id: "M2", label: "h = 630000 (GV-ORU-0003)", status: "PENDING" },
  { id: "M3", label: "h = 2100000 (GV-RES-0003)", status: "PENDING" },
  { id: "M4", label: "h ≥ 2100001 (GV-DRAW-0003)", status: "PENDING" },
  { id: "M6", label: "Harness PASS", status: "PENDING" },
  { id: "M7", label: "Manifest published", status: "PENDING" },
];

export function StatusPage() {
  const { state } = useSystemState();
  const v = state.verification_state;
  const height = state.chain_height;
  const progressPct =
    height != null && height > 0
      ? Math.min(100, Math.round((height / PROOF034_TARGET) * 1000) / 10)
      : null;

  return (
    <div className="page status-page">
      <header className="page-header">
        <h1>Verification status</h1>
        <p className="muted">Read-only display — does not control or interrupt PROOF-034 execution</p>
      </header>

      <div className="status-hero card">
        <span className="badge-running">Verification Running</span>
        <h2>PROOF-034 — runtime archive toward 2.1M</h2>
        <dl className="kv-grid status-kv">
          <dt>Release version</dt>
          <dd className="mono">{RIGOR_VERSION}</dd>
          <dt>Release stage</dt>
          <dd className="mono">{RELEASE_STAGE}</dd>
          <dt>Freeze status</dt>
          <dd className="mono">{FREEZE_STATUS}</dd>
          <dt>PROOF-034 gate</dt>
          <dd className="mono">{PROOF034_STATUS}</dd>
          <dt>Target height</dt>
          <dd className="mono">≥ {PROOF034_TARGET.toLocaleString()}</dd>
          <dt>Observed height (read-only)</dt>
          <dd className="mono">{height != null ? height.toLocaleString() : "—"}</dd>
          <dt>Phase</dt>
          <dd>{v.proof034_phase}</dd>
          <dt>Progress (height / target)</dt>
          <dd className="mono">{progressPct != null ? `${progressPct}%` : "—"}</dd>
          <dt>equivalence_claimed</dt>
          <dd className="mono equiv-invariant">{String(EQUIVALENCE_CLAIMED)}</dd>
        </dl>
      </div>

      <div className="banner banner-warning" role="alert">
        <strong>Active verification process.</strong> Results are provisional. Archive generation is in
        progress and has not reached final completion. No production mainnet finality is claimed.
      </div>

      <section className="card">
        <h2>Milestone tracker</h2>
        <table className="data-table">
          <thead>
            <tr>
              <th>Milestone</th>
              <th>Description</th>
              <th>Status</th>
            </tr>
          </thead>
          <tbody>
            {MILESTONES.map((m) => (
              <tr key={m.id}>
                <td className="mono">{m.id}</td>
                <td>{m.label}</td>
                <td>{m.status}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      <section className="card">
        <h2>Golden vectors</h2>
        <p>
          Materialized families: <span className="mono">{v.gv_materialized ?? "—"}</span> /{" "}
          <span className="mono">{v.gv_total}</span>
        </p>
        <p className="muted">
          Live counts require <code>VITE_VERIFICATION_MANIFEST_URL</code>. No fabricated progress.
        </p>
      </section>

      <section className="card">
        <h2>Observer connectivity</h2>
        <dl className="kv-grid">
          <dt>RPC</dt>
          <dd>{state.rpc_status.reachable ? `OK (${state.rpc_status.latency_ms}ms)` : "offline"}</dd>
          <dt>Explorer</dt>
          <dd>{state.explorer_status.reachable ? "OK" : "offline"}</dd>
          <dt>Global status</dt>
          <dd>{state.global_status}</dd>
        </dl>
      </section>

      <p className="muted">
        Full PROOF-034 procedure: <Link to="/experiment">experiment page</Link>
      </p>
    </div>
  );
}
