import { Button } from "../components/Button";
import { SubNav } from "../components/SubNav";

export function UseFaucetPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Faucet</h1>
        <p className="muted">TESTNET user entry. Claim UI remains at /faucet/.</p>
      </header>

      <section className="card prose-section">
        <p>
          <strong>Network: TESTNET</strong>
        </p>
        <p>Faucet is a user service.</p>
        <p>Faucet does not define consensus rules.</p>
        <p>Faucet does not create RES.</p>
      </section>

      <div className="cta-row">
        <Button href="/faucet/" variant="primary">
          Open TESTNET Faucet
        </Button>
      </div>
    </div>
  );
}
