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
          <p className="muted">Fee asset, PoW subsidy, Convergence destroy. R(0) = 0, then 128 forever.</p>
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
          <p className="muted">Reserve asset. Body-tx destroy-to-mint at destroy_cost(h). Cap 4,194,304 RES.</p>
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
          ORU coinbase outputs and body RES <em>mint</em> outputs use the same confirmation depth:{" "}
          <code>nCoinbaseMaturity = 100</code>. A UTXO created at height H is first spendable in a
          transaction whose spend height is H + 100. Mempool spend height is tip + 1. Ordinary RES
          transfers are not delayed by the mint flag and still cannot create RES (
          <code>res_in ≥ res_out</code>).
        </p>
        <p>
          Wallet UI may show a mint immature for one extra confirmation relative to consensus — the
          same Bitcoin coinbase display lag. RPC or miner-created spends cannot bypass{" "}
          <code>CheckTxInputs</code>.
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
