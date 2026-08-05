import { Link } from "react-router-dom";
import { ThreeLayerModel } from "../components/ThreeLayerModel";

export function ExperimentPage() {
  return (
    <div className="page experiment-page">
      <header className="page-header">
        <h1>PROOF-034 experiment</h1>
        <p className="muted">
          Runtime archive verification at milestone height 2,100,001 — observational layer only
        </p>
      </header>

      <section className="card prose-section">
        <h2>What PROOF-034 is</h2>
        <p>
          PROOF-034 is the live formal verification campaign that materializes a runtime archive from
          chain execution through height <span className="mono">≥ 2,100,001</span>. It produces empirical
          traces at catalogued golden-vector heights — it does <em>not</em> by itself establish protocol
          equivalence or mainnet readiness.
        </p>
      </section>

      <section className="card prose-section">
        <h2>What PROOF-034 is not</h2>
        <ul>
          <li>Not a claim that the system is complete or production-final</li>
          <li>Not equivalence_claimed = true (global invariant remains false)</li>
          <li>Not a substitute for the frozen protocol specification (Layer 1)</li>
          <li>Not controllable from this website — mining and validation continue independently</li>
        </ul>
      </section>

      <section className="card prose-section">
        <h2>Archive vs formal proof</h2>
        <table className="data-table">
          <thead>
            <tr>
              <th>Artifact</th>
              <th>Role</th>
              <th>Limit</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>Formal proof (Coq / axioms)</td>
              <td>Logical consequences of frozen rules</td>
              <td>Does not prove live chain reached milestone heights</td>
            </tr>
            <tr>
              <td>Runtime archive (PROOF-034)</td>
              <td>Empirical execution trace at GV heights</td>
              <td>Provisional until harness PASS and manifest published</td>
            </tr>
            <tr>
              <td>This website</td>
              <td>Read-only narrative and status display</td>
              <td>No consensus authority; no execution control</td>
            </tr>
          </tbody>
        </table>
      </section>

      <section className="card prose-section">
        <h2>Key milestone heights</h2>
        <table className="data-table">
          <thead>
            <tr>
              <th>Height</th>
              <th>Golden vector</th>
              <th>Phenomenon</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td className="mono">630,000</td>
              <td className="mono">GV-ORU-0003</td>
              <td>ORU halving / floor 125</td>
            </tr>
            <tr>
              <td className="mono">2,100,000</td>
              <td className="mono">GV-RES-0003</td>
              <td>RES cap boundary</td>
            </tr>
            <tr>
              <td className="mono">≥ 2,100,001</td>
              <td className="mono">GV-DRAW-0003</td>
              <td>DRAW reject fixture — PROOF-034 terminal target</td>
            </tr>
          </tbody>
        </table>
      </section>

      <h2 className="section-title">Layer context</h2>
      <ThreeLayerModel />

      <p className="muted">
        Live progress: <Link to="/status">verification status</Link>
      </p>
    </div>
  );
}
