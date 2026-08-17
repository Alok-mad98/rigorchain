import { Link } from "react-router-dom";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { useSystemState } from "../context/SystemStateContext";

export function LandingPage() {
  const { state } = useSystemState();

  return (
    <div className="page landing-page">
      <section className="hero">
        <p className="tag">WWW.RIGORCHAIN.COM</p>
        <h1>AlphaPlus</h1>
        <p className="hero-sub">
          Deterministic proof-of-work protocol — native assets <strong>ORU</strong> and{" "}
          <strong>RES</strong>
        </p>
        <p className="hero-sub muted">
          You do not need to trust a node. You can independently verify the result yourself.
        </p>
      </section>

      <section className="card prose-section">
        <h2>What is AlphaPlus?</h2>
        <p>
          AlphaPlus is the official public name for this RIGOR chain. There is <strong>no premine</strong>.
          Miners secure the ledger with PoW. ORU pays for execution. RES is issued only through
          Convergence. The website is a read-only observer: tools, documentation, and an independent
          verifier — not a marketing funnel.
        </p>
      </section>

      <section className="grid-3 identity-grid">
        <article className="card">
          <h3>No premine</h3>
          <p>Issuance follows frozen subsidy and mint rules. This site does not allocate coins.</p>
        </article>
        <article className="card">
          <h3>PoW</h3>
          <p>Mining finds valid headers and orders blocks. Pool policy cannot mint RES.</p>
        </article>
        <article className="card">
          <h3>ORU</h3>
          <p>Execution and fee resource. Terminal subsidy floor 125 = 5³.</p>
        </article>
        <article className="card">
          <h3>RES</h3>
          <p>Scarce settlement asset. Distinct from ORU. Conservation is per transition.</p>
        </article>
        <article className="card">
          <h3>Convergence</h3>
          <p>Deterministic winner selection over committed set F — not a lottery UI.</p>
        </article>
        <article className="card">
          <h3>Zero Trust</h3>
          <p>
            <Link to="/zero-trust-verification">Verify TXID or block hash</Link> in this browser.
          </p>
        </article>
      </section>

      <section className="card prose-section">
        <h2>Start in 30 seconds</h2>
        <ol className="journey-list">
          <li>
            <Link to="/about">Understand AlphaPlus</Link>
          </li>
          <li>
            <Link to="/download">Download wallet</Link> then <Link to="/download/miner">miner</Link>
          </li>
          <li>
            <Link to="/mining/windows">Start mining</Link> and observe ORU in the{" "}
            <Link to="/wallet">wallet</Link>
          </li>
          <li>
            Read <Link to="/wallet/convergence">Convergence</Link>, then{" "}
            <Link to="/zero-trust-verification">verify independently</Link>
          </li>
        </ol>
      </section>

      <section className="card prose-section">
        <h2>Official public surfaces</h2>
        <p>
          Download · Wallet · Mining · Explorer · Documentation ·{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification ⭐</Link>
        </p>
        <p className="muted">
          Explorer is a viewer. The verifier is an independent validation layer: the browser
          recomputes AlphaPlus consensus outputs without trusting nodes, wallets, or APIs.
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
        <Link to="/zero-trust-verification" className="btn-link">
          Zero-Trust Verification ⭐
        </Link>
        <Link to="/download" className="btn-link">
          Download
        </Link>
        <Link to="/explorer" className="btn-link">
          Explorer
        </Link>
        <Link to="/documentation" className="btn-link">
          Documentation
        </Link>
        <Link to="/status" className="btn-link">
          PROOF-034 status
        </Link>
      </section>

      <section className="pipeline-diagram mono">
        DurinHash → Block → FORGE → RES mint → Archive (PROOF-034, provisional)
      </section>
    </div>
  );
}
