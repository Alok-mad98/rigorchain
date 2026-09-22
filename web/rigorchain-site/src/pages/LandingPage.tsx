import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { useSystemState } from "../context/SystemStateContext";
import { MAINNET_GENESIS } from "../protocolFacts";

export function LandingPage() {
  const { state } = useSystemState();

  return (
    <div className="page landing-page">
      <section className="hero">
        <p className="tag">WWW.RIGORCHAIN.COM</p>
        <h1>RIGOR</h1>
        <p className="hero-sub">
          A dual-asset UTXO protocol. Proof-of-work orders the chain. RES is created by destroying
          ORU, not by mining.
        </p>
      </section>

      <section className="card prose-section">
        <h2>What is RIGOR?</h2>
        <p>
          RIGOR is a dual-asset UTXO protocol. Asset class is inferred from the output script.{" "}
          <strong>ORU</strong> is the fee and proof-of-work subsidy asset. <strong>RES</strong> is
          the reserve asset. RES may be created only in Convergence: the same body transaction
          destroys ORU and may create RES when consensus admits the operation. There is no
          general-purpose script machine, no token registry, and no account ledger.
        </p>
        <p>
          Formal verification covers selected consensus properties through a machine-checked Coq
          model. The production implementation remains the operational consensus implementation.
        </p>
      </section>

      <SectionTitle>Core identity</SectionTitle>
      <section className="identity-grid">
        <DarkCard title="Proof-of-work" to="/mining" linkLabel="Mining">
          <p>The chain is ordered by OpuHash. Pool policy cannot create RES.</p>
        </DarkCard>
        <DarkCard title="ORU" to="/wallet/oru" linkLabel="Wallet · ORU">
          <p>Execution and fee resource. Issued as a fixed per-block subsidy after genesis.</p>
        </DarkCard>
        <DarkCard title="RES" to="/wallet/res" linkLabel="Wallet · RES">
          <p>Reserve asset. Created through Convergence. Not mined.</p>
        </DarkCard>
        <DarkCard title="Convergence" to="/wallet/convergence" linkLabel="Wallet · Convergence">
          <p>Destroy ORU to create RES in the same transaction, under consensus rules.</p>
        </DarkCard>
      </section>

      <section className="cta-row">
        <Button to="/download">Download</Button>
        <Button to="/documentation">Documentation</Button>
        <Button to="/documentation/formal-verification">Formal verification</Button>
      </section>

      <SectionTitle>Start here</SectionTitle>
      <section className="card prose-section">
        <ol className="journey-list">
          <li>
            <Link to="/about">Understand RIGOR</Link>
          </li>
          <li>
            <Link to="/testnet">Join the public test network</Link> (download, faucet, Explorer)
          </li>
          <li>
            <Link to="/mining">Start mining</Link> and observe ORU in the{" "}
            <Link to="/wallet">wallet</Link>
          </li>
          <li>
            Read <Link to="/wallet/convergence">Convergence</Link>, then{" "}
            <Link to="/documentation/formal-verification">formal verification</Link>
          </li>
        </ol>
      </section>

      <section className="card prose-section">
        <h2>Verification</h2>
        <p>
          Formal verification covers selected consensus properties through a machine-checked Coq
          model. The production implementation remains the operational consensus implementation.
          The verification effort does not claim full binary equivalence between the model and
          production software.
        </p>
        <p className="muted">
          Published genesis <code>{MAINNET_GENESIS}</code>
        </p>
      </section>

      <SectionTitle>How the pieces fit</SectionTitle>
      <ThreeLayerModel />

      <section className="grid-3">
        <article className="card">
          <h3>Reserve issuance</h3>
          <p>
            RES issuance follows deterministic protocol rules designed to balance participation,
            scarcity, and long-term reserve behavior.
          </p>
          <p className="muted">The complete specification will be published with mainnet launch.</p>
        </article>
        <article className="card">
          <h3>ORU subsidy</h3>
          <p>ORU is issued on a fixed per-block subsidy after genesis. RES is not that subsidy.</p>
        </article>
        <article className="card">
          <h3>Network observation</h3>
          <dl className="kv-grid">
            <dt>Tip height</dt>
            <dd className="mono">{state.chain_height ?? "—"}</dd>
            <dt>Best block</dt>
            <dd className="mono truncate">{state.best_block_hash ?? "—"}</dd>
          </dl>
        </article>
      </section>

      <section className="pipeline-diagram mono">
        OpuHash → Block → Convergence destroy → RES created in the same transaction
      </section>
    </div>
  );
}
