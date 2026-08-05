import { Link } from "react-router-dom";
import { ProtocolCard } from "../components/ProtocolCard";
import { ThreeLayerModel } from "../components/ThreeLayerModel";

export function ProtocolPage() {
  return (
    <div className="page protocol-page">
      <header className="page-header">
        <h1>Protocol layer</h1>
        <p className="muted">
          Frozen specification only — deterministic rules with no runtime equivalence claim
        </p>
      </header>

      <section className="card prose-section">
        <p>
          This page describes <strong>Layer 1 — Protocol (Frozen)</strong>. These rules are fixed; live
          chain behavior is observed separately under PROOF-034 and may differ in completeness until
          archive milestones are materialized.
        </p>
      </section>

      <div className="card-grid">
        <ProtocolCard variant="overview" />
        <ProtocolCard variant="oru" />
        <ProtocolCard variant="res" />
        <ProtocolCard variant="forge" />
      </div>

      <section className="card prose-section">
        <h2>Consensus</h2>
        <p>
          Block production uses <strong>DurinHash</strong> proof-of-work over an unchanged 80-byte header.
          Mining discovers valid blocks; validation applies deterministic morphism F. No production
          mainnet finality is claimed.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Convergence draw</h2>
        <ol>
          <li>Build candidate set F from convergence-shaped transactions (|F| ≥ 2 for draw).</li>
          <li>Compute convergence root over sorted txids.</li>
          <li>Evaluate σ digest per candidate; winner from integer selection rule (P = 1/65536).</li>
          <li>RES mint when PCORB witness and cap predicates pass.</li>
        </ol>
      </section>

      <section className="card prose-section">
        <h2>Sigma &amp; PCORB</h2>
        <p>
          <strong>σ (sigma)</strong> — cryptographic selection witness binding the winner to F.
        </p>
        <p>
          <strong>PCORB</strong> — structured verification vector in the RES mint body (layout + Ed25519).
        </p>
      </section>

      <section className="card prose-section">
        <h2>UTXO model</h2>
        <p>
          Script-as-asset: asset class inferred from scriptPubKey. No parallel asset field in storage. No
          global account state as consensus truth.
        </p>
      </section>

      <h2 className="section-title">Layer separation</h2>
      <ThreeLayerModel compact />

      <p className="muted">
        Runtime verification: <Link to="/status">status</Link> ·{" "}
        <Link to="/experiment">PROOF-034</Link>
      </p>
    </div>
  );
}
