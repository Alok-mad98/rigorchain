import { Link, useLocation } from "react-router-dom";
import { ConvergencePanel } from "../components/ConvergencePanel";
import { ProtocolCard } from "../components/ProtocolCard";
import { RESMintPanel } from "../components/RESMintPanel";
import { SubNav } from "../components/SubNav";

export function WalletTopicPage() {
  const { pathname } = useLocation();

  if (pathname.endsWith("/oru")) {
    return (
      <div className="page">
        <SubNav />
        <header className="page-header">
          <h1>ORU</h1>
          <p className="muted">Execution and fee resource. Existing ProtocolCard module — KEEP.</p>
        </header>
        <ProtocolCard variant="oru" />
        <p>
          <Link to="/wallet">Wallet overview</Link>
        </p>
      </div>
    );
  }

  if (pathname.endsWith("/res")) {
    return (
      <div className="page">
        <SubNav />
        <header className="page-header">
          <h1>RES</h1>
          <p className="muted">Scarce settlement asset. Existing RESMintPanel — previously unwired, now WIRED.</p>
        </header>
        <ProtocolCard variant="res" />
        <RESMintPanel />
      </div>
    );
  }

  if (pathname.endsWith("/convergence")) {
    return (
      <div className="page">
        <SubNav />
        <header className="page-header">
          <h1>Convergence</h1>
          <p className="muted">
            Deterministic selection. Existing ConvergencePanel — previously unwired, now WIRED.
          </p>
        </header>
        <ProtocolCard variant="forge" />
        <ConvergencePanel />
      </div>
    );
  }

  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <p className="tag">PLACEHOLDER</p>
        <h1>Maturity</h1>
        <p className="muted">When wallet rewards become spendable — content planned, not deleted from elsewhere.</p>
      </header>
      <section className="card prose-section">
        <p>
          This section will map confirmation and maturity language from the wallet GUI onto the
          public site. Source notes live in <code>docs/wallet/</code>. No consensus logic is
          implemented here.
        </p>
        <ul>
          <li>Confirmation count as observation, not authority</li>
          <li>ORU subsidy maturity vs RES mint linkage</li>
          <li>Link to independent verification of the producing TXID</li>
        </ul>
        <p>
          <Link to="/zero-trust-verification">Verify a transaction</Link>
        </p>
      </section>
    </div>
  );
}
