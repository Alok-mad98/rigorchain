import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function ProtocolArchitecturePage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">PROTOCOL ARCHITECTURE</p>
        <h1>RIGOR Architecture Overview</h1>
        <p className="muted">
          Public protocol architecture. Not implementation documentation.
        </p>
      </header>

      <section className="card prose-section">
        <h2>Identity</h2>
        <p>
          RIGOR is a Proof-of-Work UTXO architecture exploring separation of two native monetary
          roles within a single consensus-defined ledger.
        </p>
        <p>
          RIGOR is a dual-asset UTXO protocol. Asset class is inferred from{" "}
          <code>scriptPubKey</code>. There is no Script VM, no token registry, and no account
          ledger.
        </p>
      </section>

      <SectionTitle>Design principles</SectionTitle>
      <section className="card prose-section">
        <ul>
          <li>Single UTXO state space</li>
          <li>Script-defined asset identity</li>
          <li>Proof-of-Work chain ordering</li>
          <li>Deterministic monetary transformation</li>
          <li>Minimal consensus surface</li>
        </ul>
      </section>

      <SectionTitle>Two native monetary roles</SectionTitle>
      <section className="card-grid">
        <article className="card dark-card">
          <h3>ORU</h3>
          <p>Transaction and security asset. Fee and PoW-subsidy asset. Not a token registry entry.</p>
        </article>
        <article className="card dark-card">
          <h3>RES</h3>
          <p>
            Scarce asset created through deterministic Convergence. Not a PoW reward.
          </p>
        </article>
      </section>

      <SectionTitle>Verification boundary</SectionTitle>
      <section className="three-layer-grid">
        <article className="card layer-card">
          <h3>Protocol model</h3>
          <p>Formal results are properties of the protocol model.</p>
        </article>
        <article className="card layer-card">
          <h3>Running C++ implementation</h3>
          <p>Not claimed equivalent to the protocol model.</p>
        </article>
        <article className="card layer-card">
          <h3>Product interfaces</h3>
          <p>Wallets, explorer, and other products are not consensus authorities.</p>
        </article>
      </section>
      <section className="card prose-section">
        <p>
          The Coq model, the production software, and product interfaces are different layers.
          Verification does not claim full binary equivalence between the model and production
          software.
        </p>
      </section>

      <SectionTitle>Architecture references</SectionTitle>
      <section className="card-grid">
        <DarkCard title="Protocol Specification" to="/protocol" linkLabel="Open">
          <p>Public protocol index on this site.</p>
        </DarkCard>
        <DarkCard
          title="Formal Verification Boundary"
          to="/protocol/verification-boundary"
          linkLabel="Open"
        >
          <p>Public verification overview. Selected model properties.</p>
        </DarkCard>
        <DarkCard title="Whitepaper" to="/protocol/architecture/whitepaper" linkLabel="Open">
          <p>Protocol design, principles and rationale.</p>
        </DarkCard>
      </section>
    </div>
  );
}
