import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function DocumentationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Documentation</h1>
        <p className="muted">Architecture, RFCs, security boundaries, and developer surfaces.</p>
      </header>

      <SectionTitle>Library</SectionTitle>
      <section className="card-grid doc-library-grid">
        <DarkCard title="Whitepaper" to="/documentation/whitepaper" linkLabel="Read">
          <p>Official protocol explanation. equivalence_claimed = false.</p>
        </DarkCard>
        <DarkCard
          title="Deterministic Reserve Issuance"
          to="/documentation/monetary-policy"
          linkLabel="Open"
        >
          <p>
            RES schedule, supply bound, cost floor. RES is not mined. Formally verified monetary
            invariants — not a refinement proof.
          </p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Status"
        >
          <p>
            Formally verified protocol invariants with explicit verification boundaries. Not a
            refinement proof.
          </p>
        </DarkCard>
        <DarkCard title="Architecture" to="/documentation/architecture" linkLabel="Open">
          <p>Three-layer model and explorer vs verifier split.</p>
        </DarkCard>
        <DarkCard title="RFC" to="/documentation/rfc" linkLabel="Overview">
          <p>Frozen constitution, consensus model, mathematics.</p>
        </DarkCard>
        <DarkCard title="Security Architecture" to="/documentation/security" linkLabel="Open">
          <p>Minimal scripts, dual native assets, Zero Trust. equivalence_claimed = false.</p>
        </DarkCard>
        <DarkCard
          title="Minimal Script Architecture"
          to="/documentation/security/minimal-script-architecture"
          linkLabel="Read"
        >
          <p>Bitcoin UTXO foundation. Restricted witness families. Not a programmable VM.</p>
        </DarkCard>
        <DarkCard title="Developer Guide" to="/documentation/developer-guide" linkLabel="Open">
          <p>Build map, read-only RPC, zeroTrust library.</p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <h2>Zero-Trust Verification</h2>
        <p>
          <Link to="/zero-trust-verification">Open the verification portal</Link>. Design notes:
          <code> docs/release/</code> zero-trust verifier portal report.
        </p>
        <h2>Protocol lab pages</h2>
        <ul>
          <li>
            <Link to="/protocol">Protocol</Link> — frozen rules
          </li>
          <li>
            <Link to="/science">Science</Link> — deterministic state machine
          </li>
          <li>
            <Link to="/lab">Lab</Link> — experimental framing
          </li>
          <li>
            <Link to="/status">Status</Link> — PROOF-034 observer
          </li>
          <li>
            <Link to="/experiment">PROOF-034</Link>
          </li>
        </ul>
      </section>
    </div>
  );
}
