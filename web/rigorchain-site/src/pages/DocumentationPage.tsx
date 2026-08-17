import { Link } from "react-router-dom";
import { SectionHub } from "../components/SectionHub";
import { SubNav } from "../components/SubNav";
import { SECTIONS } from "../ia/sitemap";

const SECTION = SECTIONS.find((s) => s.id === "documentation")!;

export function DocumentationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Documentation</h1>
        <p className="muted">Protocol specification, lab narrative, and the AlphaPlus trust layer.</p>
      </header>
      <SectionHub section={SECTION} />
      <section className="card prose-section">
        <h2>Zero-Trust Verification</h2>
        <p>
          <Link to="/zero-trust-verification">Open the verification portal</Link>. Design notes:
          <code> docs/release/ALPHAPLUS_ZERO_TRUST_VERIFIER_WEB_PORTAL_REPORT.md</code>.
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
