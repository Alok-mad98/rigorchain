import { useEffect } from "react";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const PAGE_TITLE = "RIGOR Machine-Verifiable Protocol | Deterministic Blockchain Economics";
const PAGE_DESCRIPTION =
  "RIGOR is a deterministic blockchain economic protocol designed for human and machine verification. RFC-020 defines RES economics through exact mathematical rules.";
const PAGE_KEYWORDS =
  "deterministic blockchain, machine verifiable protocol, blockchain economic model, autonomous agent compatible protocol";

function usePageMeta() {
  useEffect(() => {
    const previousTitle = document.title;
    const description = document.querySelector('meta[name="description"]');
    const previousDescription = description?.getAttribute("content") ?? null;
    let keywords = document.querySelector('meta[name="keywords"]');
    const createdKeywords = keywords === null;
    if (!keywords) {
      keywords = document.createElement("meta");
      keywords.setAttribute("name", "keywords");
      document.head.appendChild(keywords);
    }
    const previousKeywords = createdKeywords ? null : keywords.getAttribute("content");

    document.title = PAGE_TITLE;
    description?.setAttribute("content", PAGE_DESCRIPTION);
    keywords.setAttribute("content", PAGE_KEYWORDS);

    return () => {
      document.title = previousTitle;
      if (description && previousDescription !== null) {
        description.setAttribute("content", previousDescription);
      }
      if (createdKeywords) {
        keywords.remove();
      } else if (previousKeywords !== null) {
        keywords.setAttribute("content", previousKeywords);
      }
    };
  }, []);
}

export function MachineVerifiableProtocolPage() {
  usePageMeta();

  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">PROTOCOL</p>
        <h1>Machine-Verifiable Protocol</h1>
        <p className="muted">
          RIGOR is a protocol designed for machine verification, machine computation, and machine
          participation.
        </p>
      </header>

      <section className="card prose-section">
        <h2>Introduction</h2>
        <p>
          A digital asset system has to be readable by people and verifiable by machines. Autonomous
          agents, AI-assisted wallets, automated financial tools, and algorithmic protocol analysis
          all read the same rules. Those rules have to be deterministic, reproducible, mathematically
          defined, and independently verifiable.
        </p>
        <p>
          RIGOR is a protocol designed for machine verification, machine computation, and machine
          participation.
        </p>
      </section>

      <section className="card prose-section">
        <h2>RIGOR Design Philosophy</h2>
        <p>RIGOR is designed around deterministic rules.</p>
        <p>The protocol does not rely on:</p>
        <ul>
          <li>discretionary monetary policy</li>
          <li>centralized adjustment</li>
          <li>an external price oracle</li>
          <li>hidden economic parameters</li>
        </ul>
        <p>Rules are defined mathematically and executed identically by every participant.</p>
      </section>

      <section className="card prose-section">
        <h2>RFC-020: Deterministic Economics</h2>
        <p>RFC-020 is the mathematical foundation of deterministic economics.</p>
        <p>It defines:</p>
        <ul>
          <li>continuous RES destroy cost</li>
          <li>flat ORU emission</li>
          <li>deterministic issuance calculation</li>
        </ul>
        <p>
          Given the same blockchain state, different implementations should produce the same result.
        </p>
      </section>

      <section className="card prose-section">
        <h2>Human and Machine Verification</h2>
        <p>RIGOR is designed for three readers of the same rules.</p>
        <p>Human users:</p>
        <ul>
          <li>understand economic rules</li>
          <li>verify assumptions</li>
        </ul>
        <p>Developers:</p>
        <ul>
          <li>implement independent clients</li>
          <li>build analytical tools</li>
        </ul>
        <p>Machines:</p>
        <ul>
          <li>calculate future states</li>
          <li>verify economic conditions</li>
          <li>simulate protocol behavior</li>
        </ul>
      </section>

      <section className="card prose-section">
        <h2>Not AI Controlled</h2>
        <p>RIGOR is not an AI-controlled blockchain.</p>
        <p>AI does not modify consensus.</p>
        <p>AI does not decide monetary policy.</p>
        <p>AI only interacts with transparent deterministic rules.</p>
        <p>Machine-verifiable does not mean machine-controlled.</p>
      </section>

      <SectionTitle>Related</SectionTitle>
      <section className="card-grid">
        <DarkCard title="Protocol" to="/protocol" linkLabel="Open" />
        <DarkCard title="Native Assets" to="/protocol/assets" linkLabel="Open" />
        <DarkCard title="Documentation" to="/documentation" linkLabel="Open" />
      </section>
    </div>
  );
}
