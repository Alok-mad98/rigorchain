import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function FormalVerificationPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">FORMAL VERIFICATION</p>
        <h1>Formal Verification</h1>
        <p className="muted">Public verification overview for the RIGOR consensus model.</p>
      </header>

      <aside className="whitepaper-banner card" role="note">
        <p>
          Formal verification covers selected consensus properties through a machine-checked Coq
          model.
        </p>
        <p>
          The production implementation remains the operational consensus implementation.
        </p>
        <p>
          The verification effort is an ongoing engineering process and does not claim full binary
          equivalence between the model and production software.
        </p>
      </aside>

      <section className="card prose-section">
        <h2>Overview</h2>
        <p>
          RIGOR maintains a Coq model of selected consensus properties. The model is published so
          that those results can be checked independently of this website.
        </p>
        <p>
          The model is a verification aid. It does not replace the software that runs the network.
        </p>
      </section>

      <section className="three-layer-grid">
        <article className="card layer-card">
          <h3>Production software</h3>
          <p>
            The node that validates blocks and transactions is the operational consensus
            implementation.
          </p>
        </article>
        <article className="card layer-card">
          <h3>Coq model</h3>
          <p>
            Selected properties of the consensus design are stated and machine-checked in Coq.
          </p>
        </article>
        <article className="card layer-card">
          <h3>Independent review</h3>
          <p>
            Reviewers can replay the published development. A successful replay checks the model.
            It is not a claim that the binary and the model are the same artifact.
          </p>
        </article>
      </section>

      <section className="card prose-section">
        <h2>Properties in scope</h2>
        <p>The public verification effort addresses selected properties of the model, including:</p>
        <ul>
          <li>Conservation of value inside the model</li>
          <li>UTXO validity and the absence of an in-model double spend</li>
          <li>Issuance rules as stated in the model, including the destroy-to-create relationship</li>
          <li>Chain induction over intermediate states</li>
          <li>Script classification sufficient to separate spendable and unspendable outputs</li>
        </ul>
      </section>

      <section className="card prose-section">
        <h2>Limitations</h2>
        <p>The overview is intentionally general. It does not claim that verification is finished.</p>
        <ul>
          <li>The production implementation remains the operational consensus implementation.</li>
          <li>
            Machine-checked results are properties of the model. They are not a proof of full binary
            equivalence.
          </li>
          <li>
            Cryptography, networking, wallets, and hardware are outside the checked model.
          </li>
          <li>
            The complete specification, including remaining verification scope, will be published
            with mainnet launch.
          </li>
        </ul>
      </section>

      <SectionTitle>Related</SectionTitle>
      <section className="card-grid">
        <DarkCard title="Security architecture" to="/documentation/security" linkLabel="Read">
          <p>Why the script surface stays small enough to verify.</p>
        </DarkCard>
        <DarkCard title="Native assets" to="/documentation/monetary-policy" linkLabel="Read">
          <p>ORU, RES, and the destroy-to-create relationship, without a pre-mainnet parameter sheet.</p>
        </DarkCard>
        <DarkCard title="Protocol" to="/protocol" linkLabel="Open">
          <p>High-level architecture, UTXO model, and proof-of-work identity.</p>
        </DarkCard>
      </section>

      <section className="card prose-section">
        <p className="muted">
          Script surface:{" "}
          <Link to="/documentation/security/minimal-script-architecture">
            Minimal Script Architecture
          </Link>
          . Issuance overview: <Link to="/documentation/monetary-policy">native assets</Link>.
        </p>
      </section>
    </div>
  );
}
