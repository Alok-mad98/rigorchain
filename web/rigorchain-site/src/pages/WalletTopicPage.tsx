import { useLocation } from "react-router-dom";
import { ConvergencePanel } from "../components/ConvergencePanel";
import { ORUSupplyPanel } from "../components/ORUSupplyPanel";
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
          <p className="muted">Fee asset, proof-of-work subsidy, and the ORU destroyed in Convergence.</p>
        </header>
        <ProtocolCard variant="oru" />
        <ORUSupplyPanel />
      </div>
    );
  }

  if (pathname.endsWith("/res")) {
    return (
      <div className="page">
        <SubNav />
        <header className="page-header">
          <h1>RES</h1>
          <p className="muted">
            Reserve asset. Created when a body transaction destroys ORU under consensus rules.
          </p>
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
            Destroy ORU via OP_CONVERGE to mint Witness v1 RES at the confirming height.
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
        <p className="tag">WALLET</p>
        <h1>Maturity</h1>
        <p className="muted">
          When a created UTXO becomes spendable. The wallet displays maturity; consensus enforces
          it.
        </p>
      </header>
      <section className="card prose-section">
        <p>
          Newly issued ORU and newly created RES become spendable after the protocol confirmation
          depth. Ordinary RES transfers cannot create RES. The wallet displays that depth.
          Consensus enforces it.
        </p>
        <p>
          A wallet display can lag the consensus rule by a confirmation. Spending still has to
          satisfy the node.
        </p>
        <ul>
          <li>Confirmation count as observation, not authority</li>
          <li>ORU subsidy maturity vs RES mint linkage</li>
          <li>Inspect the producing TXID in Explorer</li>
        </ul>
      </section>
    </div>
  );
}
