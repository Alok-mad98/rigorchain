import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import { explorerBaseUrl } from "../lib/explorerClient";
const SURFACES = [
  { href: "", title: "Home", note: "Chain identity and height" },
  { href: "/blocks", title: "Blocks", note: "Height, hash, indexed blocks" },
  { href: "/oru.html", title: "ORU", note: "Indexed ORU amounts" },
  { href: "/res.html", title: "RES", note: "Reserve operations and created RES" },
  { href: "/res-events.html", title: "RES events", note: "ORU destroyed and RES created" },
] as const;

export function ExplorerPortalPage() {
  const explorer = explorerBaseUrl() ?? "http://explorer.rigorchain.com";
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Explorer</h1>
        <p className="muted">
          Read-only indexer. RES events display protocol-defined reserve operations. Each event
          records transparent ORU destruction and corresponding RES creation according to consensus
          rules. The Explorer is not a validation authority.
        </p>
      </header>

      <section className="card prose-section">
        <h2>What the Explorer shows</h2>
        <p>
          RES issuance follows deterministic protocol rules designed to balance participation,
          scarcity, and long-term reserve behavior. This portal does not publish the issuance
          equation or a projected ceiling. The complete specification will be published with
          mainnet launch.
        </p>
      </section>

      <SectionTitle>Surfaces</SectionTitle>
      <section className="card-grid">
        {SURFACES.map((item) => (
          <article key={item.title} className="card dark-card">
            <h3>{item.title}</h3>
            <p>{item.note}</p>
            <div className="dark-card-actions">
              <a className="ui-btn" href={`${explorer}${item.href}`} rel="noreferrer" target="_blank">
                Open
              </a>
            </div>
          </article>
        ))}
      </section>

      <section className="card prose-section">
        <p>
          Live viewer:{" "}
          <a href={explorer} rel="noreferrer" target="_blank">
            {explorer}
          </a>
          . Explorer is a read-only indexer, not a validation authority.
        </p>
      </section>
    </div>
  );
}
