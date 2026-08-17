import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";

export function AboutPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">ALPHAPLUS</p>
        <h1>About AlphaPlus</h1>
        <p className="muted">Official public identity for www.rigorchain.com</p>
      </header>

      <section className="card prose-section">
        <h2>What is AlphaPlus?</h2>
        <p>
          AlphaPlus is a deterministic proof-of-work protocol with two native assets: <strong>ORU</strong>{" "}
          (execution and fees) and <strong>RES</strong> (scarce settlement via Convergence). There is no
          premine. Mining secures ordering. You do not need to trust a node — you can independently
          verify results in the browser.
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
