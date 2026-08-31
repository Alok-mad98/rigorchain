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
          Test-phase mining is local SOLO: wallet address → rigor-miner → local rigord GBT →
          submitblock. The pool is not the test path.
        </p>
      </header>

      <SectionTitle>Mining Overview</SectionTitle>
      <section className="card prose-section">
        <p>
          Mining discovers valid headers (OpuHash / ASERT). RES allocation is a deterministic draw
          over committed set F — not a miner or pool policy. The shipped client copies the node
          template and grinds <code>nNonce</code> only.
        </p>
        <pre className="mono-json">{`wallet --coinbase-addr=rg1q…
        ↓
rigor-miner  +  local rigord
        ↓
getblocktemplate  →  nonce grind  →  submitblock
        ↓
ProcessNewBlock  →  P2P`}</pre>
        <p className="muted">
          Installer: <code>installer/windows/rigor-miner.cmd</code>. This page does not run miners.
        </p>
      </section>

      <section className="card-grid">
        <DarkCard title="Miner Download" to="/download/miner" linkLabel="Get miner">
          <p>Windows and Linux SOLO miner. Verify checksums, then pass a wallet <code>rg1q</code> address.</p>
        </DarkCard>
        <DarkCard title="Windows SOLO Guide" to="/mining/windows" linkLabel="Tutorial">
          <p>Wallet → local rigord → rigor-miner GBT → submitblock → verify the tip independently.</p>
        </DarkCard>
        <DarkCard title="Mining Economics" to="/mining/economics" linkLabel="Economics">
          <p>ORU subsidy 1000 → 500 → 250 → 125. No premine. RES is not mined.</p>
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
        <p className="muted">
          A quarantined Alpha pool observer exists at <Link to="/mining/pool">/mining/pool</Link>.
          It is not the test-phase mining path and is not a consensus authority.
        </p>
      </section>
    </div>
  );
}
