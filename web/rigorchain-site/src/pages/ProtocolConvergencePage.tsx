import { DarkCard } from "../components/DarkCard";
import { SubNav } from "../components/SubNav";

export function ProtocolConvergencePage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Convergence</h1>
        <p className="muted">Convergence protocol entry.</p>
      </header>

      <section className="card-grid">
        <DarkCard title="Native Assets" to="/protocol/assets" linkLabel="Open" />
        <DarkCard title="Wallet Convergence" to="/use/wallet/convergence" linkLabel="Open" />
      </section>
    </div>
  );
}
