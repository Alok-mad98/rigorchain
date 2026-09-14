import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const GH = "https://github.com/rigorchain/rigorchain/blob/main";

export function MinimalScriptArchitecturePage() {
  return (
    <div className="page whitepaper-page">
      <SubNav />

      <nav className="whitepaper-toc card" aria-label="Documentation path">
        <p className="muted">
          <Link to="/documentation">Documentation</Link>
          {" · "}
          <Link to="/documentation/security">Security Architecture</Link>
          {" · "}
          <span>Minimal Script Architecture</span>
        </p>
      </nav>

      <aside className="whitepaper-banner card" role="note">
        <p>
          Architecture documentation. No consensus rules are changed by this page.
          <br />
          <span className="mono">
            cpp_equivalence = false · refinement_proved = false · maturity 4.9
          </span>
        </p>
      </aside>

      <header className="page-header">
        <p className="tag">SECURITY ARCHITECTURE</p>
        <h1>Minimal Script Architecture</h1>
        <p className="muted">
          Bitcoin UTXO foundation. Native dual assets. Restricted scripts. Maximum verifiability.
        </p>
      </header>

      <section className="card-grid whitepaper-links">
        <DarkCard title="Whitepaper Chapter 3" to="/documentation/whitepaper" linkLabel="Whitepaper">
          <p>This chapter is bound into the official protocol whitepaper.</p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Kernel status"
        >
          <p>
            Classifier range, partition, and spendability by class are proved in the Coq kernel.
            Not a VM and not C++ refinement.
          </p>
        </DarkCard>
      </section>

      <article className="whitepaper-article">
        <section className="card prose-section whitepaper-section">
          <h2>1. Why minimal script architecture</h2>
          <p>RIGOR does not attempt to compete with programmable blockchain platforms.</p>
          <p>
            RIGOR is a Bitcoin-derived UTXO architecture, a native dual-asset system, a restricted
            script environment, and a deterministic validation system. It is not a general-purpose
            programmable blockchain, a smart contract platform, an EVM-compatible chain, a Taproot
            extension, a token layer, a colored coin system, or an inscription/Rune system.
          </p>
          <p>
            Instead, RIGOR focuses on <strong>monetary integrity</strong>,{" "}
            <strong>deterministic consensus</strong>, <strong>verifiable asset ownership</strong>,
            and a <strong>minimized attack surface</strong>.
          </p>
          <p>
            A blockchain securing monetary value does not necessarily require unlimited
            programmability. More code paths create more possible failures. A smaller execution
            surface creates stronger verification capability.
          </p>
          <p>
            The goal is not maximum programmability. The goal is maximum verifiability.
          </p>
          <p>
            RIGOR minimizes script complexity rather than eliminating scripts. The protocol
            intentionally restricts execution semantics to preserve deterministic verification and
            reduce consensus attack surface.
          </p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>2. Bitcoin UTXO foundation</h2>
          <p>RIGOR is based on the Bitcoin UTXO model.</p>
          <pre className="math-block">{`TxOut = (nValue, scriptPubKey)`}</pre>
          <h3>Explicit ownership</h3>
          <p>Each asset exists as an unspent transaction output.</p>
          <pre className="pipeline-diagram">{`Previous UTXO
      |
      |
Transaction Validation
      |
      |
New UTXO Owner`}</pre>
          <p>No hidden balances. No global mutable account state.</p>
          <h3>Deterministic validation</h3>
          <p>
            Every node independently verifies transaction inputs, outputs, signatures, asset rules,
            and consensus conditions. The same input must produce the same validation result.
          </p>
          <h3>Proven security model</h3>
          <p>
            Bitcoin’s UTXO architecture has operated at global scale for many years. RIGOR
            preserves this proven foundation while reducing unnecessary complexity.
          </p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>3. Script defines asset identity</h2>
          <p>
            In RIGOR, asset identity is not stored as external metadata. The script structure
            itself defines the asset domain.
          </p>
          <pre className="pipeline-diagram">{`Script Structure
        |
        |
Asset Classification
        |
        |
Validation Rules`}</pre>
          <p>
            This avoids hidden asset metadata, external indexing dependency, and inconsistent
            interpretation. Script is the compliance layer. Classification is inferred from{" "}
            <code>scriptPubKey</code>. Legitimacy still requires conservation and mint rules.
          </p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>4. Dual native asset architecture</h2>
          <p>
            ORU and RES are not colored coins, Bitcoin inscriptions, Ordinals, Runes, smart
            contract tokens, wrapped assets, or external accounting layers.
          </p>
          <p>
            They are <strong>native UTXO assets</strong>. Both are first-class protocol-native
            assets.
          </p>
          <blockquote>
            ORU and RES are two native asset identities inside the RIGOR UTXO system. Neither asset
            is a secondary representation of another asset.
          </blockquote>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Asset</th>
                  <th>Role</th>
                  <th>Characteristics</th>
                  <th>Script</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>ORU</td>
                  <td>Medium of Exchange</td>
                  <td>Transactional asset, mining reward, network utility, liquidity layer</td>
                  <td>Spendable Witness v0 P2WPKH; Convergence destroy is &lt;HASH160&gt; 0xbd (not a UTXO)</td>
                </tr>
                <tr>
                  <td>RES</td>
                  <td>Store of Value</td>
                  <td>
                    Scarcity asset, cap 2²² RES, created only by a body-tx destroy at destroy_cost(h)
                  </td>
                  <td>Witness v1 32-byte Ed25519; mint is same-tx with OP_CONVERGE</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>Both exist at the same protocol layer. There is no parent-child asset relationship.</p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>5. Script restrictions and attack surface reduction</h2>
          <p>RIGOR intentionally limits script functionality.</p>
          <p>
            RIGOR does <strong>not</strong> provide a general-purpose VM, arbitrary smart contracts,
            a programmable execution layer, Turing-complete execution, or unrestricted programmable
            state.
          </p>
          <p>
            RIGOR <strong>does</strong> provide constrained transaction validation, asset-specific
            script rules, and deterministic verification logic.
          </p>
          <p>
            RIGOR minimizes script complexity rather than eliminating scripts. The protocol
            intentionally restricts execution semantics to preserve deterministic verification and
            reduce consensus attack surface.
          </p>
          <p>
            Reduced: execution complexity, consensus bugs, unexpected state transitions,
            verification burden.
          </p>
          <blockquote>
            Every additional execution capability creates additional consensus responsibility.
          </blockquote>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>6. RIGOR script model</h2>
          <p>Scripts exist. A general-purpose VM does not.</p>
          <p>
            RIGOR minimizes script complexity rather than eliminating scripts. The protocol
            intentionally restricts execution semantics to preserve deterministic verification and
            reduce consensus attack surface.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Execution surface</th>
                  <th>Role</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Witness v0 / 20-byte</td>
                  <td>ORU P2WPKH; Ed25519 spend via VerifyRigorPredicate</td>
                </tr>
                <tr>
                  <td>Witness v1 / 32-byte</td>
                  <td>Native RES; not BIP341 Taproot; not CVG2 / RES2</td>
                </tr>
                <tr>
                  <td>Convergence 0xbd</td>
                  <td>
                    &lt;HASH160&gt; 0xbd; ORU destroy; not a UTXO. Same body tx MAY mint Witness v1 RES
                    when R · (2²⁵ + h)² ≤ B ≪ 40
                  </td>
                </tr>
                <tr>
                  <td>Coinbase mint 0xbb</td>
                  <td>Retired. Not the live issuance path</td>
                </tr>
                <tr>
                  <td>NULL_DATA</td>
                  <td>0x6a + pushes; nValue = 0</td>
                </tr>
                <tr>
                  <td>Not creatable</td>
                  <td>P2WSH, P2PKH, P2SH, Taproot, secp256k1 CHECKSIG</td>
                </tr>
              </tbody>
            </table>
          </div>
          <SectionTitle>No Script VM</SectionTitle>
          <p>
            There is no <code>EvalScript</code> and no <code>interpreter.cpp</code>. Bytes are
            classified by <code>ClassifyRigorTxout</code>; spends go through{" "}
            <code>VerifyRigorPredicate</code>. Witness is a two-item stack:{" "}
            <code>&lt;sig64&gt; &lt;pk32&gt;</code>.
          </p>
          <SectionTitle>Witness v0</SectionTitle>
          <p>
            Spendable ORU is Witness v0 with a 20-byte HASH160 program. A 32-byte Witness v0
            program (P2WSH) is <code>NONSTANDARD</code> / <code>ASSET_INVALID</code> and is not
            creatable.
          </p>
          <SectionTitle>Witness v1</SectionTitle>
          <p>
            Native 32-byte Witness v1 programs are RES. New RES is created only in the same body
            transaction as an <code>OP_CONVERGE</code> destroy. Convergence is not a witness program;
            it is <code>&lt;HASH160&gt; 0xbd</code>.
          </p>
          <p>
            <strong>Witness v1 is not Bitcoin Taproot.</strong> RIGOR does not inherit BIP341
            Taproot execution semantics. No control blocks, no annex, no tapscript.
          </p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>7. Convergence script architecture</h2>
          <p>
            Convergence is not a smart contract and not a selection among candidates. It is a
            deterministic destroy-to-mint rule at the confirming height.
          </p>
          <pre className="pipeline-diagram">{`ORU destroy  <HASH160> 0xbd  (amount B)
        ↓
same body transaction
        ↓
Witness v1 RES outputs (sum R)
        if R = 0  or  R · (2²⁵ + h)² ≤ B << 40`}</pre>
          <p>
            <code>h</code> is the confirming height. Display <code>destroy_cost(h)</code> is not a
            consensus argument. The 0xbd output does not enter the UTXO set. The kernel proves{" "}
            <code>converge_unspendable</code>, <code>mint_not_converge</code>, and{" "}
            <code>ladder_order_immaterial</code>: mint, converge, and null-data first bytes are
            disjoint, so classifier order does not choose the class.
          </p>
          <p>
            Coinbase <code>0xbb</code> does not mint RES. A miner cannot issue RES. Mint-output
            maturity 100 matches coinbase depth.
          </p>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>8. Why RIGOR rejects complex script systems</h2>
          <p>
            Complex scripting creates a larger audit scope, more edge cases, and more formal
            verification difficulty.
          </p>
          <pre className="pipeline-diagram">{`Less capability


       ↓


Less complexity


       ↓


Smaller attack surface


       ↓


Stronger mathematical verification`}</pre>
        </section>

        <section className="card prose-section whitepaper-section">
          <h2>9. Formal verification relationship</h2>
          <p>
            Formal verification becomes more realistic when state transitions are limited,
            execution paths are restricted, and rules are deterministic.
          </p>
          <p>
            RIGOR&apos;s minimal script design is not a limitation. It is a prerequisite for
            stronger mathematical assurance.
          </p>
          <p>
            The Coq kernel proves classifier totality and exact range, pairwise disjoint mint /
            converge / null-data shapes, and spendability by class: converge, null-data, and
            nonstandard are unspendable; witness v0 / v1 and mint require abstract{" "}
            <code>sig_ok</code>. That is not a Script VM and not Ed25519 unforgeability.
          </p>
          <pre className="math-block">{`cpp_equivalence = false
refinement_proved = false
EXTRACTION_FORMAL_PROOF = source_identity_only`}</pre>
          <p>
            Source chapter: protocol whitepaper Chapter 3. GitHub:{" "}
            <a href={`${GH}/docs/security/minimal-script-architecture.md`}>
              docs/security/minimal-script-architecture.md
            </a>
            . Status: <Link to="/documentation/formal-verification">Formal Verification</Link>.
          </p>
        </section>
      </article>
    </div>
  );
}
