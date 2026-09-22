import { Link } from "react-router-dom";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

/**
 * Public pre-mainnet overview of native assets.
 * Exact issuance mathematics stays unpublished until mainnet.
 */

export function MonetaryPolicyPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">NATIVE ASSETS</p>
        <h1>ORU and RES</h1>
        <p className="muted">
          Two native assets, one ledger. Issuance follows deterministic protocol rules. The
          complete specification will be published with mainnet launch.
        </p>
      </header>

      <aside className="whitepaper-banner card" role="note">
        <p>
          RES issuance follows deterministic protocol rules designed to balance participation,
          scarcity, and long-term reserve behavior.
        </p>
        <p>The complete specification will be published with mainnet launch.</p>
      </aside>

      <SectionTitle>Two native assets</SectionTitle>
      <section className="card prose-section">
        <p>
          <strong>ORU</strong> is the fee and proof-of-work subsidy asset. It pays for transactions
          and for chain security. <strong>RES</strong> is the reserve asset. RES is not a mining
          reward and not a separate token registry.
        </p>
        <p>
          Both assets live in the same UTXO set. Asset class is determined by the output script.
          There is no account ledger and no global token table.
        </p>
      </section>

      <SectionTitle>Destroy to create</SectionTitle>
      <section className="card prose-section">
        <p>
          Convergence is a destroy-to-create rule. A body transaction destroys ORU and may create
          RES in the same transaction when the protocol admits the operation. Destroyed ORU is not
          reassigned to the miner. Created RES is a reserve output of that transaction.
        </p>
        <p>
          A destroy that does not create RES remains a valid destruction. Creation is optional and
          bounded by consensus. It is not a market quote and not a governance vote.
        </p>
        <p>
          RES issuance follows deterministic protocol rules designed to balance participation,
          scarcity, and long-term reserve behavior. The complete specification will be published
          with mainnet launch.
        </p>
      </section>

      <SectionTitle>Transparent accounting</SectionTitle>
      <section className="card prose-section">
        <p>
          Every admitted operation is a transaction on the public chain. Observers can see ORU
          destroyed and RES created. The Explorer records those events. It does not authorize them.
        </p>
        <p>
          Circulating supply depends on participation. A scarcity bound is a ceiling on what the
          rules can create. It is not a promise that the ceiling will be filled.
        </p>
      </section>

      <SectionTitle>Fair launch</SectionTitle>
      <section className="card prose-section">
        <p>
          There is no premine and no public sale on this website. ORU enters circulation through
          the proof-of-work subsidy and fees. RES enters circulation only through Convergence.
        </p>
        <p>
          This network is a public test network. Coins have no monetary value. Mainnet parameters
          will be released at mainnet launch.
        </p>
        <p>
          Verification overview:{" "}
          <Link to="/documentation/formal-verification">Formal verification</Link>.
        </p>
      </section>
    </div>
  );
}
