import { DarkCard } from "../components/DarkCard";
import { SubNav } from "../components/SubNav";

export function ProtocolProofOfWorkPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Proof-of-Work</h1>
        <p className="muted">Why the chain is ordered by work, and why mining does not mint RES.</p>
      </header>

      <section className="card prose-section">
        <p>
          Consensus proof-of-work is <strong>OpuHash</strong>. It supplies a height order that is
          expensive to rewrite. Difficulty adjustment is ASERT. Numerical parameters are part of
          the specification that will be published with mainnet launch.
        </p>
        <p>
          Mining pays ORU. <strong>RES is not a proof-of-work reward.</strong> Pool policy cannot
          create RES. Finding a block does not override the Convergence rules.
        </p>
        <p>
          The production implementation remains the operational consensus implementation. Mining
          software is a product, not a second consensus authority.
        </p>
      </section>

      <section className="card-grid">
        <DarkCard title="Mining" to="/use/mining" linkLabel="Open">
          <p className="muted">TESTNET operator path. Local SOLO workflow.</p>
        </DarkCard>
      </section>
    </div>
  );
}
