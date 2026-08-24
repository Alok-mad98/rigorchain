import { Link } from "react-router-dom";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

/**
 * Public Documentation → Deterministic Reserve Issuance.
 * Keep equivalence_claimed = false. Do not claim “complete monetary correctness.”
 */
export function MonetaryPolicyPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">Deterministic Reserve Issuance</p>
        <h1>Deterministic Reserve Issuance</h1>
        <p className="muted">
          RES is not mined. Dual-asset issuance rules. Formally verified monetary invariants — not a
          refinement proof.
        </p>
      </header>

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
          <strong>ORU</strong> is the PoW / fee / Convergence-weight asset (subsidy 1000 → 500 → 250 →
          125 ORU/block; no hard cap). <strong>RES</strong> is the scarce settlement asset with a{" "}
          <strong>maximum</strong> supply of 21,000,000.
        </p>
        <p>
          RES has a maximum supply of 21,000,000. Actual circulating supply depends on Convergence
          participation. The 21M figure is a hard ceiling — not a promise that the cap will be
          filled.
        </p>
      </section>

      <SectionTitle>2. Deterministic Reserve Issuance equation</SectionTitle>
      <section className="card prose-section">
        <p className="math-block">k = height / 210000</p>
        <p>
          For <code>0 ≤ k ≤ 250</code>:
        </p>
        <p className="math-block">W(k) = floor(40 · RES_COIN · (k+1)^(-3/2))</p>
        <p>
          For <code>k &gt; 250</code>: <code>W(k) = 0</code>. <code>RES_COIN = 1_000_000</code> (six
          decimals). A successful Convergence draw mints exactly <code>W(h)</code> RES (at most one
          mint per block). Draw target <code>T</code> is constant (Deterministic Reserve Issuance);
          the expected ORU cost
          per mint is the protocol constant <code>C</code>, so the cost floor{" "}
          <code>C / W(k)</code> weakly rises as rewards decay.
        </p>
      </section>

      <SectionTitle>3. Epoch schedule (selected pins)</SectionTitle>
      <section className="card prose-section">
        <table className="doc-table">
          <thead>
            <tr>
              <th>Epoch k</th>
              <th>W(k) RES</th>
            </tr>
          </thead>
          <tbody>
            <tr>
              <td>0</td>
              <td>40.000000</td>
            </tr>
            <tr>
              <td>1</td>
              <td>14.142196</td>
            </tr>
            <tr>
              <td>10</td>
              <td>1.096405</td>
            </tr>
            <tr>
              <td>100</td>
              <td>0.039407</td>
            </tr>
            <tr>
              <td>250</td>
              <td>0.010058</td>
            </tr>
            <tr>
              <td>251+</td>
              <td>0</td>
            </tr>
          </tbody>
        </table>
        <p className="muted">
          Full table <code>k = 0..251</code> is differential-checked (C++ ≡ Python ≡ Coq pins).
        </p>
      </section>

      <SectionTitle>4. Supply proof (bounded emission)</SectionTitle>
      <section className="card prose-section">
        <p>
          Even if every block from genesis through epoch 250 mints, maximum conceivable issuance is
        </p>
        <p className="math-block">210000 × 99450437 atoms = 20,884,591.770000 RES &lt; 21,000,000</p>
        <p>
          That finite-sum identity is a C++ <code>static_assert</code> and a Coq theorem (
          <code>max_issuance_within_cap</code>). Demand cannot push issuance past this bound under
          the one-mint-per-block rule.
        </p>
      </section>

      <SectionTitle>5. Formal verification status</SectionTitle>
      <section className="card prose-section">
        <ul>
          <li>Cap finite sum, termination, reward / cost-floor monotonicity: model theorems in Coq</li>
          <li>Cross-implementation parity gate for all epochs</li>
          <li>
            <code>equivalence_claimed = false</code> — shipping C++ is not proved equivalent to the
            Coq model
          </li>
        </ul>
        <p>
          Details:{" "}
          <Link to="/documentation/formal-verification">Formal Verification</Link> · audit reports
          under <code>docs/security/RES_MONETARY_*</code>.
        </p>
      </section>
    </div>
  );
}
