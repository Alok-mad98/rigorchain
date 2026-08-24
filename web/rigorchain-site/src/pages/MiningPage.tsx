import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

export function MiningPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Mining</h1>
        <p className="muted">
          Windows Miner ecosystem. Miners verify blocks independently of the pool or node UI.
        </p>
      </header>

      <SectionTitle>Mining Overview</SectionTitle>
      <section className="card prose-section">
        <p>
          Mining discovers valid headers. RES allocation is a deterministic draw over the committed
          set F — not a pool policy. Start with a wallet address, a miner binary, and an optional
          pool.
        </p>
        <p className="muted">
          Installer scripts: <code>installer/windows/rigor-miner.cmd</code>,{" "}
          <code>start-wallet.cmd</code>. This page does not run miners.
        </p>
      </section>

      <section className="card-grid">
        <DarkCard title="Miner Download" to="/download/miner" linkLabel="Get miner">
          <p>Windows and Linux miner artifacts. Verify checksums, then connect a wallet address.</p>
        </DarkCard>
        <DarkCard title="Pool Dashboard" to="/mining/pool" linkLabel="Pool status">
          <p>Read-only pool observer. Hashrate and workers are display data, not consensus.</p>
        </DarkCard>
        <DarkCard title="Mining Economics" to="/mining/economics" linkLabel="Economics">
          <p>ORU subsidy 1000 → 500 → 250 → 125. No premine. RES is not mined.</p>
        </DarkCard>
        <DarkCard title="Windows Miner Guide" to="/mining/windows" linkLabel="Tutorial">
          <p>Wallet → miner → pool → observe ORU → verify a found block independently.</p>
        </DarkCard>
      </section>

      <div className="cta-row">
        <Button to="/download/miner">Download miner</Button>
        <Button to="/wallet">Connect wallet</Button>
      </div>

      <section className="card prose-section">
        <h2>Get a block hash</h2>
        <pre className="mono-json">{`getblockhash <height>`}</pre>
        <p>
          Verify the hash in{" "}
          <Link to="/zero-trust-verification?mode=block">Block Verification</Link>. Advanced
          auditors can paste <code>getblock &lt;hash&gt; 2</code> JSON into Advanced Raw Block mode.
        </p>
      </section>
    </div>
  );
}
