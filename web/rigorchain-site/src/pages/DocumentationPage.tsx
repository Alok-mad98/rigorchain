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
          Architecture, explanatory RFCs, security boundaries, and developer surfaces. Code is
          SSOT.
        </p>
      </header>

      <SectionTitle>Library</SectionTitle>
      <section className="card-grid doc-library-grid">
        <DarkCard title="Whitepaper" to="/documentation/whitepaper" linkLabel="Read">
          <p>Issuance and Coq kernel boundary. cpp_equivalence = false.</p>
        </DarkCard>
        <DarkCard
          title="Continuous Reserve Issuance"
          to="/documentation/monetary-policy"
          linkLabel="Open"
        >
          <p>
            destroy_cost(h) at the confirming height, flat 128 ORU, cap 2²² RES. Epoch 210000 is not a mint
            argument.
          </p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Status"
        >
          <p>
            Inside the Coq consensus kernel model, critical consensus invariants have been
            machine-checked, with explicit extraction and runtime trust boundaries. Not a C++
            refinement.
          </p>
        </DarkCard>
        <DarkCard title="Architecture" to="/documentation/architecture" linkLabel="Open">
          <p>Three-layer model and explorer vs verifier split.</p>
        </DarkCard>
        <DarkCard title="RFC" to="/documentation/rfc" linkLabel="Overview">
          <p>Shipping issuance is Continuous Reserve Issuance. Historical RFCs do not supersede core/src/.</p>
        </DarkCard>
        <DarkCard title="Security Architecture" to="/documentation/security" linkLabel="Open">
          <p>Minimal scripts and dual native assets. equivalence_claimed = false.</p>
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
