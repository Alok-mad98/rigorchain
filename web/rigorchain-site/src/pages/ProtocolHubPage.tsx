import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const ENTRIES = [
  { title: "Overview", to: "/protocol/overview" },
  { title: "Machine-Verifiable Protocol", to: "/machine-readable-protocol" },
  { title: "Architecture", to: "/protocol/architecture" },
  { title: "Consensus", to: "/protocol/consensus" },
  { title: "Native Assets", to: "/protocol/assets" },
  { title: "Script-as-Asset", to: "/protocol/script-as-asset" },
  { title: "Convergence", to: "/protocol/convergence" },
  { title: "Proof-of-Work", to: "/protocol/proof-of-work" },
  { title: "Formal Verification", to: "/protocol/verification-boundary" },
] as const;

export function ProtocolHubPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Protocol</h1>
        <p className="muted">
          Protocol rules, architecture, assets, verification and consensus boundaries.
        </p>
      </header>

      <SectionTitle>Index</SectionTitle>
      <section className="card-grid">
        {ENTRIES.map((item) => (
          <DarkCard key={item.to} title={item.title} to={item.to} linkLabel="Open" />
        ))}
      </section>
    </div>
  );
}
