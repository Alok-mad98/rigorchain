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
          Mining is local SOLO: wallet address → rigor-miner → local rigord GBT → submitblock. The
          pool observer is not the mining path.
        </p>
        <p className="banner-warning">
          <strong>TESTNET ONLY</strong> for current public downloads. Coinbase must be{" "}
          <code>trg1q</code>. Local RPC stays on 127.0.0.1. Do not use mainnet software on the
          test network.
        </p>
      </header>

      <SectionTitle>Mining Overview</SectionTitle>
      <section className="card prose-section">
        <p>
          Consensus proof-of-work is <strong>OpuHash</strong>. Difficulty adjustment is ASERT.
          Numerical parameters will be published with the mainnet specification.
        </p>
        <p>
          RES is not a proof-of-work reward. The miner receives the ORU subsidy. RES is created
          only when a body transaction destroys ORU under consensus rules.
        </p>
        <p>
          Public mining on this site is solo on the test network: a local node, a testnet coinbase
          address, and the official miner package. Do not point the miner at a public RPC port.
          This page does not run miners.
        </p>
      </section>

      <section className="card-grid">
        <DarkCard title="Miner Download" to="/download" linkLabel="Get miner">
          <p>
            Current public downloads are TESTNET. Verify SHA256SUMS, then pass a wallet{" "}
            <code>trg1q</code> address. Do not use <code>rg1q</code> or port 8760 on TESTNET.
          </p>
        </DarkCard>
        <DarkCard title="Windows SOLO Guide" to="/mining/windows" linkLabel="Tutorial">
          <p>Wallet → local rigord → rigor-miner GBT → submitblock → verify the tip independently.</p>
        </DarkCard>
        <DarkCard title="Mining Economics" to="/mining/economics" linkLabel="Economics">
          <p>Miners receive a fixed ORU subsidy. There is no premine. RES is not mined.</p>
        </DarkCard>
      </section>

      <div className="cta-row">
        <Button to="/download">Download miner</Button>
        <Button to="/wallet">Connect wallet</Button>
      </div>

      <section className="card prose-section">
        <h2>Get a block hash</h2>
        <pre className="mono-json">{`getblockhash <height>`}</pre>
        <p>
          Observe the hash in <Link to="/explorer">Explorer</Link>. Advanced auditors can inspect{" "}
          <code>getblock &lt;hash&gt; 2</code> JSON from a local node.
        </p>
        <p className="muted">
          A separate pool view is listed at <Link to="/mining/pool">pool</Link>. It is not the
          solo mining path and is not a consensus authority.
        </p>
      </section>
    </div>
  );
}
