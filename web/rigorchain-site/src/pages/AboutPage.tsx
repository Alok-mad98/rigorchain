import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";
import { CHAIN_NAME, CODENAME, MAINNET_GENESIS, PRODUCT } from "../protocolFacts";

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
          RIGOR is a dual-asset UTXO protocol. Asset class is inferred from the output script.{" "}
          <strong>ORU</strong> is the fee and proof-of-work subsidy asset. <strong>RES</strong> is
          created only when a body transaction destroys ORU under consensus rules. RES is not a
          mining reward. Proof-of-work is <strong>OpuHash</strong>. Difficulty adjustment is ASERT.
          There is no general-purpose script machine. Formal verification covers selected
          consensus properties through a machine-checked Coq model. The production implementation
          remains the operational consensus implementation.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Protocol identity</h2>
        <table className="data-table">
          <tbody>
            <tr>
              <td>Product</td>
              <td className="mono">
                {PRODUCT} ({CODENAME})
              </td>
            </tr>
            <tr>
              <td>Chain</td>
              <td className="mono">{CHAIN_NAME}</td>
            </tr>
            <tr>
              <td>Mainnet genesis</td>
              <td className="mono">{MAINNET_GENESIS}</td>
            </tr>
            <tr>
              <td>Proof-of-work</td>
              <td className="mono">OpuHash · ASERT</td>
            </tr>
            <tr>
              <td>Public network</td>
              <td>RIGOR Testnet. Connection details are on the testnet page.</td>
            </tr>
          </tbody>
        </table>
      </section>

      <section className="grid-3">
        <article className="card">
          <h3>No premine</h3>
          <p>Issuance follows consensus rules. This site does not sell an allocation.</p>
        </article>
        <article className="card">
          <h3>Proof of Work</h3>
          <p>
            Miners grind OpuHash. Pool policy cannot mint RES. SOLO copies node GBT coinbase bytes.
          </p>
        </article>
        <article className="card">
          <h3>Formal Verification</h3>
          <p>
            <Link to="/documentation/formal-verification">A public verification overview</Link> of
            selected consensus properties.
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
            <Link to="/documentation/formal-verification">Read formal verification</Link>
          </li>
        </ol>
      </section>
    </div>
  );
}
