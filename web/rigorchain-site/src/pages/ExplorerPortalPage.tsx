import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import { explorerBaseUrl } from "../lib/explorerClient";

export function ExplorerPortalPage() {
  const explorer = explorerBaseUrl();
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Explorer</h1>
        <p className="muted">Block data viewing system — not a validation authority.</p>
      </header>

      <SectionTitle>Surfaces</SectionTitle>
      <section className="card-grid">
        <DarkCard title="Block data" to="/explorer/blocks" linkLabel="Blocks">
          <p>Height, hash, transactions. Copy identifiers; do not treat the page as consensus.</p>
        </DarkCard>
        <DarkCard title="ORU statistics" to="/explorer/oru-supply" linkLabel="ORU supply">
          <p>Indexed ORU amounts. Defer to rigord on mismatch.</p>
        </DarkCard>
        <DarkCard title="RES statistics" to="/explorer/res" linkLabel="RES">
          <p>Indexed RES amounts and mint events.</p>
        </DarkCard>
        <DarkCard title="Consensus Fingerprint" to="/explorer/fingerprint" linkLabel="Fingerprint">
          <p>Observational Draw Fingerprint. Not a verdict. Verify ConvergenceRoot locally.</p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <h2>Website → Explorer → Verification</h2>
        <p className="pipeline-diagram mono">
          www.rigorchain.com → Explorer (viewer) → Zero-Trust Verification (local compute)
        </p>
        <p>
          Copy a TXID or block hash, then paste it into{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link>. Explorer already
          exposes “Verify independently” links to this portal.
        </p>
        {explorer ? (
          <p>
            Live viewer:{" "}
            <a href={explorer} rel="noreferrer" target="_blank">
              {explorer}
            </a>
          </p>
        ) : (
          <p className="muted">
            Explorer URL is not configured in this build (<code>VITE_EXPLORER_API_URL</code>).
          </p>
        )}
      </section>
    </div>
  );
}
