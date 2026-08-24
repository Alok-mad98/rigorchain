import { Link } from "react-router-dom";
import { ThreeLayerModel } from "../components/ThreeLayerModel";

export function SciencePage() {
  return (
    <div className="page science-page">
      <header className="page-header">
        <h1>Scientific model</h1>
        <p className="muted">
          Deterministic state machine formalization — mathematical layer, independent of live execution
        </p>
      </header>

      <section className="card prose-section">
        <h2>System as state machine</h2>
        <p className="math-block">
          S<sub>t+1</sub> = F(S<sub>t</sub>, T<sub>t</sub>)
        </p>
        <p>
          RIGOR is modeled as a deterministic financial state machine. Given frozen protocol rules F and
          an accepted transaction batch T<sub>t</sub>, the next state S<sub>t+1</sub> is uniquely determined.
          This layer establishes what <em>must</em> hold if the rules are applied — not what has been
          empirically observed on a live chain.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Dual-asset conservation</h2>
        <p className="math-block">Σ ORU<sub>in</sub> = Σ ORU<sub>out</sub> + fee</p>
        <p className="math-block">Σ RES<sub>in</sub> = Σ RES<sub>out</sub></p>
        <p>
          ORU and RES are distinct asset classes inferred from scriptPubKey paths. Conservation laws apply
          per transition; there is no global account balance table as a source of truth.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Probabilistic fairness (hash space)</h2>
        <p className="math-block">E = 246 · C = 1024 ORU per mint</p>
        <p>
          Convergence eligibility is a deterministic integer inequality over σ digests. Shipping
          expected wins per committed ORU ≈ n / 1024 (Deterministic Reserve Issuance). Historical
          RFC-014 / retired design:{" "}
          <code>1/65536</code> (<code>C = 65536</code> ORU) is a teaching slice, not the live
          aggregate identity.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Convergence stability constant</h2>
        <p className="math-block">1000 → 500 → 250 → 125 = 5³</p>
        <p>Terminal ORU subsidy floor at 125 ORU per block — no further halving beyond this constant.</p>
      </section>

      <h2 className="section-title">Three-layer separation</h2>
      <ThreeLayerModel />

      <p className="muted">
        Scientific correctness does not imply runtime equivalence. See{" "}
        <Link to="/documentation/formal-verification">formal verification freeze</Link>,{" "}
        <Link to="/status">verification status</Link>, and{" "}
        <Link to="/experiment">PROOF-034 experiment</Link> for the correspondence and archive
        layers.
      </p>
    </div>
  );
}
