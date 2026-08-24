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
        <p className="tag">RIGOR TRUST LAYER</p>
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
          Verification here is based on deterministic rules, public identifiers as inputs,
          reproducible local computation, and independent validation. It does not claim that
          attacks are “mathematically proven impossible,” except where a specific Coq-model theorem
          says so — and even then the theorem is about the model, not the shipping node.
        </p>
        <p>
          Formal correspondence status:{" "}
          <Link to="/documentation/formal-verification">Formal Verification</Link>. Open the
          working verifier: <Link to="/zero-trust-verification">Zero-Trust Verification</Link>
        </p>
      </section>

      <SectionHub section={ZERO_TRUST} />
    </div>
  );
}
