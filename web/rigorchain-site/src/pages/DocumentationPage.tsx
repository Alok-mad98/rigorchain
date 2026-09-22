import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function DocumentationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Documentation</h1>
        <p className="muted">
          Architecture, assets, security, and verification. This library is a public overview.
        </p>
      </header>

      <SectionTitle>Library</SectionTitle>
      <section className="card-grid doc-library-grid">
        <DarkCard title="Whitepaper" to="/documentation/whitepaper" linkLabel="Read">
          <p>
            Dual-asset architecture, Convergence, verification overview, and security model.
          </p>
        </DarkCard>
        <DarkCard
          title="Continuous Reserve Issuance"
          to="/documentation/monetary-policy"
          linkLabel="Open"
        >
          <p>
            ORU, RES, and the destroy-to-create relationship. The complete issuance specification
            will be published with mainnet launch.
          </p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Status"
        >
          <p>
            Selected consensus properties are machine-checked in a Coq model. The production
            implementation remains the operational consensus implementation.
          </p>
        </DarkCard>
        <DarkCard title="Architecture" to="/documentation/architecture" linkLabel="Open">
          <p>Three-layer model and explorer vs verifier split.</p>
        </DarkCard>
        <DarkCard title="RFC" to="/documentation/rfc" linkLabel="Overview">
          <p>Issuance is defined by consensus. This index is an overview, not the normative specification.</p>
        </DarkCard>
        <DarkCard title="Security Architecture" to="/documentation/security" linkLabel="Open">
          <p>Minimal scripts and two native assets. No general-purpose execution layer.</p>
        </DarkCard>
        <DarkCard
          title="Minimal Script Architecture"
          to="/documentation/security/minimal-script-architecture"
          linkLabel="Read"
        >
          <p>Bitcoin UTXO foundation. Restricted witness families. Not a programmable VM.</p>
        </DarkCard>
        <DarkCard title="Developer Guide" to="/documentation/developer-guide" linkLabel="Open">
          <p>Build map and read-only RPC.</p>
        </DarkCard>
      </section>
    </div>
  );
}
