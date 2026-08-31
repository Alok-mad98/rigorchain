import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const TOC = [
  { id: "abstract", label: "Abstract" },
  { id: "security-budget", label: "1. Security Budget Problem" },
  { id: "dual-asset", label: "2. Dual-Asset Economic Architecture" },
  { id: "minimal-script", label: "3. Minimal Script Architecture" },
  { id: "convergence", label: "4. Convergence Consensus" },
  { id: "fairness", label: "5. Mathematical Fairness" },
  { id: "mining", label: "6. Mining Architecture" },
  { id: "self-proving", label: "7. Self-Proving Blockchain" },
  { id: "formal", label: "8. Formal Verification" },
  { id: "ecosystem", label: "9. Ecosystem and Roadmap" },
  { id: "appendix", label: "Appendix" },
] as const;

export function WhitepaperPage() {
  return (
    <div className="page whitepaper-page">
      <SubNav />

      <aside className="whitepaper-banner card" role="note">
        <p>
          This document describes protocol design and architecture.
          <br />
          It does not claim completed formal equivalence proof.
          <br />
          <span className="mono">equivalence_claimed = false.</span>
        </p>
      </aside>

      <header className="page-header">
        <p className="tag">OFFICIAL WHITEPAPER</p>
        <h1>RIGOR Protocol Whitepaper</h1>
        <p className="muted">
          A dual-asset proof-of-work protocol with deterministic Convergence and Zero Trust
          Verification.
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

      <section className="card-grid whitepaper-links">
        <DarkCard title="Explorer" to="/explorer" linkLabel="Open Explorer">
          <p>Block, ORU, RES, and fingerprint viewer. Display only — not a verdict.</p>
        </DarkCard>
        <DarkCard
          title="Zero Trust Verification"
          to="/zero-trust-verification"
          linkLabel="Verify independently"
        >
          <p>Recompute public bytes in the browser. Equal bytes are accepted.</p>
        </DarkCard>
        <DarkCard title="Mining" to="/mining" linkLabel="Mining">
          <p>Windows miner path. Mining pays ORU. Mining does not mint RES.</p>
        </DarkCard>
        <DarkCard title="Wallet" to="/wallet" linkLabel="Wallet">
          <p>Spending tool for ORU, Convergence, and RES. Not a consensus oracle.</p>
        </DarkCard>
        <DarkCard title="RFC documentation" to="/documentation/rfc" linkLabel="RFC overview">
          <p>Frozen protocol documents remain the normative rules.</p>
        </DarkCard>
        <DarkCard
          title="Formal Verification"
          to="/documentation/formal-verification"
          linkLabel="Freeze status"
        >
          <p>Strongly verified Coq model. 422 golden vectors. Not a refinement proof.</p>
        </DarkCard>
        <DarkCard
          title="Minimal Script Architecture"
          to="/documentation/security/minimal-script-architecture"
          linkLabel="Read chapter"
        >
          <p>UTXO foundation, native dual assets, restricted scripts. Not a programmable VM.</p>
        </DarkCard>
      </section>

      <article className="whitepaper-article">
        <section id="abstract" className="card prose-section whitepaper-section">
          <h2>Abstract</h2>
          <p>
            Public proof-of-work chains usually ask one native asset to circulate as a medium of
            exchange, remain scarce as a store of value, and pay miners for security. Those jobs
            conflict. That tension is the security-budget problem.
          </p>
          <p>
            <strong>RIGOR</strong> separates the jobs at consensus. <strong>ORU</strong> is the
            utility asset: fees, miner subsidy, and Convergence entry weight, with a permanent tail
            of <strong>125 ORU</strong> per block. <strong>RES</strong> is the scarce asset: maximum
            supply <strong>21,000,000</strong>. New RES appears only after ORU Convergence and a
            deterministic draw — not from mining subsidy, governance, or a 1:1 swap.
          </p>
          <p>
            Eligible ORU commitments form a public candidate set. Two roots bind that set:{" "}
            <code>Convergence_Root</code> and <code>draw_candidate_root</code>. Every honest node
            computes the same winner from the same public bytes. Fairness is a hash inequality over
            a <code>2^256</code> space. Deterministic Reserve Issuance: <code>E = 246</code>, mint
            cost <code>C = 1024</code> ORU per RES mint, expected wins per committed ORU ≈{" "}
            <code>n / 1024</code>. Historical RFC-014 / retired design:{" "}
            <code>2^240 / 2^256 = 1/65536</code> (<code>C = 65536</code> ORU) — not the shipping
            target.
          </p>
          <p>
            <strong>Zero Trust Verification</strong> means a stranger does not accept a website or
            explorer verdict. Public data is recomputed locally. Invalid modifications are
            detectable by deterministic validation.
          </p>
          <pre className="pipeline-diagram">{`RIGOR =
    2 Native Assets (ORU, RES)
  + 4 Core Monetary Script Families
  + UTXO Local Validation
  + Ed25519 Authentication
  + OpuHash + ASERT PoW
  + Deterministic Reserve Issuance
  + Deflationary Fee Burn
  + Formal Verification Model
  + No Global Asset State

Script is the compliance/classification layer.
equivalence_claimed = false`}</pre>
        </section>

        <section id="security-budget" className="card prose-section whitepaper-section">
          <h2>1. Security Budget Problem</h2>
          <p>A typical public chain forces one asset to perform three roles:</p>
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
                  <th>What they want</th>
                  <th>What they contribute to security</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Medium-of-exchange users</td>
                  <td>Cheap, frequent transfers</td>
                  <td>Fees — but they resist high fees</td>
                </tr>
                <tr>
                  <td>Store-of-value users</td>
                  <td>Scarcity and permanent protection</td>
                  <td>Few transactions, so few fees</td>
                </tr>
                <tr>
                  <td>Miners</td>
                  <td>Predictable payment for hashpower</td>
                  <td>Security of the chain</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            If issuance continues forever, the reserve story weakens. If issuance ends, the miner
            budget must be carried by fees that holders have little reason to generate. RIGOR’s
            answer is an asset split at consensus: ORU takes utility and miner payment, including a
            permanent tail. RES takes scarcity, and is created only through Convergence.
          </p>
        </section>

        <section id="dual-asset" className="card prose-section whitepaper-section">
          <h2>2. Dual-Asset Economic Architecture</h2>
          <pre className="pipeline-diagram">{`PoW  →  ORU  →  Convergence  →  RES`}</pre>
          <p>
            Proof-of-work discovers blocks and orders history. Proof-of-work does not create RES.
            Observe balances in the <Link to="/wallet">wallet</Link>.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Role</th>
                  <th>Asset</th>
                  <th>Design target</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Medium of exchange</td>
                  <td>ORU</td>
                  <td>Usage, fees, miner budget</td>
                </tr>
                <tr>
                  <td>Store of value</td>
                  <td>RES</td>
                  <td>
                    Scarcity; designed to provide long-term monetary separation between
                    transactional liquidity and a capped reserve
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
          <h3>ORU</h3>
          <p>
            Sequence <code>1000 → 500 → 250 → 125</code>, interval 210,000 blocks, then a locked
            tail of 125 ORU per block.{" "}
            <code>S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)</code>. Minimum Convergence is 1 ORU.
            Committed ORU leaves circulating supply and becomes draw weight. It does not convert 1:1
            into RES.
          </p>
          <h3>RES</h3>
          <p>
            Maximum supply 21,000,000. A successful draw mints exactly{" "}
            <code>W(h)</code> RES (<code>GetResMintReward</code>; 40.000000 RES at genesis) in the
            block body to the winner’s declared Witness v1 program — not to the miner.{" "}
            <code>M(h) ∈ {"{0,1}"}</code>. No premine. The mint <em>amount</em> follows a height
            schedule; whether a mint occurs is not calendar-guaranteed.
          </p>
          <pre className="math-block">{`S_RES(0) = 0
S_RES(h+1) = S_RES(h) + GetResMintReward(h+1) · M(h+1)
S_RES(h) ≤ 21,000,000`}</pre>
          <p>
            <strong>No Convergence → No RES.</strong> The claim that more demand makes winning
            harder is economic analysis, not a consensus guarantee.
          </p>
        </section>

        <section id="minimal-script" className="card prose-section whitepaper-section">
          <h2>3. Minimal Script Architecture</h2>
          <p>
            RIGOR does not attempt to compete with programmable blockchain platforms. It focuses
            on monetary integrity, deterministic consensus, verifiable ownership, and a minimized
            attack surface. Full chapter:{" "}
            <Link to="/documentation/security/minimal-script-architecture">
              Minimal Script Architecture
            </Link>
            .
          </p>
          <pre className="pipeline-diagram">{`Previous UTXO
      |
      |
Transaction Validation
      |
      |
New UTXO Owner`}</pre>
          <p>
            Asset identity is not external metadata. Script structure classifies the domain.
            ORU and RES are two native asset identities inside the RIGOR UTXO system. Neither
            asset is a secondary representation of another asset. They are not colored coins,
            inscriptions, Ordinals, Runes, wrapped assets, or smart-contract tokens.
          </p>
          <blockquote>
            Every additional execution capability creates additional consensus responsibility.
          </blockquote>
          <p>
            RIGOR minimizes script complexity rather than eliminating scripts. The protocol
            intentionally restricts execution semantics to preserve deterministic verification and
            reduce consensus attack surface. Spendable ORU is
            Witness v0; Convergence is <code>OP_CONVERGE</code> (ORU destruction / draw-entry).
            Witness v1 RES is an Ed25519 key-path.{" "}
            <strong>Witness v1 is not Bitcoin Taproot.</strong> Execution surfaces include Witness
            v0, P2WSH, Witness v1, Ed25519 signature verification, and <code>OP_CONVERGE</code>.
            There is no
            general-purpose VM, no arbitrary smart contracts, and no programmable execution layer.
            There is constrained transaction validation, asset-specific script rules, and
            deterministic verification logic. Convergence is a deterministic consensus mechanism,
            not a smart contract. <code>equivalence_claimed = false</code>.
          </p>
          <pre className="pipeline-diagram">{`Less capability
       ↓
Less complexity
       ↓
Smaller attack surface
       ↓
Stronger mathematical verification`}</pre>
        </section>

        <section id="convergence" className="card prose-section whitepaper-section">
          <h2>4. Convergence Consensus</h2>
          <p>
            Convergence is a deterministic cryptographic selection process over eligible economic
            actions. It is not a lottery UI, not mining, and not a 1:1 exchange counter. A user
            commits ≥ 1 ORU into an <code>OP_CONVERGE</code> output. That ORU becomes weight{" "}
            <code>f_i</code> in
            candidate set <code>F</code> (<code>W = 32</code>; already-paid winners excluded).
          </p>
          <h3>Convergence_Root</h3>
          <p>
            <code>Convergence_Root</code> is the deterministic commitment to eligible convergence
            transactions. Users and miners do not choose the sort key. There is no hidden
            randomness.
          </p>
          <pre className="math-block">{`Convergence_Root =
Hash256(
    concat(
        sort(
            eligible txids
        )
    )
)`}</pre>
          <p>
            If <code>|F| &lt; 2</code>, <code>C</code> is 32 zero bytes (no hashing). RFC-015 still
            allows a singleton <code>F</code> to compete for RES. Public predicates include{" "}
            <code>OP_CONVERGE</code>,{" "}
            <code>f_i ≥ 1 ORU</code>, a declared RES owner program, valid inputs, and a positive
            fee.
          </p>
          <h3>draw_candidate_root</h3>
          <p>
            <code>draw_candidate_root</code> is the deterministic commitment used for candidate
            selection after convergence eligibility is established. It is not another name for{" "}
            <code>Convergence_Root</code>. After <code>F</code> and <code>C</code> are fixed, each
            candidate is tested against <code>H_i &lt; T · f_i</code>.{" "}
            <code>draw_candidate_root</code> commits the inequality-eligible subset.
          </p>
          <pre className="math-block">{`σ_i  =  Hash256( txid_i  ||  C  ||  B_{h-1}  ||  LE32(h) )
f_i  =  converge_amount(tx_i)
T    =  RES_PROBABILITY_TARGET   (not mining difficulty)
W    =  32`}</pre>
          <p>
            Winner is the minimal element under integer cross-multiplication{" "}
            <code>H_i · f_j &lt; H_j · f_i</code>, with a byte-wise txid tie-break. Height{" "}
            <code>h</code> pays the set committed by block <code>h−1</code>. A miner cannot issue
            RES. Observe the live set in the <Link to="/wallet/convergence">wallet</Link> and on the{" "}
            <Link to="/explorer">Explorer</Link>.
          </p>
        </section>

        <section id="fairness" className="card prose-section whitepaper-section">
          <h2>5. Mathematical Fairness</h2>
          <p>
            Deterministic Reserve Issuance: <code>E = 246</code>, mint cost{" "}
            <code>C = 1024</code> ORU per RES mint. Expected wins per committed ORU ≈{" "}
            <code>n / 1024</code>. This is a hash-space probability component, not a guaranteed
            winning probability and not a fixed mining probability.
          </p>
          <pre className="math-block">{`E = 246
C = 1024 ORU per RES mint

p(n ORU)   =  min(1, n / 1024)           (aggregate identity)
p_draw(n)  =  n / (1024 · 32)            (per draw, W = 32)
expected wins  =  n / 1024`}</pre>
          <p>
            Historical RFC-014 parameter: <code>2^240 / 2^256 = 1/65536</code> implied{" "}
            <code>C = 65536</code> ORU. That 16-bit teaching slice is not the shipping identity.
          </p>
          <p>
            125 represents the terminal value of the defined reduction sequence{" "}
            <code>1000 → 500 → 250 → 125</code>. It is mathematical design symbolism, not a
            protocol security guarantee and not a consensus fairness proof.
          </p>
        </section>

        <section id="mining" className="card prose-section whitepaper-section">
          <h2>6. Mining Architecture</h2>
          <p>
            Mining discovers valid headers (OpuHash) and orders blocks (ASERT). Mining pays ORU.
            Mining does not mint RES. The public path is visible Windows miner participation, not
            hidden SOAP / soak mining. Product guide: <Link to="/mining">Mining</Link>.
          </p>
          <pre className="pipeline-diagram">{`Wallet rg1q
      |
local rigord  +  rigor-miner (SOLO GBT)
      |
submitblock → ProcessNewBlock
      |
ORU to miner; RES mint in next coinbase if parent draw named a winner`}</pre>
          <p>
            The node template binds <code>Convergence_Root</code> and the paid-winner push in the
            coinbase. The SOLO miner copies those bytes and grinds nonce. On acceptance, coinbase
            pays ORU to the miner. If the parent draw named a winner, the same coinbase carries{" "}
            <code>OP_RES_MINT</code> to the winner, not to the miner. A quarantined pool is not
            the test-phase path.
          </p>
        </section>

        <section id="self-proving" className="card prose-section whitepaper-section">
          <h2>7. Self-Proving Blockchain</h2>
          <p>
            <strong>Self-Proving Blockchain Commitment</strong> is equality of commitment and
            recomputation, not an explorer flag.
          </p>
          <pre className="pipeline-diagram">{`Convergence tx
    → OP_CONVERGE commitment
    → Convergence_Root / draw_candidate_root
    → coinbase commitment
    → coinbase txid
    → merkle root
    → block header
    → OpuHash`}</pre>
          <p>
            The Consensus Fingerprint is a verification identity layer. It is not a consensus-rule
            replacement. A matching fingerprint cannot accept an invalid block.
          </p>
          <pre className="math-block">{`ConsensusFingerprint =
  Hash256(Convergence_Root || winner_txid || Sigma || height || target_prev)`}</pre>
          <p>
            <strong>Zero Trust Verification</strong> recomputes public data locally. You do not need
            to trust a node. Invalid modifications are detectable by deterministic validation. Open
            the <Link to="/zero-trust-verification">Zero Trust Verification</Link> portal.
          </p>
        </section>

        <section id="formal" className="card prose-section whitepaper-section">
          <h2>8. Formal Verification</h2>
          <p>
            Three layers. They must not be collapsed into one “formally proven blockchain” claim.
            Current freeze: strongly verified Coq model; tighter correspondence;{" "}
            <span className="mono">equivalence_claimed = false</span>.
          </p>
          <p>
            Public freeze page: <Link to="/documentation/formal-verification">Formal Verification</Link>.
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
                  <td>Layer 1 — Frozen protocol specification</td>
                  <td>RFC / SPEC / freeze declaration</td>
                  <td>A running chain</td>
                </tr>
                <tr>
                  <td>Layer 2 — Scientific / mathematical model</td>
                  <td>Strongly verified Coq model properties</td>
                  <td>A C++ refinement proof</td>
                </tr>
                <tr>
                  <td>Layer 3 — Runtime differential verification</td>
                  <td>Golden vectors, tripartite hash, σ/root CLI</td>
                  <td>Coq equals C++; shared production hash binary</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            Living lock: <strong>422</strong> shared golden vectors, <strong>215</strong>{" "}
            Coq-enforced vectors, tripartite hash validation, production σ/root CLI in the
            differential gate (operator freeze day recorded 324/117). This strengthens
            correspondence. It does not claim full refinement.
          </p>
          <p>
            <strong>Runtime Archive (PROOF-034)</strong> is a separate empirical campaign. It
            materializes catalogued golden-vector heights. It is not a protocol equivalence proof
            and is not a license to set <code>equivalence_claimed = true</code>.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Height</th>
                  <th>Vector</th>
                  <th>Phenomenon</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td className="mono">630,000</td>
                  <td className="mono">GV-ORU-0003</td>
                  <td>ORU halving / floor region</td>
                </tr>
                <tr>
                  <td className="mono">2,100,000</td>
                  <td className="mono">GV-RES-0003</td>
                  <td>RES cap boundary</td>
                </tr>
                <tr>
                  <td className="mono">2,100,001</td>
                  <td className="mono">GV-DRAW-0003</td>
                  <td>DRAW reject fixture</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            Observer pages: <Link to="/experiment">PROOF-034</Link> · <Link to="/status">Status</Link>{" "}
            · <Link to="/lab">Lab</Link>.
          </p>
          <pre className="math-block">equivalence_claimed = false</pre>
        </section>

        <section id="ecosystem" className="card prose-section whitepaper-section">
          <h2>9. Ecosystem and Roadmap</h2>
          <p>
            RIGOR is a protocol plus observer and participation surfaces. None of the surfaces below
            is a consensus authority.
          </p>
          <div className="table-wrap">
            <table className="data-table">
              <thead>
                <tr>
                  <th>Surface</th>
                  <th>Route</th>
                  <th>Role</th>
                </tr>
              </thead>
              <tbody>
                <tr>
                  <td>Wallet</td>
                  <td>
                    <Link to="/wallet">/wallet</Link>
                  </td>
                  <td>ORU, maturity, Convergence, RES</td>
                </tr>
                <tr>
                  <td>Mining</td>
                  <td>
                    <Link to="/mining">/mining</Link>
                  </td>
                  <td>Windows miner participation</td>
                </tr>
                <tr>
                  <td>Explorer</td>
                  <td>
                    <Link to="/explorer">/explorer</Link>
                  </td>
                  <td>Viewer / indexer</td>
                </tr>
                <tr>
                  <td>Zero Trust Verification</td>
                  <td>
                    <Link to="/zero-trust-verification">/zero-trust-verification</Link>
                  </td>
                  <td>Browser verifier</td>
                </tr>
                <tr>
                  <td>Security Architecture</td>
                  <td>
                    <Link to="/documentation/security">/documentation/security</Link>
                  </td>
                  <td>Minimal scripts, dual native assets</td>
                </tr>
                <tr>
                  <td>Minimal Script Architecture</td>
                  <td>
                    <Link to="/documentation/security/minimal-script-architecture">
                      /documentation/security/minimal-script-architecture
                    </Link>
                  </td>
                  <td>UTXO / restricted witness chapter</td>
                </tr>
                <tr>
                  <td>RFC documentation</td>
                  <td>
                    <Link to="/documentation/rfc">/documentation/rfc</Link>
                  </td>
                  <td>Frozen rules index</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p>
            No investment yield is promised. No date is given for filling 21 million RES. Source
            markdown: <code>docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md</code>.
          </p>
        </section>

        <section id="appendix" className="card prose-section whitepaper-section">
          <h2>Appendix</h2>
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
                  <td>Medium of exchange asset</td>
                </tr>
                <tr>
                  <td className="mono">RES</td>
                  <td>Store of value asset</td>
                </tr>
                <tr>
                  <td className="mono">Convergence</td>
                  <td>ORU consumption mechanism creating RES eligibility</td>
                </tr>
                <tr>
                  <td className="mono">Convergence_Root</td>
                  <td>Deterministic commitment to eligible convergence transactions</td>
                </tr>
                <tr>
                  <td className="mono">draw_candidate_root</td>
                  <td>
                    Deterministic commitment used for candidate selection after convergence
                    eligibility is established
                  </td>
                </tr>
                <tr>
                  <td>Self-Proving Blockchain Commitment</td>
                  <td>Equality of commitment and recomputation</td>
                </tr>
                <tr>
                  <td>Zero Trust Verification</td>
                  <td>Independent recomputation of public data</td>
                </tr>
                <tr>
                  <td>Fingerprint</td>
                  <td>Verification identity layer</td>
                </tr>
                <tr>
                  <td>Runtime Archive (PROOF-034)</td>
                  <td>Layer 3 empirical campaign. equivalence_claimed = false</td>
                </tr>
              </tbody>
            </table>
          </div>
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
                  <td>1000 → 500 → 250 → 125</td>
                </tr>
                <tr>
                  <td>RES cap / mint</td>
                  <td>21,000,000 / W(h) (40.000000 at genesis)</td>
                </tr>
                <tr>
                  <td>CONVERGENCE_DRAW_WINDOW</td>
                  <td>32</td>
                </tr>
                <tr>
                  <td>σ preimage</td>
                  <td className="mono">txid || C || B_h-1 || LE32(h)</td>
                </tr>
              </tbody>
            </table>
          </div>
          <p className="muted">
            Normative rules remain in RFC / SPEC. This page is public explanation only.
          </p>
        </section>
      </article>

      <div className="cta-row">
        <Button to="/zero-trust-verification" variant="primary">
          Zero Trust Verification
        </Button>
        <Button to="/mining">Mining</Button>
        <Button to="/wallet">Wallet</Button>
        <Button to="/explorer">Explorer</Button>
        <Button to="/documentation/rfc">RFC documentation</Button>
      </div>
    </div>
  );
}
