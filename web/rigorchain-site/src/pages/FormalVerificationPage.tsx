import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const GH = "https://github.com/rigorchain/rigorchain/blob/main";

export function FormalVerificationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">PUBLIC VERIFICATION ARCHITECTURE</p>
        <h1>Formal Verification</h1>
        <p className="muted">
          Formally verified protocol invariants with explicit verification boundaries. Not a
          refinement proof.
        </p>
      </header>

      <aside className="whitepaper-banner card" role="note">
        <p>
          <span className="mono">
            equivalence_claimed = false
            <br />
            hash_kernel_shared_binary_with_cpp = false
            <br />
            refinement_proved = false
          </span>
        </p>
      </aside>

      <section className="card prose-section">
        <h2>Overview</h2>
        <p>
          RIGOR does not ask users to trust code alone. The protocol publishes executable
          verification artifacts: formal specifications, Coq models, golden vectors, and
          cross-implementation differential checks.
        </p>
        <p>
          Formally verified protocol invariants within an explicitly defined verification
          boundary. Layers must not be collapsed into a claim of a “fully formally verified”
          protocol or a “mathematically proven complete blockchain.”
        </p>
        <p>
          Public repository map:{" "}
          <a href={`${GH}/verification/README.md`}>
            <code>verification/</code>
          </a>
          . Reproduce:{" "}
          <a href={`${GH}/docs/security/HOW_TO_VERIFY_RIGOR.md`}>How to Verify RIGOR</a>.
        </p>
      </section>

      <section className="three-layer-grid">
        <article className="card layer-card">
          <h3>Layer 1 — Frozen protocol specification</h3>
          <p>RFC / SPEC / executable Python SSOT. Deterministic rules. This layer is the protocol.</p>
        </article>
        <article className="card layer-card">
          <h3>Layer 2 — Mathematical model</h3>
          <p>
            Strongly verified Coq model: schedule, convergence, dual-pool, script shape. Properties
            of the model, not of shipping C++.
          </p>
        </article>
        <article className="card layer-card">
          <h3>Layer 3 — Runtime differential verification</h3>
          <p>
            Golden vectors and a reproducible differential gate (Python oracle, portable hash CLI,
            production σ/root CLI). Finite correspondence, not refinement.
          </p>
        </article>
      </section>

      <section className="card prose-section">
        <h2>Verification Coverage</h2>
        <p>Verified cone (model and/or differential gate):</p>
        <ul>
          <li>RES monetary policy (schedule, finite-sum cap, cost-floor along schedule)</li>
          <li>Convergence consensus model (σ layout, root order-independence, selection, window)</li>
          <li>UTXO asset rules (dual-pool separation; mint = schedule term in the model)</li>
          <li>Script shape restrictions on accepted model transactions / blocks</li>
        </ul>
        <p>Outside boundary:</p>
        <ul>
          <li>Mining (templates, GBT, solo miner)</li>
          <li>Difficulty / PoW hardness</li>
          <li>Wallet security and UX</li>
          <li>Networking / P2P / mempool</li>
          <li>Complete C++ equivalence (`equivalence_claimed = false`)</li>
        </ul>
      </section>

      <SectionTitle>Living lock</SectionTitle>
      <section className="card-grid">
        <DarkCard title="422 shared golden vectors">
          <p>Python oracle and generated C++ cases. Finite-vector agreement, not equivalence.</p>
        </DarkCard>
        <DarkCard title="215 Coq-enforced vectors">
          <p>
            <code>vm_compute</code> lemmas in the golden harness. A regressed enforced vector
            breaks the Coq build.
          </p>
        </DarkCard>
        <DarkCard title="Tripartite hash validation">
          <p>
            Portable CSHA256 × hashlib × extracted Coq kernel on HashKernel / FIPS vectors. Test
            drivers, not a shared production hash binary.
          </p>
        </DarkCard>
        <DarkCard title="σ / root CLI in the gate">
          <p>
            Production <code>ComputeSigmaHash</code> and{" "}
            <code>ComputeConvergenceRootFromEligibilityTxids</code> vs existing JSON families.
          </p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <h2>Reproducibility</h2>
        <p>Independent reviewers can verify:</p>
        <ul>
          <li>
            <strong>Coq models</strong> — compile <code>formal_verification/coq</code> (
            <code>_CoqProject</code>)
          </li>
          <li>
            <strong>Golden vectors</strong> — lock check against{" "}
            <code>phase3_golden_vectors.json</code> / <code>golden_lock.json</code>
          </li>
          <li>
            <strong>Differential checks</strong> —{" "}
            <code>bash formal_verification/scripts/run_differential_gate.sh</code> (expect PASS)
          </li>
        </ul>
        <p>
          Step-by-step prerequisites and commands:{" "}
          <a href={`${GH}/docs/security/HOW_TO_VERIFY_RIGOR.md`}>HOW_TO_VERIFY_RIGOR.md</a>. Claim
          table:{" "}
          <a href={`${GH}/docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md`}>
            RIGOR_FORMAL_CLAIM_TRACEABILITY.md
          </a>
          .
        </p>
        <pre className="math-block">{`equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false`}</pre>
      </section>

      <section className="card prose-section">
        <h2>Source documents</h2>
        <ul>
          <li>
            <a href={`${GH}/verification/README.md`}>Public verification architecture</a> —{" "}
            <code>verification/</code> map
          </li>
          <li>
            <a href={`${GH}/docs/verification/FORMAL-VERIFICATION-STATUS.md`}>
              Formal Verification Status
            </a>{" "}
            — operator status page
          </li>
          <li>
            <a href={`${GH}/docs/verification/FORMAL-VERIFICATION-VERDICT.md`}>Verdict</a> — what
            the Coq model proves
          </li>
          <li>
            <a href={`${GH}/docs/security/FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md`}>
              Assumption boundary
            </a>
          </li>
          <li>
            <a href={`${GH}/docs/verification/GOLDEN-VECTOR-DISCREPANCIES.md`}>
              Golden-vector documentation
            </a>
          </li>
        </ul>
        <p className="muted">
          Observer campaign PROOF-034 remains a separate empirical archive. See{" "}
          <Link to="/status">Status</Link> and <Link to="/experiment">PROOF-034</Link>. Independent
          byte checks: <Link to="/zero-trust-verification">Zero Trust</Link>. Script surface:{" "}
          <Link to="/documentation/security/minimal-script-architecture">
            Minimal Script Architecture
          </Link>
          .
        </p>
      </section>
    </div>
  );
}
