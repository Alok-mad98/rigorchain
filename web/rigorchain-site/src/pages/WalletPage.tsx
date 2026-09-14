import { ConvergencePanel } from "../components/ConvergencePanel";
import { DarkCard } from "../components/DarkCard";
import { RESMintPanel } from "../components/RESMintPanel";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function WalletPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Wallet</h1>
        <p className="muted">A spending tool. It is not an authority for consensus.</p>
      </header>

      <SectionTitle>Balances and state</SectionTitle>
      <section className="card-grid">
        <DarkCard title="ORU" to="/wallet/oru" linkLabel="ORU detail">
          <p>Execution and fee resource. Observe subsidy and spendable ORU separately from RES.</p>
        </DarkCard>
        <DarkCard title="Maturity" to="/wallet/maturity" linkLabel="Maturity">
          <p>Confirmation and spendability are observations, not a verdict.</p>
        </DarkCard>
        <DarkCard title="Convergence" to="/wallet/convergence" linkLabel="Convergence">
          <p>Destroy ORU at destroy_cost(h) to mint RES. The miner does not receive the RES.</p>
        </DarkCard>
        <DarkCard title="RES" to="/wallet/res" linkLabel="RES detail">
          <p>Scarce settlement asset. Mint linkage is distinct from ORU balance.</p>
        </DarkCard>
      </section>

      <SectionTitle>Live observer panels</SectionTitle>
      <ConvergencePanel />
      <RESMintPanel />

      <section className="card prose-section">
        <h2>Get a transaction ID</h2>
        <pre className="mono-json">{`gettransaction <txid>
sendtoaddress  # returns the TXID`}</pre>
        <p>
          Use that TXID to inspect the transaction in Explorer.
        </p>
      </section>
    </div>
  );
}
