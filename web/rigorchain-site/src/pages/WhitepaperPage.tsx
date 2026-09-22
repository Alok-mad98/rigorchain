import { Link } from "react-router-dom";
import { SubNav } from "../components/SubNav";

const TOC = [
  { id: "abstract", label: "Abstract" },
  { id: "security-budget", label: "1. Why RIGOR Separates Two Monetary Functions" },
  { id: "dual-asset", label: "2. Two Native Assets" },
  { id: "utxo", label: "3. UTXO Identity" },
  { id: "convergence", label: "4. Convergence" },
  { id: "issuance", label: "5. Issuance Principles" },
  { id: "mining", label: "6. Proof-of-Work" },
  { id: "local", label: "7. Local Verification" },
  { id: "formal", label: "8. Formal Verification" },
  { id: "surfaces", label: "9. Product Surfaces" },
  { id: "security", label: "10. Security Model" },
  { id: "tradeoffs", label: "11. Design Trade-offs" },
  { id: "evolution", label: "12. Testnet and Mainnet" },
] as const;

export function WhitepaperPage() {
  return (
    <div className="page whitepaper-page">
      <SubNav />

      <aside className="whitepaper-banner card" role="note">
        <p>
          This is a public overview of the RIGOR design. It is not the normative specification.
        </p>
        <p>
          The complete specification will be published with mainnet launch. RIGOR Testnet is a
          public testing network for evaluation, mining participation, and software testing.
        </p>
      </aside>

      <header className="page-header">
        <p className="tag">PROTOCOL</p>
        <h1>RIGOR Protocol</h1>
        <p className="muted">
          A dual-asset UTXO system. Proof-of-work orders the chain. Reserve creation is separate
          from mining.
        </p>
      </header>

      <nav className="whitepaper-toc card" aria-label="Contents">
        <ol>
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
            RIGOR explores a long-standing design question in proof-of-work systems: how one native
            asset can support transaction utility, long-term scarcity, and miner security at the
            same time.
          </p>
          <p>
            A single-asset design asks one unit to circulate, stay scarce, and pay for security.
            Those roles can pull in different directions. RIGOR treats that tension as an
            architectural problem and separates the roles at the consensus asset layer.
          </p>
          <p>
            <strong>ORU</strong> is the utility asset: fees and the proof-of-work subsidy.{" "}
            <strong>RES</strong> is the reserve asset. New RES is created only when a body
            transaction destroys ORU under deterministic consensus rules. RES is not mined.
          </p>
          <p>
            There is no general-purpose script machine and no account ledger. Nodes recompute the
            same public data. This paper does not claim market outcomes, and it does not claim that
            the verification model and the production software are the same binary.
          </p>
        </section>

        <section id="security-budget" className="card prose-section whitepaper-section">
          <h2>1. Why RIGOR Separates Two Monetary Functions</h2>
          <p>
            A public proof-of-work chain has to do three jobs at once: move value, preserve
            scarcity, and pay for security. Assigning all three jobs to one asset creates a
            security-budget tension. That is a design observation, not a market forecast.
          </p>
          <p>
            RIGOR keeps a single ledger and a single proof-of-work chain, and gives the two jobs
            to two native assets. ORU carries fees and miner payment. RES carries the scarcity
            role. Neither asset is a wrapper around the other.
          </p>
        </section>

        <section id="dual-asset" className="card prose-section whitepaper-section">
          <h2>2. Two Native Assets</h2>
          <p>
            ORU and RES are native UTXO assets. They are not colored coins, inscriptions, or smart
            contract tokens. Asset class is inferred from the output script. Script does not, by
            itself, authorize creation of either asset.
          </p>
          <p>
            ORU is spent as fees and issued as the block subsidy. RES is created only through
            Convergence. Mining a block does not mint RES. Pool policy cannot mint RES.
          </p>
        </section>

        <section id="utxo" className="card prose-section whitepaper-section">
          <h2>3. UTXO Identity</h2>
          <p>
            Each asset exists as an unspent transaction output. There are no hidden balances and no
            global account state. Every node checks inputs, outputs, signatures, and asset rules.
            The same input must produce the same validation result.
          </p>
          <p>
            The script surface is intentionally small. Spendable outputs use a restricted witness
            form. There is no general-purpose virtual machine.
          </p>
        </section>

        <section id="convergence" className="card prose-section whitepaper-section">
          <h2>4. Convergence</h2>
          <p>
            Convergence is the destroy-to-create relationship. One body transaction destroys ORU
            and may create RES in that same transaction when consensus admits the operation.
            Destroyed ORU is removed. It is not paid to the miner as the reserve asset.
          </p>
          <p>
            Destroying ORU without creating RES remains possible. Creation is not a 1:1 swap, not a
            governance action, and not a market quote. The protocol defines when creation is valid.
            Participants decide whether to use it.
          </p>
        </section>

        <section id="issuance" className="card prose-section whitepaper-section">
          <h2>5. Issuance Principles</h2>
          <p>
            RES issuance follows deterministic protocol rules designed to balance participation,
            scarcity, and long-term reserve behavior.
          </p>
          <p>The complete specification will be published with mainnet launch.</p>
          <p>
            Until then, the public description is the relationship, not the parameter sheet. ORU
            destruction and RES creation are recorded on chain. Supply that appears is the supply
            participants actually created. A scarcity ceiling, when published, is an upper bound,
            not a promise that it will be filled.
          </p>
        </section>

        <section id="mining" className="card prose-section whitepaper-section">
          <h2>6. Proof-of-Work</h2>
          <p>
            The chain is ordered by proof-of-work. The proof-of-work identity is{" "}
            <strong>OpuHash</strong>. Difficulty adjustment is ASERT. Mining is paid in ORU.
          </p>
          <p>
            There is no premine and no public sale on this website. Fair launch means new ORU
            comes from the subsidy and fees, and new RES comes from Convergence. Finding a block
            does not legalize a Convergence operation that consensus rejects.
          </p>
        </section>

        <section id="local" className="card prose-section whitepaper-section">
          <h2>7. Local Verification</h2>
          <p>
            A website, an explorer, or a wallet is a display. A stranger does not have to accept
            that display. Public data can be recomputed by a node. Nodes that see the same public
            data reach the same consensus decision.
          </p>
        </section>

        <section id="formal" className="card prose-section whitepaper-section">
          <h2>8. Formal Verification</h2>
          <p>
            Formal verification covers selected consensus properties through a machine-checked Coq
            model.
          </p>
          <p>The production implementation remains the operational consensus implementation.</p>
          <p>
            The verification effort is an ongoing engineering process and does not claim full binary
            equivalence between the model and production software.
          </p>
          <p>
            <Link to="/documentation/formal-verification">Formal verification overview</Link>.
          </p>
        </section>

        <section id="surfaces" className="card prose-section whitepaper-section">
          <h2>9. Product Surfaces</h2>
          <p>
            The website, Explorer, wallet, and miner are product surfaces. They help people
            observe and participate. They are not consensus authorities. A balance shown on a page
            is an observation of public data.
          </p>
        </section>

        <section id="security" className="card prose-section whitepaper-section">
          <h2>10. Security Model</h2>
          <p>
            RIGOR reduces consensus complexity instead of adding a programmable execution layer.
            Fewer execution paths mean a smaller attack surface and a verification problem that
            stays finite.
          </p>
          <p>
            Signatures authenticate ownership. Consensus rules authenticate creation. Script
            classifies the asset. None of those layers is a general smart-contract platform.
          </p>
        </section>

        <section id="tradeoffs" className="card prose-section whitepaper-section">
          <h2>11. Design Trade-offs</h2>
          <p>
            RIGOR does not aim to host arbitrary applications. The trade-off is deliberate: less
            programmability, less complexity, a smaller attack surface, and a clearer verification
            story.
          </p>
          <p>
            What the system does not provide is part of the design. There is no token factory, no
            account model, and no open-ended script interpreter.
          </p>
        </section>

        <section id="evolution" className="card prose-section whitepaper-section">
          <h2>12. Testnet and Mainnet</h2>
          <p>
            RIGOR Testnet is a public testing network. It is provided for evaluation, mining
            participation, and software testing. Test coins have no monetary value.
          </p>
          <p>
            Mainnet parameters and the final public specification will be released at mainnet
            launch. This overview will not be treated as a substitute for that specification.
          </p>
        </section>
      </article>
    </div>
  );
}
