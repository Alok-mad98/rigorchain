import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import { explorerBaseUrl } from "../lib/explorerClient";
import { ResIssuanceCharts } from "../components/ResIssuanceCharts";

const SURFACES = [
  { href: "", title: "Home", note: "Chain identity, height, ASERT / OpuHash" },
  { href: "/blocks", title: "Blocks", note: "Height, hash, indexed blocks" },
  { href: "/oru.html", title: "ORU", note: "Indexed ORU amounts" },
  { href: "/res.html", title: "RES", note: "Mint cost curve and derived issuance ceiling" },
] as const;

export function ExplorerPortalPage() {
  const explorer = explorerBaseUrl() ?? "https://explorer.rigorchain.com";
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Explorer</h1>
        <p className="muted">
          Read-only indexer. RES mints in the same body transaction as an OP_CONVERGE destroy.
          Not a validation authority.
        </p>
      </header>

      <SectionTitle>RES issuance schedule</SectionTitle>
      <ResIssuanceCharts />

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
