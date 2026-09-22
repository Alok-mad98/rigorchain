import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function ProtocolConsensusPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Consensus</h1>
        <p className="muted">Index only. Not a specification.</p>
      </header>

      <section className="card prose-section">
        <ul>
          <li>UTXO validation</li>
          <li>transaction verification</li>
          <li>maturity rules</li>
          <li>consensus boundary</li>
        </ul>
      </section>

      <SectionTitle>Related</SectionTitle>
      <section className="card-grid">
        <DarkCard title="Formal Verification" to="/protocol/verification-boundary" linkLabel="Open" />
        <DarkCard title="Documentation" to="/documentation" linkLabel="Open" />
      </section>
    </div>
  );
}
