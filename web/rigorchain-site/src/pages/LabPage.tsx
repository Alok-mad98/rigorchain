import { Link } from "react-router-dom";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { useSystemState } from "../context/SystemStateContext";

const GV_FAMILIES = [
  { id: "GV-ORU-0003", height: "630000", label: "ORU halving / floor 125" },
  { id: "GV-RES-0003", height: "2100000", label: "RES cap boundary" },
  { id: "GV-DRAW-0003", height: "2100001", label: "DRAW reject fixture" },
];

export function LabPage() {
  const { state } = useSystemState();
  const v = state.verification_state;

  return (
    <div className="page lab-page">
      <header className="page-header">
        <h1>Lab narrative</h1>
        <p className="muted">
          Experimental framing — scientific interpretation, not protocol authority or finality claim
        </p>
      </header>

      <section className="card">
        <h2>Experimental framing</h2>
        <blockquote>
          RIGOR is a deterministic execution system under continuous formal verification — studied as a
          live laboratory, not marketed as a completed financial product.
        </blockquote>
        <p>
          The lab observes block acceptance, FORGE convergence, and RES mint emission as state transitions.
          PROOF-034 archive mining is actively running toward height 2,100,001; results remain provisional.
        </p>
      </section>

      <h2 className="section-title">Three-layer model</h2>
      <ThreeLayerModel />

      <section className="card">
        <h2>Axiom system overview</h2>
        <ul className="axiom-list">
          <li>Deterministic transition: S<sub>t+1</sub> = F(S<sub>t</sub>, T<sub>t</sub>)</li>
          <li>UTXO integrity — no hidden state mutation</li>
          <li>Dual conservation (ORU + RES)</li>
          <li>Probabilistic fairness: P = 1/65536 in hash space</li>
          <li>Convergence stability: terminal subsidy 125 = 5³</li>
          <li>equivalence_claimed = false (global invariant)</li>
        </ul>
      </section>

      <section className="card">
        <h2>Formal verification pipeline</h2>
        <div className="layer-diagram mono">
          Protocol ≠ Formal Proof ≠ Runtime Archive ≠ Visualization
        </div>
        <div className="tag-row">
          <span className="tag-chip">PROOF-034 (RUNNING)</span>
          <span className="tag-chip">RUNTIME-SOAK</span>
          <span className="tag-chip">FORMAL-PROOF</span>
          <span className="tag-chip">VISUALIZATION</span>
        </div>
      </section>

      <section className="card">
        <h2>Golden vector catalog (pending milestones)</h2>
        <p>
          Materialized: {v.gv_materialized ?? "—"} / {v.gv_total} — archive incomplete at 2.1M target
        </p>
        <table className="data-table">
          <thead>
            <tr>
              <th>Vector</th>
              <th>Height</th>
              <th>Phenomenon</th>
            </tr>
          </thead>
          <tbody>
            {GV_FAMILIES.map((g) => (
              <tr key={g.id}>
                <td className="mono">{g.id}</td>
                <td className="mono">{g.height}</td>
                <td>{g.label}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </section>

      <section className="card">
        <h2>PROOF-034 / P03 (observed)</h2>
        <dl className="kv-grid">
          <dt>PROOF-034</dt>
          <dd>{v.proof034_phase}</dd>
          <dt>P03 certification</dt>
          <dd>{v.p03_label}</dd>
          <dt>Live chain height</dt>
          <dd className="mono">{state.chain_height ?? "—"}</dd>
        </dl>
      </section>

      <p className="muted">
        <Link to="/status">Live status</Link> · <Link to="/experiment">PROOF-034 details</Link>
      </p>
    </div>
  );
}
