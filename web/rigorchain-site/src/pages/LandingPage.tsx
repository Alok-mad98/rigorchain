import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { ThreeLayerModel } from "../components/ThreeLayerModel";
import { Rfc020LivePanel } from "../components/Rfc020LivePanel";
import { useSystemState } from "../context/SystemStateContext";
import { readRfc020Live } from "../lib/rfc020Status";
import {
  ASERT_HALF_LIFE_S,
  COQ_ENFORCED_VECTORS,
  MAINNET_GENESIS,
  MAINNET_HRP,
  MAINNET_RPC_PORT,
  OPUHASH,
  SHARED_GOLDEN_VECTORS,
  TARGET_SPACING_S,
} from "../protocolFacts";

export function LandingPage() {
  const { state } = useSystemState();
  const live = readRfc020Live(state.convergence_status);

  return (
    <div className="page landing-page">
      <section className="hero">
        <p className="tag">WWW.RIGORCHAIN.COM</p>
        <h1>RIGOR</h1>
        <p className="hero-sub">
          A dual-asset UTXO protocol. Asset class is inferred from scriptPubKey.
          Formal results are properties of the protocol model, not C++ equivalence.
        </p>
      </section>

      <section className="card prose-section">
        <h2>What is RIGOR?</h2>
        <p>
          RIGOR is a dual-asset UTXO protocol. Asset class is inferred from{" "}
          <code>scriptPubKey</code>. ORU is the fee and PoW-subsidy asset. RES is minted only in
          Convergence: the same body transaction destroys ORU to mint RES at <code>destroy_cost(h)</code> — not from PoW. There is no Script VM, no token
          registry, and no account ledger. Code is the source of truth. Formal invariants are
          properties of the protocol model (
          <code>equivalence_claimed = false</code>).
        </p>
        <pre className="pipeline-diagram">{`RIGOR =
    Dual Native Assets (ORU, RES)
  + UTXO classification (no Script VM)
  + Deterministic issuance
  + Deflationary fee destroy
  + Convergence destroy-to-mint (destroy_cost(h); cap 2²² RES)
  + OpuHash = ${OPUHASH}
  + ASERT (spacing ${TARGET_SPACING_S}s, half-life ${ASERT_HALF_LIFE_S}s)
  + Ed25519 ownership

equivalence_claimed = false`}</pre>
      </section>

      <SectionTitle>Core identity</SectionTitle>
      <section className="identity-grid">
        <DarkCard title="PoW" to="/mining" linkLabel="Mining">
          <p>
            OpuHash = {OPUHASH}. ASERT half-life {ASERT_HALF_LIFE_S / 3600} hours. Pool policy
            cannot mint RES.
          </p>
        </DarkCard>
        <DarkCard title="ORU" to="/wallet/oru" linkLabel="Wallet · ORU">
          <p>Execution and fee resource. Flat subsidy 128 ORU per block (h ≥ 1).</p>
        </DarkCard>
        <DarkCard title="RES" to="/wallet/res" linkLabel="Wallet · RES">
          <p>Reserve asset. Deterministic issuance after Convergence — not mined. Capped supply.</p>
        </DarkCard>
        <DarkCard title="Convergence" to="/wallet/convergence" linkLabel="Wallet · Convergence">
          <p>Destroy ORU via OP_CONVERGE to mint RES at destroy_cost(h).</p>
        </DarkCard>
      </section>

      <section className="cta-row">
        <Button to="/download">Download</Button>
        <Button to="/documentation">Documentation</Button>
        <Button to="/documentation/formal-verification">Formal verification</Button>
      </section>

      <SectionTitle>Start in 30 seconds</SectionTitle>
      <section className="card prose-section">
        <ol className="journey-list">
          <li>
            <Link to="/about">Understand RIGOR</Link>
          </li>
          <li>
            <Link to="/download">Download wallet</Link> then <Link to="/download/miner">miner</Link>
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
        <h2>Verification model</h2>
        <p>
          Formal verification covers protocol-model invariants and a finite correspondence lock (
          {SHARED_GOLDEN_VECTORS} / {COQ_ENFORCED_VECTORS} in <code>golden_lock.json</code>). It is
          not a refinement proof. OpuHash and ASERT are implemented in C++ and are not Coq
          theorems. <span className="mono">equivalence_claimed = false</span>.
        </p>
        <p className="muted">
          Mainnet genesis <code>{MAINNET_GENESIS}</code> · HRP <code>{MAINNET_HRP}</code> · RPC{" "}
          {MAINNET_RPC_PORT}
        </p>
      </section>

      <SectionTitle>Three-layer model</SectionTitle>
      <ThreeLayerModel />

      <section className="grid-3">
        <article className="card">
          <h3>Shipping mint economics</h3>
          <p className="math-block">
            R · (2²⁵ + h)² ≤ B ≪ 40
          </p>
          <p className="muted">Genesis cost 1024 ORU per 1 RES. Cap 2²² RES. Draw is retired.</p>
        </article>
        <article className="card">
          <h3>Flat ORU subsidy</h3>
          <p className="math-block">R(h) = 128 ORU &nbsp; (h ≥ 1)</p>
          <p className="muted">Genesis pays 0. No halvings. RES mint cost is destroy_cost(h).</p>
        </article>
        <article className="card">
          <h3>Live observation</h3>
          <dl className="kv-grid">
            <dt>Tip</dt>
            <dd className="mono">{live.tipHeight ?? state.chain_height ?? "—"}</dd>
            <dt>Confirm h</dt>
            <dd className="mono">{live.confirmHeight ?? "—"}</dd>
            <dt>destroy_cost(h)</dt>
            <dd className="mono">{live.destroyCostOru} ORU</dd>
            <dt>Status</dt>
            <dd>{state.global_status}</dd>
            <dt>equivalence_claimed</dt>
            <dd className="mono">false</dd>
          </dl>
        </article>
      </section>
      <Rfc020LivePanel />

      <section className="pipeline-diagram mono">
        OpuHash → Block → Convergence destroy → same-tx RES mint at destroy_cost(h)
      </section>
    </div>
  );
}
