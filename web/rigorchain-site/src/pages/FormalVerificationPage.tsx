import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import { OPUHASH } from "../protocolFacts";

const GH = "https://github.com/rigorchain/rigorchain/blob/main";
const KERNEL = `${GH}/formal_verification/rigor_kernel`;
const RELEASE = `${KERNEL}/release`;

export function FormalVerificationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">COQ CONSENSUS KERNEL</p>
        <h1>Formal Verification</h1>
        <p className="muted">
          Inside the Coq consensus kernel model, critical consensus invariants have been
          machine-checked, with explicit extraction and runtime trust boundaries.
        </p>
      </header>

      <aside className="whitepaper-banner card" role="note">
        <p>
          The path from the model to a running binary is declared, not verified. That is not a
          second publicity sentence. Maturity <span className="mono">4.9</span>. Level 5 is not
          claimed.
        </p>
        <p>
          <span className="mono">
            cpp_equivalence = false
            <br />
            refinement_proved = false
            <br />
            EXTRACTION_FORMAL_PROOF = source_identity_only
            <br />
            verified_compiler_in_current_tcb = false
            <br />
            COMPILER_CORRECTNESS = false
          </span>
        </p>
      </aside>

      <section className="card prose-section">
        <h2>Overview</h2>
        <p>
          Production consensus is the C++ node (<code>core/src/</code>). Formal verification
          publishes a Coq kernel of selected invariants after the P0-130 audit and P0-131 repair.
          Those artifacts do not replace the node.
        </p>
        <p>
          Continuous Reserve Issuance closed the issuance half of the model: flat 128 ORU per block (
          <code>h ≥ 1</code>) and a block-local RES predicate{" "}
          <code>R · (2²⁵ + h)² ≤ B ≪ 40</code>. It did not close PoW security, C++{" "}
          <code>ConnectBlock</code>, or k-deep / honest-majority fork choice.
        </p>
        <p>
          Public map:{" "}
          <a href={`${KERNEL}/README.md`}>
            <code>formal_verification/rigor_kernel/</code>
          </a>
          . Frontier:{" "}
          <a href={`${RELEASE}/FormalizationFrontier.md`}>FormalizationFrontier.md</a>. Claim
          wording: <a href={`${RELEASE}/PublicClaims.md`}>PublicClaims.md</a>.
        </p>
      </section>

      <section className="three-layer-grid">
        <article className="card layer-card">
          <h3>Layer 1 — Production consensus</h3>
          <p>
            Running C++ (<code>core/src/</code>). Code is the source of truth. RFC / SPEC / Coq
            do not supersede the node.
          </p>
        </article>
        <article className="card layer-card">
          <h3>Layer 2 — Coq kernel model</h3>
          <p>
            Closed Category-A invariants: conservation, UTXO soundness, issuance, chain
            induction, codec, classifier, most-work tip selection, maturity 100, script
            spendability by class. Properties of the model, not of <code>rigord</code>.
          </p>
        </article>
        <article className="card layer-card">
          <h3>Layer 3 — Declared runtime path</h3>
          <p>
            Extraction source identity, codec differential testing, and a CertiCoq/CompCert
            research sidecar at maturity 4.9. Testing is not proof. Compiler and{" "}
            <code>ConnectBlock</code> refinement stay false.
          </p>
        </article>
      </section>

      <section className="card prose-section">
        <h2>What is proved today</h2>
        <div className="table-wrap">
          <table className="data-table">
            <thead>
              <tr>
                <th>Layer</th>
                <th>Proved</th>
                <th>Not proved</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td>Domain models</td>
                <td>
                  Dual-supply rewind; most-work finite-tip selection on{" "}
                  <code>GetBlockProof</code>; coinbase / RES-mint depth 100; script spendability
                  by class + abstract <code>sig_ok</code>; ORU fold = n·128
                </td>
                <td>
                  Models match C++; k-deep / honest-majority (OP-R1); Script VM / Ed25519
                </td>
              </tr>
              <tr>
                <td>Unified kernel</td>
                <td>
                  <code>kernel_sound</code>; chain induction
                </td>
                <td>
                  Reorg k-deep; C++ <code>ConnectBlock</code>
                </td>
              </tr>
              <tr>
                <td>Executable / runtime</td>
                <td>
                  <code>executable_step_preserves_invariants</code>;{" "}
                  <code>machine_step_refines_executable_step</code> on a 63-bit checked-word
                  domain
                </td>
                <td>That OCaml <code>int</code> is the modelled word</td>
              </tr>
              <tr>
                <td>Extraction</td>
                <td>
                  <code>kernel_call_boundary</code> is a bijection (source identity)
                </td>
                <td>
                  That <code>ExtractedKernel.ml</code> computes the Coq function
                </td>
              </tr>
              <tr>
                <td>Checker / compiler</td>
                <td>
                  <code>abstract_checker</code> decides validity in the model
                </td>
                <td>
                  Shipped checker binary; <code>ocamlopt</code>; Level 5
                </td>
              </tr>
              <tr>
                <td>Cryptography</td>
                <td>
                  <code>H</code> is uninterpreted; non-claims are theorems
                </td>
                <td>SHA3 / Ed25519 security</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section className="card prose-section">
        <h2>Verification coverage</h2>
        <p>Closed in the kernel model (cite <code>FinalClaimsBoundary.md</code>):</p>
        <ul>
          <li>Economic conservation — no hidden creation; fees transfer; destroy removes</li>
          <li>UTXO soundness — valid tx applies; inputs consumed; no in-model double-spend</li>
          <li>
            Issuance — RES rewind / replay-equality; ORU rewind; subsidy fold{" "}
            <code>n · 128</code>
          </li>
          <li>
            Most-work tip selection — <code>GetBlockProof</code> increments; finite{" "}
            <code>select_tip</code>; not longest-chain
          </li>
          <li>
            Maturity 100 — coinbase and body RES mint; spend-side <code>MAX_MONEY</code>
          </li>
          <li>
            Script classifier range and partition; spendability by class (converge / null-data /
            nonstandard unspendable; witness / mint need abstract <code>sig_ok</code>)
          </li>
          <li>
            Byte codec — encode/decode identity and canonicity over <code>list byte</code>;
            numerics on <code>N</code>
          </li>
          <li>
            ASERT integer model — cubic stays positive and under <code>2⁶⁴</code>;{" "}
            <code>factor 65536 = 2 · factor 0</code>
          </li>
          <li>Chain induction — invariants on every intermediate state, not only the tip</li>
        </ul>
        <p>Outside the claim boundary:</p>
        <ul>
          <li>
            C++ equivalence and <code>ConnectBlock</code> refinement (
            <code>refinement_proved = false</code>)
          </li>
          <li>
            k-deep / honest-majority reorg (OP-R1). Issuance rewind is not that race
          </li>
          <li>
            ASERT 0.013% envelope vs <code>2ˣ</code> — measured, not a theorem
          </li>
          <li>
            CheckProofOfWork / nBits / genesis exemption — C++ only. Digest model of{" "}
            <code>{OPUHASH}</code> is a model property
          </li>
          <li>Script VM, Ed25519 unforgeability, wallet, networking, mempool, mining</li>
          <li>Verified compiler in the checking TCB; Level 5; CPU / hardware semantics</li>
        </ul>
      </section>

      <SectionTitle>Living evidence</SectionTitle>
      <section className="card-grid">
        <DarkCard title="75 mutants · 70 Category A">
          <p>
            67 theorems killed directly; 3 covered by checked deferral chains. Isolation harness;
            a mutant is credited only with a kill it can be shown to make.
          </p>
        </DarkCard>
        <DarkCard title="Codec differential">
          <p>
            Byte inputs vs production <code>CDataStream &gt;&gt; CBlock</code>. Three declared
            divergences: empty dummy-vin tx, trailing bytes, <code>MAX_SIZE</code>. A fourth
            undeclared hit fails the harness. Testing is not proof.
          </p>
        </DarkCard>
        <DarkCard title="Extraction source identity">
          <p>
            Six extracted names are definitionally the Coq validators.{" "}
            <code>EXTRACTION_FORMAL_PROOF = source_identity_only</code>. Not operational
            correspondence.
          </p>
        </DarkCard>
        <DarkCard title="Maturity 4.9 sidecar">
          <p>
            CertiCoq boxed-<code>Z</code> Clight for six <code>check_*</code> functions; sidecar{" "}
            <code>ccomp</code> 3.13. Correspondence theorems were not instantiated. Not{" "}
            <code>rigord</code>.
          </p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <h2>Open properties</h2>
        <p>
          Listing an item is not a proof. Full inventory:{" "}
          <a href={`${RELEASE}/OpenProperties.md`}>OpenProperties.md</a>.
        </p>
        <ul>
          <li>
            <strong>OP-R1</strong> — k-deep / common-prefix under honest majority. Most-work{" "}
            <code>select_tip</code> has no depth bound.
          </li>
          <li>
            <strong>OP-R2</strong> — implementation refinement. Coq-side{" "}
            <code>ConnectBlockObservables → KernelInput</code> projection is Closed; C++ inhabit
            is not.
          </li>
          <li>
            <strong>OP-L1 / L3 / L4</strong> — liveness, network, finality. Not formalized.
          </li>
          <li>Compiler correctness, binary semantic equivalence, crypto security.</li>
        </ul>
      </section>

      <section className="card prose-section">
        <h2>Reproducibility</h2>
        <p>Independent reviewers can replay:</p>
        <ul>
          <li>
            <strong>Kernel Coq</strong> —{" "}
            <code>formal_verification/rigor_kernel/run_kernel_critical.sh</code>
          </li>
          <li>
            <strong>Differential checkers</strong> —{" "}
            <code>formal_verification/rigor_kernel/scripts/diff_verify.sh</code> (
            <code>MATCH</code> is fragment agreement, not C++ refinement)
          </li>
          <li>
            <strong>Auditor replay</strong> —{" "}
            <code>formal_verification/rigor_kernel/release/independent_audit_replay.sh</code>
          </li>
        </ul>
        <pre className="math-block">{`cpp_equivalence = false
refinement_proved = false
EXTRACTION_FORMAL_PROOF = source_identity_only
verified_compiler_in_current_tcb = false
COMPILER_CORRECTNESS = false`}</pre>
      </section>

      <section className="card prose-section">
        <h2>Source documents</h2>
        <ul>
          <li>
            <a href={`${RELEASE}/FormalizationFrontier.md`}>Formalization Frontier</a> — proved /
            not proved, issuance re-read, stopping criteria
          </li>
          <li>
            <a href={`${RELEASE}/FinalClaimsBoundary.md`}>Final Claims Boundary</a> — allowed and
            forbidden public sentences
          </li>
          <li>
            <a href={`${RELEASE}/PublicClaims.md`}>Public Claims</a> — auditor wording
          </li>
          <li>
            <a href={`${RELEASE}/OpenProperties.md`}>Open Properties</a> — OP-R1 and remaining
            HIGH items
          </li>
          <li>
            <a href={`${KERNEL}/README.md`}>RigorKernel README</a> — build surface
          </li>
        </ul>
        <p className="muted">
          Formal verification proves properties of the Coq consensus kernel. It does not claim
          C++ equivalence. Script surface:{" "}
          <Link to="/documentation/security/minimal-script-architecture">
            Minimal Script Architecture
          </Link>
          . Issuance: <Link to="/documentation/monetary-policy">monetary policy</Link>.
        </p>
      </section>
    </div>
  );
}
