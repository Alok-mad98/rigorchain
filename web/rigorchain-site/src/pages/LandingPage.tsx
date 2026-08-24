import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { useSystemState } from "../context/SystemStateContext";

export function LandingPage() {
  const { state } = useSystemState();

  return (
    <div className="page landing-page">
      <section className="hero">
        <img src="/assets/rigor-mark.png" alt="" className="hero-mark" width={56} height={56} />
        <p className="tag">WWW.RIGORCHAIN.COM</p>
        <h1>RIGOR</h1>
        <p className="hero-sub">
          A dual-asset UTXO protocol built on script-derived asset classification,
          stateless monetary architecture, and formally verified consensus invariants.
        </p>
      </section>

      <section className="card prose-section">
        <h2>What is RIGOR?</h2>
        <p>
          RIGOR is a dual-asset UTXO protocol built on script-derived asset
          classification, stateless monetary architecture, and formally verified
          consensus invariants. ORU provides PoW security and fees. RES is
          deterministically issued after protocol-defined Convergence conditions —
          not mined. Script is the compliance/classification layer. There is no token
          registry, account ledger, or global asset state. Invariants are properties
          of the protocol model; the C++ node is not claimed equivalent
          (<code>equivalence_claimed = false</code>).
        </p>
        <pre className="pipeline-diagram">{`RIGOR =
    Dual Native Assets
  + UTXO Monetary Model
  + Deterministic Issuance
  + Deflationary Mechanisms
  + Probabilistic Selection
  + Adaptive Proof-of-Work
  + Cryptographic Ownership

equivalence_claimed = false`}</pre>
      </section>

      <SectionTitle>Core identity</SectionTitle>
      <section className="identity-grid">
        <DarkCard title="PoW" to="/mining" linkLabel="Mining">
          <p>Miners find valid headers and order blocks. Pool policy cannot mint RES.</p>
        </DarkCard>
        <DarkCard title="ORU" to="/wallet/oru" linkLabel="Wallet · ORU">
          <p>Execution and fee resource. Terminal subsidy floor 125 = 5³.</p>
        </DarkCard>
        <DarkCard title="RES" to="/wallet/res" linkLabel="Wallet · RES">
          <p>Reserve asset. Deterministic issuance after Convergence — not mined. Capped supply.</p>
        </DarkCard>
        <DarkCard title="Convergence" to="/wallet/convergence" linkLabel="Wallet · Convergence">
          <p>Deterministic winner selection over committed set F — not a lottery UI.</p>
        </DarkCard>
      </section>
      <section className="identity-spotlight" aria-label="Zero Trust">
        <DarkCard title="Zero Trust" to="/zero-trust-verification" linkLabel="Verify">
          <p>Anyone can verify a TXID or block hash without trusting a server.</p>
        </DarkCard>
      </section>

      <section className="cta-row">
        <Button to="/download">Download</Button>
        <Button to="/zero-trust-verification">Verify independently</Button>
        <Button to="/documentation">Documentation</Button>
        <Button to="/documentation/formal-verification">Formal verification</Button>
      </section>

      <SectionTitle>Start in 30 seconds</SectionTitle>
      <section className="card prose-section">
        <ol className="journey-list">
          <li>
            <Link to="/about">Understand RIGOR</Link>
          </li>
          <li>
            <Link to="/download">Download wallet</Link> then <Link to="/download/miner">miner</Link>
          </li>
          <li>
            <Link to="/mining">Start mining</Link> and observe ORU in the{" "}
            <Link to="/wallet">wallet</Link>
          </li>
          <li>
            Read <Link to="/wallet/convergence">Convergence</Link>, then{" "}
            <Link to="/zero-trust-verification">verify independently</Link>
          </li>
        </ol>
      </section>

      <section className="card prose-section">
        <h2>Experimental system overview</h2>
        <p>
          Formal verification covers protocol-model invariants and a correspondence
          boundary (golden vectors, differential gate). It is{" "}
          <em>not</em> presented as a finished cryptocurrency, a refinement proof, or production
          mainnet. Protocol rules are frozen; <span className="mono">equivalence_claimed = false</span>.
        </p>
      </section>

      <SectionTitle>Three-layer model</SectionTitle>
      <ThreeLayerModel />

      <section className="grid-3">
        <article className="card">
          <h3>Shipping draw economics</h3>
          <p className="math-block">
            E = 246 · C = 1024 ORU / mint
          </p>
          <p className="muted">
            Expected wins per committed ORU ≈ n / 1024. Historical RFC-014 parameter: 1/65536
            (C = 65536 ORU) is not the shipping target.
          </p>
        </article>
        <article className="card">
          <h3>125 convergence constant</h3>
          <p className="math-block">1000 → 500 → 250 → 125 = 5³</p>
          <p className="muted">Terminal ORU subsidy floor — frozen protocol constant.</p>
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

      <section className="pipeline-diagram mono">
        DurinHash → Block → FORGE → RES mint → Archive (PROOF-034, provisional)
      </section>
    </div>
  );
}
