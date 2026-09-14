import { Link } from "react-router-dom";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import {
  ASERT_ANCHOR_HEIGHT,
  ASERT_CLAMP,
  ASERT_HALF_LIFE_S,
  MAINNET_GENESIS,
  MAINNET_HRP,
  MAINNET_P2P_PORT,
  MAINNET_RPC_PORT,
  OPUHASH,
  TARGET_SPACING_S,
} from "../protocolFacts";

const TOC = [
  { id: "abstract", label: "Abstract" },
  { id: "security-budget", label: "1. Why Two Assets Must Exist" },
  { id: "dual-asset", label: "2. Rules and Parameters" },
  { id: "minimal-script", label: "3. Native UTXO Identities" },
  { id: "convergence", label: "4. The One-Way Bridge" },
  { id: "issuance", label: "5. Checkable Mathematics" },
  { id: "mining", label: "6. Shared Height, Paid in ORU" },
  { id: "self-proving", label: "7. Recompute Locally" },
  { id: "formal", label: "8. The Formal Boundary" },
  { id: "product-surfaces", label: "9. Product Surfaces" },
  { id: "appendix", label: "Appendix" },
] as const;

export function WhitepaperPage() {
  return (
    <div className="page whitepaper-page">
      <SubNav />

      <aside className="whitepaper-banner card" role="note">
        <p>
          This document explains production Continuous Reserve Issuance and the Coq consensus kernel. Production
          consensus is <code>core/src/</code>.
        </p>
        <p>
          Production consensus is <code>core/src/</code>. Maturity{" "}
          <span className="mono">4.9</span>. Level 5 is not claimed. The one public
          sentence is in Chapter 8.
        </p>
        <p>
          <span className="mono">
            cpp_equivalence = false
            <br />
            refinement_proved = false
            <br />
            EXTRACTION_FORMAL_PROOF = source_identity_only
          </span>
        </p>
      </aside>

      <header className="page-header">
        <p className="tag">OFFICIAL WHITEPAPER</p>
        <h1>RIGOR Protocol Whitepaper</h1>
        <p className="muted">
          A dual-asset proof-of-work protocol. RES mints at <code>destroy_cost(h)</code>. Formal results
          are properties of the kernel model.
        </p>
      </header>

      <nav className="whitepaper-toc card" aria-label="Whitepaper sections">
        <SectionTitle>Contents</SectionTitle>
        <ol className="whitepaper-toc-list">
          {TOC.map((item) => (
            <li key={item.id}>
              <a href={`#${item.id}`}>{item.label}</a>
            </li>
          ))}
        </ol>
      </nav>

      <article className="whitepaper-article">
        <section id="abstract" className="card prose-section whitepaper-section">
          <h2>Abstract</h2>
          <p>
            RIGOR exists to exit a structural defect of single-asset proof-of-work chains. Those
            chains ask one native asset to circulate as a medium of exchange, remain scarce as a
            store of value, and pay miners for security. The jobs conflict. A single finite-supply
            ticker cannot occupy all three vertices. That tension is the security-budget problem.
            The dual-asset split is not an optional optimisation. It is the existence reason for
            the protocol.
          </p>
          <p>
            <strong>RIGOR</strong> separates the jobs at consensus. <strong>ORU</strong> is the
            utility asset: fees, miner subsidy, and Convergence destroy, with a flat subsidy of{" "}
            <strong>128 ORU</strong> per block after genesis. <strong>RES</strong> is the scarce
            asset: maximum supply <strong>4,194,304</strong> (<code>2²²</code> RES). New RES appears
            only in Convergence: a body transaction destroys ORU and may mint RES when it satisfies{" "}
            <code>destroy_cost(h)</code> — not from mining subsidy, governance, or a 1:1 swap.
          </p>
          <p>
            Convergence destroys ORU via <code>OP_CONVERGE</code> and may mint Witness v1 RES in the
            same body transaction when <code>R · (2²⁵ + h)² ≤ B ≪ 40</code>. <code>R = 0</code> is a
            legal plain destroy. Fairness is that published integer inequality. Genesis cost is 1024
            ORU destroyed to mint 1 RES.
            Epoch index and <code>RATE(k)</code> are not mint arguments.
          </p>
          <p>
            The Coq kernel proves selected consensus invariants, including issuance rewind and the
            ORU closed form <code>n · 128</code>. It does not prove C++{" "}
            <code>ConnectBlock</code> or k-deep / honest-majority fork choice.
          </p>
          <pre className="pipeline-diagram">{`RIGOR =
    2 Native Assets (ORU, RES)
  + Closed monetary script templates (v0 ORU, v1 RES, mint, converge)
  + UTXO Local Validation
  + Ed25519 Authentication
  + OpuHash (SHA3-256 / RGRPOW01) + ASERT PoW
  + Continuous Reserve Issuance
  + Deflationary Fee Destroy
  + Coq consensus kernel (maturity 4.9)
  + No Global Asset State

No Script VM. Classification then VerifyRigorPredicate.
cpp_equivalence = false
refinement_proved = false`}</pre>
        </section>

        <section id="security-budget" className="card prose-section whitepaper-section">
          <h2>1. Why Two Assets Must Exist</h2>
          <p>
            A public proof-of-work chain must do three jobs at once. A typical single-asset design,
            especially one with a declared finite supply, asks one native unit to perform all three.
            The users do not share a time horizon, a fee tolerance, or a reason to transact.
          </p>
          <pre className="pipeline-diagram">{`Medium of Exchange
        +
Store of Value
        +
Security Incentive`}</pre>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Role</th>
                  <th>Time horizon</th>
                  <th>What they want</th>
                  <th>What they contribute to miner payment</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Medium of exchange</td>
                  <td>Minutes to hours</td>
                  <td>Cheap, frequent transfers</td>
                  <td>Fees — and they resist high fees</td>
                </tr>
                <tr>
                  <td>Store of value</td>
                  <td>Years</td>
                  <td>Scarcity and permanent protection</td>
                  <td>Almost no transactions, so almost no fees</td>
                </tr>
                <tr>
                  <td>Miners</td>
                  <td>Every block, forever</td>
                  <td>Predictable payment for hashpower</td>
                  <td>Protection of the entire history, not only the current block</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            A declared hard cap does not merely limit issuance. It selects a store-of-value
            narrative. Rational holders then prefer to accumulate rather than spend. Velocity
            falls. The fee market that was supposed to replace the subsidy never appears at the
            scale the security budget requires.
          </p>
          <p>
            Bitcoin’s two readings fail for complementary reasons. As a medium of exchange, fixed
            block space makes success raise fees; larger or faster blocks buy cheaper inclusion at
            the cost of security and decentralisation. As a store of value, a successful holding
            culture is exactly a culture that does not transact — so the post-subsidy fee hypothesis
            remains a hope, not a protocol rule. The large-block path does not escape the
            finite-supply constraint: extra block space stays empty, and the security budget is
            still unpaid.
          </p>
          <p>
            The category error is to fund a flow — continuous protection of history — with a spot
            payment for inclusion. Inflation-based block reward is incentive-compatible for
            store-of-value holders, who pay in proportion to duration. Placed on the same unit that
            is supposed to be the reserve, it destroys the reserve story. Placed on no unit, miners
            go unpaid after the subsidy. RIGOR assigns the vertices to two assets and couples them.
          </p>
          <pre className="pipeline-diagram">{`work                      →  ORU subsidy (128 / block)
RES demand at the ceiling →  ORU destroy          (RFC-020)
RES transfers             →  ORU fee destroy      (RFC-018)
ρ                         →  realised RES stock and residual ORU inflation`}</pre>
          <p>
            ORU takes circulation and miner payment, with a flat 128 ORU tail and no declared cap.
            Hardness is endogenous in <code>ρ</code> — the share of emitted ORU ever destroyed to
            create RES. RES takes scarcity: it is not mined; it is created only by destroying ORU
            at a strictly rising <code>destroy_cost(h)</code>. The ceiling <code>2²²</code> RES is a
            closed-form integral, not a declared 21-million-style parameter, and needs no consensus
            counter. Hoarding is isolated on RES. The miner budget no longer depends on
            store-of-value holders transacting.
          </p>
          <p>
            ORU alone is a work-payment asset with no scarce stock. RES alone is a capped stock with
            no way to pay hashpower. The pair is not an optional optimisation. It is the exit from
            the defect.
          </p>
        </section>

        <section id="dual-asset" className="card prose-section whitepaper-section">
          <h2>2. The Rules That Implement the Split</h2>
          <p>
            Chapter 1 assigned the three vertices to two assets. This chapter writes that split as
            frozen rules. The protocol fixes a transform, not a price and not a participation rate.
            Observe balances in the <Link to="/wallet">wallet</Link>.
          </p>
          <pre className="pipeline-diagram">{`work → ORU
ORU destroy → (may) RES
RES → ORU    illegal`}</pre>
          <p>
            Proof-of-work discovers blocks and gives irreversible height order. It does not create
            RES. Users mine ORU, destroy ORU, and may mint RES in the same body transaction. That
            sentence must not be shortened to “mine RES”. <code>destroy_cost(h)</code> is a protocol
            destroy requirement, not a market quote.
          </p>
          <h3>ORU — the flow</h3>
          <p>
            <code>subsidy(0) = 0</code>. For every <code>h ≥ 1</code>: <code>128</code> ORU. No
            halvings. No declared cap. Circulating stock updates as{" "}
            <code>S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)</code>. Residual inflation is{" "}
            <code>1 − ρ</code>. Under <code>ρ = 100%</code>, circulating ORU is exactly constant.
            That is why ORU does not get a hard cap. The Coq model proves the fold identity{" "}
            <code>oru_subsidy_fold_closed</code>: heights <code>0..n</code> sum to{" "}
            <code>n · 128</code>.
          </p>
          <h3>RES — the stock</h3>
          <p>
            RES is not mined and not convertible back. A body transaction that destroys ORU{" "}
            <code>B</code> MAY mint Witness v1 RES <code>R</code> when{" "}
            <code>R · (2²⁵ + h)² ≤ B ≪ 40</code>. Display form:{" "}
            <code>destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰</code>. Genesis cost is 1024 ORU per
            RES. The cost is strictly increasing, so an earlier mint pays less per RES for a longer
            remaining span of protection. Coinbase <code>0xbb</code> is not this path.
          </p>
          <pre className="math-block">{`RES_MAX_SUPPLY = (S / P0) · H = 2⁷ / 2¹⁰ · 2²⁵ = 2²² = 4 194 304 RES
S_RES(h) ≤ 419430400000000
cumulative(h) = cap · h / (h + 2²⁵)`}</pre>
          <p>
            The ceiling is a closed-form integral, not a declared 21-million-style parameter. It
            needs no consensus counter: delaying a destroy yields strictly less RES. Realised stock
            is <code>ρ · 2²²</code>. Even at <code>ρ = 100%</code>, year 1 issues about 6 564 RES.
            The cap is a hard bound, not a promise that it will be filled.
          </p>
          <p>
            RFC-018 is the second sink: half of the ORU fee on a RES transfer is unclaimable. Four
            ORU quantities must stay separate — ledger supply, circulating UTXO, Convergence
            destroy, and fee destroy. The rules fix the transform. Whether demand arrives is not a
            rule.
          </p>
        </section>

        <section id="minimal-script" className="card prose-section whitepaper-section">
          <h2>3. Native UTXO Identities</h2>
          <p>
            Chapter 2 wrote the rules. This chapter states how the two identities exist as ledger
            objects. The protocol fixes objects and classification, not an off-chain registry and
            not a colour on a single UTXO. If ORU and RES were only annotations, Chapter 1’s split
            would not have happened. Engineering audit:{" "}
            <Link to="/documentation/security/minimal-script-architecture">
              Minimal Script Architecture
            </Link>
            .
          </p>
          <pre className="pipeline-diagram">{`TxOut = (nValue, scriptPubKey)
scriptPubKey shape  →  ORU | RES | NONE | INVALID
consensus predicate →  born legally, and who may spend`}</pre>
          <p>
            There is no account model as consensus truth and no parallel <code>asset</code> field.
            Classification is <code>AssetTypeFromScriptPubKey</code>. Script defines identity;
            consensus defines legitimacy. Parsing Witness v1 is not a legal mint. Birth of RES
            still requires <code>OP_CONVERGE</code> in the same transaction and the RFC-020
            inequality.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Shape</th>
                  <th>Asset</th>
                  <th>Spendable</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Witness v0, 20-byte</td>
                  <td>ORU</td>
                  <td>Yes</td>
                </tr>
                <tr>
                  <td>Witness v1, 32-byte</td>
                  <td>RES</td>
                  <td>Yes</td>
                </tr>
                <tr>
                  <td>
                    <code>&lt;HASH160&gt; 0xbd</code>
                  </td>
                  <td>ORU (destroy)</td>
                  <td>No — not a UTXO</td>
                </tr>
                <tr>
                  <td>Zero-value <code>0x6a</code></td>
                  <td>NONE</td>
                  <td>No</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            Production has <strong>no Script VM</strong>. Bytes are classified; spends go through{" "}
            <code>VerifyRigorPredicate</code>. Conservation is per asset pool: ORU inputs cannot
            fund RES outputs. The only one-way exception is destroy-to-mint. The new RES is a new
            UTXO, not a colour on the old output. P2PKH, P2SH, P2WSH, Taproot, and secp256k1 spends
            are consensus-invalid. <strong>Witness v1 is not Bitcoin Taproot.</strong>
          </p>
          <p>
            ORU and RES are not colored coins, inscriptions, Ordinals, Runes, contract units, or
            wrapped assets. The split happens in consensus, not in the explorer. Identity lives in
            the script. Legitimacy lives in consensus.
          </p>
        </section>

        <section id="convergence" className="card prose-section whitepaper-section">
          <h2>4. Convergence: The One-Way Bridge</h2>
          <p>
            Without a public, deterministic bridge the two native identities are unrelated ledger
            objects. Convergence is that bridge. The protocol fixes one visible destroy and an
            integer predicate in the same transaction. It does not fix that anyone will destroy.
            Observe destroys and mints in the <Link to="/wallet/convergence">wallet</Link> and on
            the <Link to="/explorer">Explorer</Link>.
          </p>
          <pre className="math-block">{`ORU inputs
        ↓
exactly one OP_CONVERGE (B), not a UTXO
        ↓
Witness v1 RES outputs sum R, same body transaction
        ↓
R > 0  →  R · (2²⁵ + h)² ≤ B << 40`}</pre>
          <p>
            Destroy is an explicit <code>&lt;HASH160&gt; 0xbd</code> output, not a remainder inferred
            from fees. Mint lives in the block body, not coinbase. A miner who includes the
            transaction receives no RES. <code>R = 0</code> is a legal plain destroy. No
            Convergence → no new RES.
          </p>
          <p>
            A 1:1 swap would rename flow into stock and collapse the Chapter 1 triangle. A draw
            would hand “who receives RES” to a hash. Convergence has no random term: same{" "}
            <code>R</code>, <code>B</code>, <code>h</code> ⇒ same admit-or-reject.{" "}
            <code>destroy_cost(h)</code> is a protocol destroy requirement, not a market quote.
            Underpayment invalidates the whole transaction. Mint maturity 100 is reorg protection,
            not a wait for a draw.
          </p>
        </section>

        <section id="issuance" className="card prose-section whitepaper-section">
          <h2>5. Checkable Mathematics</h2>
          <p>
            Fairness is not a draw. It is a public decision function: same <code>(R, B, h)</code> ⇒
            same admit or reject. No hidden rate. No random term. Anyone who re-evaluates a
            confirmed transaction must get the same yes or no as the network.
          </p>
          <pre className="math-block">{`destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰     display only
R · 1024 · (2²⁵+h)²  ≤  B · 2⁵⁰
R · (2²⁵+h)²         ≤  B · 2⁴⁰
RES_DESTROY_SHIFT = 50 − log₂(1024) = 40
∫ S / destroy_cost(h) dh = (S / P0) · H = 2²²
cumulative(h) = 2²² · h / (h + 2²⁵)`}</pre>
          <p>
            The shift is forced by the genesis cost. Change that cost and leave the shift, and the
            predicate no longer expresses the curve. The exponent 2 makes the integral exact, with
            no residual, so the cap lands on a power of two. Checking the cap is checking
            exponents. Because cost is strictly increasing, delay yields less RES: the ceiling
            needs no consensus counter. Realised stock is <code>ρ · 2²²</code>.
          </p>
          <p>
            <code>128 = 2⁷</code> is the numerator of that identity, not an independent lucky
            number. Earlier minters pay a lower unit cost — duration-proportional payment, not a
            hole. Later arrivals face the same public curve. Detail page:{" "}
            <Link to="/documentation/monetary-policy">Continuous Reserve Issuance</Link>.
          </p>
        </section>

        <section id="mining" className="card prose-section whitepaper-section">
          <h2>6. Proof-of-Work: Shared Height, Paid in ORU</h2>
          <p>
            Chapter 5’s predicate contains <code>h</code>. If nodes disagree on height, the same
            destroy is judged differently. Proof-of-work supplies an order that is not cheap to
            rewrite. Mining pays ORU. Mining does not mint RES. Product guide:{" "}
            <Link to="/mining">Mining</Link>.
          </p>
          <pre className="pipeline-diagram">{`OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid  ⇔  OpuHash ≤ target

hashpower → shared height h
        ↓
predicate at h; coinbase 128 ORU at h
RES only in a body Convergence tx`}</pre>
          <p>
            Genesis subsidy is 0. Every later block pays 128 ORU to the Witness v0 coinbase script.
            Block identity remains SHA256d of the header. Coinbase states this block’s height and
            pays ORU only. Pool policy cannot allocate RES. Changing a payout address cannot create RES.
            Finding a header does not legalise an underpaid mint.
          </p>
          <p>
            ASERT (spacing {TARGET_SPACING_S} s, half-life {ASERT_HALF_LIFE_S} s, {ASERT_CLAMP}{" "}
            clamp) lets <code>h</code> advance with wall time. Difficulty is not mint cost and does
            not enter the predicate. The kernel models most-work finite-tip selection on{" "}
            <code>GetBlockProof</code>. It does not prove <code>ActivateBestChain</code>. Validity
            is not a pool dashboard — Chapter 7.
          </p>
        </section>

        <section id="self-proving" className="card prose-section whitepaper-section">
          <h2>7. Recompute Locally</h2>
          <p>
            A pool, explorer, wallet, or website may display a conclusion. None of them may be that
            conclusion. The protocol does not claim that cheating is impossible. It claims that
            unequal bytes fail the published checks.
          </p>
          <pre className="pipeline-diagram">{`public data → independent calculation → compare → accept / reject

body tx → OP_CONVERGE B → Witness v1 sum R
        → R > 0 → R · (2²⁵ + h)² ≤ B << 40
        → coinbase (this block’s height; ORU only)
        → merkle root → header
        → OpuHash = SHA3-256("RGRPOW01" || header_80)`}</pre>
          <p>
            Validity is equality of commitment and recomputation, not an explorer flag. A
            fingerprint over public fields is observational: it cannot accept an invalid block, and
            a match is not a substitute for a full node. Browser Zero Trust recomputes selected
            fields; that is not <code>ConnectBlock</code>. A conclusion may be displayed. Validity
            must be recomputed.
          </p>
        </section>

        <section id="formal" className="card prose-section whitepaper-section">
          <h2>8. The Formal Boundary</h2>
          <p>
            Snapshot (not a protocol parameter): Coq 8.15.0; 254 classified theorems (Category A
            119, B 102, C 33); maturity 4.9; mutation isolation list 127. Living inventory:{" "}
            <Link to="/documentation/formal-verification">Formal Verification</Link>.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Layer</th>
                  <th>What it is</th>
                  <th>What it is not</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Running consensus</td>
                  <td>
                    C++ node (<code>core/src/</code>) realization of RFC-020 / RFC-018.
                  </td>
                  <td>RFC / SPEC / Coq rewriting running rules</td>
                </tr>
                <tr>
                  <td>Coq kernel model</td>
                  <td>
                    Machine-checked properties of the model: conservation, issuance rewind,{" "}
                    <code>n · 128</code>, mint predicate, derived ceiling, classifier, most-work
                    finite-tip, maturity 100
                  </td>
                  <td>
                    A C++ refinement proof; <code>ConnectBlock</code>; k-deep / honest-majority
                  </td>
                </tr>
                <tr>
                  <td>Differential / extraction</td>
                  <td>Finite golden vectors; declared runtime path</td>
                  <td>Universal equivalence; verified compiler in the current TCB</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            The only public sentence: inside the Coq consensus kernel model, critical consensus
            invariants have been machine-checked, with explicit extraction and runtime trust
            boundaries. Yes: selected invariants inside this Coq model hold — kernel soundness,
            ORU conservation, the RES ceiling, classifier partition, chain induction.{" "}
            <code>Print Assumptions</code> is Closed on the public classification. Not yes: the
            running C++ node equals the model.
          </p>
          <p>
            What is proved is the model. What runs is C++. A display remains a display — Chapter 9.
          </p>
        </section>

        <section id="product-surfaces" className="card prose-section whitepaper-section">
          <h2>9. Product Surfaces</h2>
          <p>
            A surface may display a conclusion. It may not be that conclusion. A green Chapter 8 is
            not “the chain has been proved.” Product completeness is not consensus completeness.
            Sight is a condition of use. Sight is not decision.
          </p>
          <pre className="pipeline-diagram">{`rules accepted or rejected in the node
        ↓
a surface displays public facts
        ≠
the surface becomes a source of validity
        ≠
a formal-verification page becomes an equivalence proof`}</pre>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Surface</th>
                  <th>It may</th>
                  <th>It may not</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Wallet / pool / explorer</td>
                  <td>Show balances, shares, height, fingerprints</td>
                  <td>Be a validity oracle; allocate RES; replace recomputation</td>
                </tr>
                <tr>
                  <td>
                    <Link to="/zero-trust-verification">Zero Trust</Link>
                  </td>
                  <td>Recompute selected fields in the browser</td>
                  <td>
                    Replace <code>ConnectBlock</code> or a full node
                  </td>
                </tr>
                <tr>
                  <td>
                    <Link to="/documentation/formal-verification">Formal Verification</Link>
                  </td>
                  <td>Publish what is proved and what is not</td>
                  <td>
                    Write <code>equivalence_claimed = true</code>
                  </td>
                </tr>
                <tr>
                  <td>Website / this paper</td>
                  <td>Entry, explanation, bind the argument</td>
                  <td>
                    Consensus authority; rewrite <code>core/src/</code>
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            Formal documentation is the most dangerous surface. What it says is true: the model
            contains machine-checked invariants. Product copy may not supply the unwritten
            half-sentence “so the chain has been proved.” Do not drop the word “model” from a
            Coq-green badge, write finite vectors as “C++ equals Coq,” or write “do not trust the
            pool” as “do not run a node.” Documentation work is not a consensus change. No
            investment yield is promised. No date is given for filling the derived RES ceiling.
          </p>
          <p>
            A display remains a display. The rules remain in the node.
          </p>
        </section>

        <section id="appendix" className="card prose-section whitepaper-section">
          <h2>Appendix</h2>
          <p>
            Chapters 1–9 are the public argument. This appendix is lookup. It does not add a
            claim. Code SSOT: <code>core/src/</code>.
          </p>
          <h3>Frozen public parameters</h3>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Symbol</th>
                  <th>Value</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>ORU subsidy</td>
                  <td>
                    <code>R(0) = 0</code>; then 128 ORU forever
                  </td>
                </tr>
                <tr>
                  <td>RES mint</td>
                  <td>
                    Body-tx destroy-to-mint; <code>R · (2²⁵ + h)² ≤ B ≪ 40</code>
                  </td>
                </tr>
                <tr>
                  <td>Display form</td>
                  <td>
                    <code>destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰</code>
                  </td>
                </tr>
                <tr>
                  <td>Pole / shift</td>
                  <td>
                    <code>2²⁵</code> ; <code>40 = 50 − log₂(1024)</code>
                  </td>
                </tr>
                <tr>
                  <td>Derived ceiling</td>
                  <td>2²² RES (4,194,304)</td>
                </tr>
                <tr>
                  <td>Mint maturity</td>
                  <td>100 confirmations (same as coinbase)</td>
                </tr>
                <tr>
                  <td>RFC-018</td>
                  <td>Half of RES-transfer ORU fees unclaimable</td>
                </tr>
                <tr>
                  <td>Scripts</td>
                  <td>v0 / 20 bytes = ORU; v1 / 32 bytes = RES (not Taproot); HASH160 + 0xbd = Convergence</td>
                </tr>
                <tr>
                  <td>PoW</td>
                  <td>{OPUHASH}</td>
                </tr>
                <tr>
                  <td>Block identity</td>
                  <td>SHA256d of the same 80-byte header</td>
                </tr>
                <tr>
                  <td>ASERT</td>
                  <td>
                    spacing {TARGET_SPACING_S} s · half-life {ASERT_HALF_LIFE_S} s · clamp{" "}
                    {ASERT_CLAMP} · anchor height {ASERT_ANCHOR_HEIGHT}
                  </td>
                </tr>
                <tr>
                  <td>Mainnet genesis</td>
                  <td className="mono">{MAINNET_GENESIS}</td>
                </tr>
                <tr>
                  <td>HRP / ports</td>
                  <td className="mono">
                    {MAINNET_HRP} · P2P {MAINNET_P2P_PORT} · RPC {MAINNET_RPC_PORT}
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
          <h3>Mint reject conditions</h3>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Reject</th>
                  <th>Meaning</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td className="mono">bad-txns-res-mint-underpaid</td>
                  <td>
                    <code>R · (2²⁵ + h)² &gt; B ≪ 40</code>
                  </td>
                </tr>
                <tr>
                  <td className="mono">bad-txns-res-mint-no-burn</td>
                  <td>RES output present, no OP_CONVERGE</td>
                </tr>
                <tr>
                  <td className="mono">bad-txns-res-mint-cap-overshoot</td>
                  <td>Would exceed 419430400000000 atoms</td>
                </tr>
              </tbody>
            </table>
          </div>
          <h3>Terminology</h3>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Term</th>
                  <th>Official meaning</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td className="mono">ORU</td>
                  <td>Medium of exchange asset. Flat subsidy 128 after genesis</td>
                </tr>
                <tr>
                  <td className="mono">RES</td>
                  <td>Store of value asset. Derived ceiling 2²² RES. Not mined</td>
                </tr>
                <tr>
                  <td className="mono">Convergence</td>
                  <td>ORU destroy that MAY mint RES at destroy_cost(h) in the same body transaction</td>
                </tr>
                <tr>
                  <td className="mono">destroy_cost(h)</td>
                  <td>Display mint cost. Consensus uses the inequality, not this quotient</td>
                </tr>
                <tr>
                  <td className="mono">OpuHash</td>
                  <td>Live PoW: {OPUHASH}</td>
                </tr>
                <tr>
                  <td>Consensus fingerprint</td>
                  <td>Observational identity. Does not accept or reject blocks</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p className="muted">
            Production consensus is <code>core/src/</code>. Lookup ends here. The claims are in
            Chapters 1–9.
          </p>
        </section>
      </article>
    </div>
  );
}
