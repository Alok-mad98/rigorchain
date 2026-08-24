import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function SecurityArchitecturePage() {
  return (
    <div className="page">
      <SubNav />

      <nav className="whitepaper-toc card" aria-label="Documentation path">
        <p className="muted">
          <Link to="/documentation">Documentation</Link>
          {" · "}
          <span>Security Architecture</span>
          {" · "}
          <Link to="/documentation/security/minimal-script-architecture">
            Minimal Script Architecture
          </Link>
        </p>
      </nav>

      <header className="page-header">
        <p className="tag">SECURITY ARCHITECTURE</p>
        <h1>Security Architecture</h1>
        <p className="muted">
          Security through minimalism. Deterministic validation. Independent recomputation.
        </p>
      </header>

      <aside className="whitepaper-banner card" role="note">
        <p>
          This section describes protocol security architecture. It does not change consensus
          rules.
          <br />
          <span className="mono">equivalence_claimed = false.</span>
        </p>
      </aside>

      <section className="card prose-section">
        <h2>Priority</h2>
        <ol>
          <li>Reduce consensus complexity</li>
          <li>Reduce execution surface</li>
          <li>Preserve deterministic validation</li>
          <li>Avoid unnecessary programmability</li>
          <li>Make verification possible by independent nodes</li>
        </ol>
        <p>
          RIGOR does not attempt to compete with programmable blockchain platforms. A smaller
          execution surface creates stronger verification capability.
        </p>
        <p>
          RIGOR minimizes script complexity rather than eliminating scripts. The protocol
          intentionally restricts execution semantics to preserve deterministic verification and
          reduce consensus attack surface.
        </p>
        <p>
          Constrained transaction validation, asset-specific script rules, and deterministic
          verification logic. No general-purpose VM, no arbitrary smart contracts, no programmable
          execution layer.
        </p>
        <pre className="pipeline-diagram">{`Less capability
       ↓
Less complexity
       ↓
Smaller attack surface
       ↓
Stronger mathematical verification`}</pre>
      </section>

      <SectionTitle>Surfaces</SectionTitle>
      <section className="card-grid">
        <DarkCard
          title="Minimal Script Architecture"
          to="/documentation/security/minimal-script-architecture"
          linkLabel="Read"
        >
          <p>
            Bitcoin UTXO foundation, script-defined asset identity, native ORU and RES, restricted
            witness families. Witness v1 is not Taproot.
          </p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Freeze status"
        >
          <p>Strongly verified Coq model. Finite correspondence. Not a refinement proof.</p>
        </DarkCard>
        <DarkCard
          title="Zero Trust Verification"
          to="/zero-trust-verification"
          linkLabel="Verify independently"
        >
          <p>Recompute public bytes locally. Equal bytes are accepted. Unequal bytes are rejected.</p>
        </DarkCard>
        <DarkCard title="Whitepaper" to="/documentation/whitepaper" linkLabel="Read">
          <p>Protocol paper, including Chapter 3 — Minimal Script Architecture.</p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <h2>Execution surfaces</h2>
        <p>Scripts exist. Witness v0, P2WSH, Witness v1, Ed25519 signature verification, and OP_CONVERGE are real. They are constrained, not a contract VM.</p>
        <p>
          GitHub:{" "}
          <a href="https://github.com/rigorchain/rigorchain/blob/main/docs/security/minimal-script-architecture.md">
            docs/security/minimal-script-architecture.md
          </a>
          . Registry:{" "}
          <a href="https://github.com/rigorchain/rigorchain/blob/main/docs/security/RIGOR_SCRIPT_TYPE_REGISTRY.md">
            script type registry
          </a>
          . Boundary:{" "}
          <a href="https://github.com/rigorchain/rigorchain/blob/main/docs/security/RIGOR_SCRIPT_SECURITY_BOUNDARY.md">
            security boundary
          </a>
          . Audit:{" "}
          <a href="https://github.com/rigorchain/rigorchain/blob/main/docs/security/RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md">
            consistency audit
          </a>
          .
        </p>
      </section>

      <section className="card prose-section">
        <h2>What this site is not</h2>
        <p>
          The website, explorer, wallet, and pool are observers. They are not consensus
          authorities. Trust is established by independent verification of public bytes.
        </p>
      </section>
    </div>
  );
}
