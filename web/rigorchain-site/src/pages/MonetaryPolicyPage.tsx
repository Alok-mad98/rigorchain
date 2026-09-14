import { Link } from "react-router-dom";
import { Rfc020LivePanel } from "../components/Rfc020LivePanel";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import {
  CUMULATIVE_ISSUANCE_CAPTION,
  RES_COIN,
  RES_DERIVED_MAX_SUPPLY,
  UNIT_PRICE_CURVE_CAPTION,
  resDestroyCostForHeight,
} from "../lib/resIssuanceSchedule";

/**
 * Public Documentation → Continuous Reserve Issuance (RFC-020).
 * Keep equivalence_claimed = false. Do not claim “complete monetary correctness.”
 */

const HEIGHT_PINS = [0, 210000, 420000, 630000] as const;

function formatUnits(atoms: bigint, coin: bigint): string {
  const whole = atoms / coin;
  const frac = atoms % coin;
  if (frac === 0n) return whole.toString();
  return `${whole}.${frac.toString().padStart(8, "0").replace(/0+$/, "")}`;
}

export function MonetaryPolicyPage() {
  const derivedRes = formatUnits(RES_DERIVED_MAX_SUPPLY, RES_COIN);

  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">Continuous Reserve Issuance</p>
        <h1>Continuous Reserve Issuance</h1>
        <p className="muted">
          RES is minted in a body transaction at the confirming-height protocol cost. ORU is flat 128 per block after
          genesis. Formally verified monetary invariants — not a refinement proof.
        </p>
      </header>
      <Rfc020LivePanel title="This node (regtest / RPC)" />

      <aside className="whitepaper-banner card" role="note">
        <p>
          <span className="mono">equivalence_claimed = false</span>
        </p>
        <p>
          Allowed claim: <strong>formally verified monetary invariants</strong>. Forbidden claim:
          “mathematically proven complete monetary correctness.”
        </p>
      </aside>

      <SectionTitle>1. Two assets</SectionTitle>
      <section className="card prose-section">
        <p>
          <strong>ORU</strong> is the PoW / fee / Convergence-destroy asset (
          <code>R(0) = 0</code>; then <code>128</code> ORU/block forever; no hard cap).{" "}
          <strong>RES</strong> is the scarce settlement asset. Its maximum supply is the closed form{" "}
          <code>2²²</code> RES: <code>{RES_DERIVED_MAX_SUPPLY.toString()}</code> atoms (
          {derivedRes} RES).
        </p>
        <p>
          Actual circulating supply equals participation times that ceiling. The cap is a hard bound
          — not a promise that the ceiling will be filled.
        </p>
      </section>

      <SectionTitle>2. Continuous destroy cost</SectionTitle>
      <section className="card prose-section">
        <p className="math-block">destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰</p>
        <p>
          Convergence destroys ORU to mint RES. Display <code>destroy_cost(h)</code> is ORU destroyed to
          mint 1 RES (C++ <code>&gt;&gt; 50</code>). Consensus never evaluates the price. It
          evaluates one inequality:
        </p>
        <p className="math-block">R · (2²⁵ + h)² ≤ B &lt;&lt; 40</p>
        <p>
          <code>R = 0</code> is a legal plain destroy. The price never hits zero, so there is no mint
          horizon. <code>RES_COIN = {RES_COIN.toString()}</code> (eight decimals).{" "}
          <code>destroy_cost(0) = 102400000000</code> (1024 ORU destroyed to mint 1 RES). The cost
          hardens every height. There is no epoch step.
        </p>
        <p className="muted">{UNIT_PRICE_CURVE_CAPTION}</p>
      </section>

      <SectionTitle>3. Selected height pins</SectionTitle>
      <section className="card prose-section">
        <table className="doc-table">
          <thead>
            <tr>
              <th>Height</th>
              <th>destroy_cost(h) atoms</th>
              <th>destroy_cost(h) ORU per 1 RES</th>
            </tr>
          </thead>
          <tbody>
            {HEIGHT_PINS.map((h) => {
              const atoms = resDestroyCostForHeight(h);
              return (
                <tr key={h}>
                  <td>{h}</td>
                  <td>{atoms.toString()}</td>
                  <td>{formatUnits(atoms, RES_COIN)}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
        <p className="muted">
          Mint-output maturity 100 is reorg protection. Shipping issuance uses continuous{" "}
          <code>destroy_cost(h)</code>.
        </p>
      </section>

      <SectionTitle>4. Closed-form cap</SectionTitle>
      <section className="card prose-section">
        <p>
          <code>RES_MAX_SUPPLY = (S / P0) · H = 2⁷ / 2¹⁰ · 2²⁵ = 2²²</code> RES. Cumulative ceiling
          through height <code>h</code> is <code>cap · h / (h + 2²⁵)</code>.
        </p>
        <p className="math-block">
          {RES_DERIVED_MAX_SUPPLY.toString()} atoms = {derivedRes} RES
        </p>
        <p className="muted">{CUMULATIVE_ISSUANCE_CAPTION}</p>
      </section>

      <SectionTitle>5. Formal verification status</SectionTitle>
      <section className="card prose-section">
        <ul>
          <li>
            Shipping issuance is Convergence: destroy ORU to mint RES at{" "}
            <code>R · (2²⁵+h)² ≤ B &lt;&lt; 40</code>
          </li>
          <li>
            <code>equivalence_claimed = false</code> — shipping C++ is not proved equivalent to the
            Coq model
          </li>
        </ul>
        <p>
          Details:{" "}
          <Link to="/documentation/formal-verification">Formal Verification</Link> · adoption record
          under the economics adoption record.
        </p>
      </section>
    </div>
  );
}
