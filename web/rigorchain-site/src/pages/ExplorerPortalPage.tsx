import { Link } from "react-router-dom";
import { SectionHub } from "../components/SectionHub";
import { SubNav } from "../components/SubNav";
import { explorerBaseUrl } from "../lib/explorerClient";
import { SECTIONS } from "../ia/sitemap";

const SECTION = SECTIONS.find((s) => s.id === "explorer")!;

export function ExplorerPortalPage() {
  const explorer = explorerBaseUrl();
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Explorer</h1>
        <p className="muted">Blockchain data viewing system — not a validation authority.</p>
      </header>
      <SectionHub section={SECTION} />
      <section className="card prose-section">
        <h2>Website → Explorer → Verification</h2>
        <p className="pipeline-diagram mono">
          www.rigorchain.com → Explorer (viewer) → Zero-Trust Verification (local compute)
        </p>
        <p>
          Copy a TXID or block hash from the explorer, then paste it into{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link>. Explorer already
          exposes “Verify independently” links to this portal.
        </p>
      </section>
      <section className="card prose-section">
        <p>
          Explorer copies public RPC fields for display. It must not be treated as consensus. For
          mathematical verification use the independent{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link> portal.
        </p>
        {explorer ? (
          <p>
            Open the live viewer:{" "}
            <a href={explorer} rel="noreferrer" target="_blank">
              {explorer}
            </a>
          </p>
        ) : (
          <p className="muted">Explorer URL is not configured in this build (<code>VITE_EXPLORER_API_URL</code>).</p>
        )}
      </section>
    </div>
  );
}
