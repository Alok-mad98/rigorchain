import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";
import {
  ASERT_HALF_LIFE_S,
  CHAIN_NAME,
  CODENAME,
  MAINNET_GENESIS,
  MAINNET_HRP,
  MAINNET_MAGIC,
  MAINNET_P2P_PORT,
  MAINNET_RPC_PORT,
  OPUHASH,
  PRODUCT,
  TARGET_SPACING_S,
} from "../protocolFacts";

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
          RIGOR is a dual-asset UTXO protocol. Asset class is inferred from{" "}
          <code>scriptPubKey</code>. <strong>ORU</strong> is the fee and PoW-subsidy asset.{" "}
          <strong>RES</strong> is minted only in a body transaction that destroys ORU at{" "}
          <code>destroy_cost(h)</code> — not a PoW reward. Proof-of-work is <strong>OpuHash</strong>: <code>{OPUHASH}</code>. Difficulty is
          ASERT (spacing {TARGET_SPACING_S}s, half-life {ASERT_HALF_LIFE_S}s). There is no Script
          VM. Formal verification covers a model and a finite differential lock; the C++ node is
          not claimed equivalent (<code>equivalence_claimed = false</code>).
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
              <td>Magic</td>
              <td className="mono">{MAINNET_MAGIC}</td>
            </tr>
            <tr>
              <td>P2P / RPC</td>
              <td className="mono">
                {MAINNET_P2P_PORT} / {MAINNET_RPC_PORT}
              </td>
            </tr>
            <tr>
              <td>Bech32 HRP</td>
              <td className="mono">{MAINNET_HRP}</td>
            </tr>
            <tr>
              <td>PoW</td>
              <td className="mono">{OPUHASH}</td>
            </tr>
            <tr>
              <td>ASERT</td>
              <td className="mono">
                spacing {TARGET_SPACING_S}s · half-life {ASERT_HALF_LIFE_S}s
              </td>
            </tr>
          </tbody>
        </table>
      </section>

      <section className="grid-3">
        <article className="card">
          <h3>No premine</h3>
          <p>Issuance follows frozen subsidy and mint rules. This site does not sell allocation.</p>
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
            <Link to="/documentation/formal-verification">Protocol-model invariants</Link> with
            explicit verification boundaries.
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
