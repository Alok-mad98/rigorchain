import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";
import { SectionHub } from "../components/SectionHub";
import { SECTIONS } from "../ia/sitemap";

const ZERO_TRUST = SECTIONS.find((s) => s.id === "zero-trust")!;

export function WhatIsZeroTrustPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">ALPHA PLUS TRUST LAYER</p>
        <h1>What is Zero Trust?</h1>
        <p className="muted">You do not need to trust a node. You can independently verify the result yourself.</p>
      </header>

      <section className="card prose-section">
        <p>
          Nodes do not agree by trusting. Nodes agree by producing identical outputs. The official
          portal recomputes ConvergenceRoot, Sigma, and winner selection in this browser using
          WebCrypto. Explorer, wallet UI, and pool dashboards are viewers — not authorities.
        </p>
        <p>
          Open the working verifier:{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link>
        </p>
      </section>

      <SectionHub section={ZERO_TRUST} />
    </div>
  );
}
