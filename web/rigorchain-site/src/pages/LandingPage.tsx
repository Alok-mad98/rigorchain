import { Link } from "react-router-dom";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { useSystemState } from "../context/SystemStateContext";

export function LandingPage() {
  const { state } = useSystemState();

  return (
    <div className="page landing-page">
      <section className="hero">
        <h1>RIGOR</h1>
        <p className="hero-sub">
          Experimental deterministic execution system — native assets <strong>ORU</strong> and{" "}
          <strong>RES</strong>
        </p>
        <p className="hero-sub muted">
          Under continuous formal verification (PROOF-034) · read-only public observer
        </p>
      </section>

      <section className="card prose-section">
        <h2>Experimental system overview</h2>
        <p>
          RIGOR is a deterministic dual-asset protocol under live formal verification. It is{" "}
          <em>not</em> presented as a finished cryptocurrency or production mainnet. Protocol rules are
          frozen; runtime archive generation toward height 2,100,001 is ongoing and provisional.
        </p>
      </section>

      <h2 className="section-title">Three-layer model</h2>
      <ThreeLayerModel />

      <section className="grid-3">
        <article className="card">
          <h3>65536 principle</h3>
          <p className="math-block">P(success) = 2<sup>−16</sup> = 1/65536</p>
          <p>Hash-space bounded fairness via deterministic σ digest inequality.</p>
        </article>
        <article className="card">
          <h3>125 convergence constant</h3>
          <p className="math-block">1000 → 500 → 250 → 125 = 5³</p>
          <p>Terminal ORU subsidy floor — frozen protocol constant.</p>
        </article>
        <article className="card">
          <h3>Live observation</h3>
          <dl className="kv-grid">
            <dt>Height</dt>
            <dd className="mono">{state.chain_height ?? "—"}</dd>
            <dt>Status</dt>
            <dd>{state.global_status}</dd>
            <dt>PROOF-034</dt>
            <dd>IN_PROGRESS</dd>
          </dl>
        </article>
      </section>

      <section className="cta-row">
        <Link to="/protocol" className="btn-link">
          Protocol (frozen)
        </Link>
        <Link to="/science" className="btn-link">
          Scientific model
        </Link>
        <Link to="/lab" className="btn-link">
          Lab narrative
        </Link>
        <Link to="/status" className="btn-link">
          Verification status
        </Link>
        <Link to="/experiment" className="btn-link">
          PROOF-034
        </Link>
      </section>

      <section className="pipeline-diagram mono">
        DurinHash → Block → FORGE → RES mint → Archive (PROOF-034, provisional)
      </section>
    </div>
  );
}
