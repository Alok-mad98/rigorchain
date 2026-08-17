import { Link } from "react-router-dom";
import { SectionHub } from "../components/SectionHub";
import { SubNav } from "../components/SubNav";
import { SECTIONS } from "../ia/sitemap";

const SECTION = SECTIONS.find((s) => s.id === "mining")!;

export function MiningPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Mining</h1>
        <p className="muted">Miners and auditors verify blocks independently of the pool or node UI.</p>
      </header>
      <SectionHub section={SECTION} />
      <section className="card prose-section">
        <h2>Windows Miner ecosystem (planned)</h2>
        <p>
          AlphaPlus mining starts on Windows with a wallet address, a miner binary, and an optional
          pool. Existing installer scripts live in <code>installer/windows/</code> (
          <code>rigor-miner.cmd</code>, <code>start-wallet.cmd</code>). This page does not run miners.
        </p>
        <ul>
          <li>
            <Link to="/download/miner">Miner download</Link>
          </li>
          <li>
            <Link to="/mining/pool">Pool status</Link> (display only)
          </li>
          <li>
            <Link to="/mining/windows">Mining tutorial</Link>
          </li>
          <li>
            Wallet connection: receive ORU in <Link to="/wallet">Wallet</Link>
          </li>
        </ul>
      </section>
      <section className="card prose-section">
        <p>
          Mining discovers valid headers. RES allocation is a deterministic draw over the committed
          set F — not a pool policy.
        </p>
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
