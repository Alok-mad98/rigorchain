import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";

export function AboutPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">RIGOR</p>
        <h1>About RIGOR</h1>
        <p className="muted">Official public identity for www.rigorchain.com</p>
      </header>

      <section className="card prose-section">
        <h2>What is RIGOR?</h2>
        <p>
          RIGOR is a dual-asset UTXO protocol built on script-derived asset
          classification, stateless monetary architecture, and formally verified
          consensus invariants.{" "}
          <strong>ORU</strong> provides PoW security and fees. <strong>RES</strong> is
          deterministically issued after Convergence — not mined, not a PoW reward. Script is the
          compliance/classification layer. Formal verification covers a model and finite vectors;
          the C++ node is not claimed equivalent (<code>equivalence_claimed = false</code>).
        </p>
      </section>

      <section className="grid-3">
        <article className="card">
          <h3>No premine</h3>
          <p>Issuance follows frozen subsidy and mint rules. This site does not sell allocation.</p>
        </article>
        <article className="card">
          <h3>Proof of Work</h3>
          <p>Miners find valid headers. Pool policy cannot mint RES.</p>
        </article>
        <article className="card">
          <h3>Zero Trust</h3>
          <p>
            <Link to="/zero-trust-verification">Verify a TXID or block hash</Link> locally.
          </p>
        </article>
      </section>

      <section className="card prose-section">
        <h2>How to use it (30 seconds)</h2>
        <ol className="journey-list">
          <li>
            <Link to="/download">Download</Link> wallet and miner
          </li>
          <li>
            <Link to="/mining">Start mining</Link> and receive ORU in the wallet
          </li>
          <li>
            Observe <Link to="/wallet/convergence">Convergence</Link> and RES rules
          </li>
          <li>
            <Link to="/zero-trust-verification">Verify independently</Link>
          </li>
        </ol>
      </section>
    </div>
  );
}
