import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";
import {
  ASERT_ANCHOR_HEIGHT,
  ASERT_CLAMP,
  ASERT_HALF_LIFE_S,
  MAINNET_HRP,
  MAINNET_RPC_PORT,
  OPUHASH,
  TARGET_SPACING_S,
} from "../protocolFacts";

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
      </header>

      <SectionTitle>Mining Overview</SectionTitle>
      <section className="card prose-section">
        <p>
          Consensus PoW is <strong>OpuHash</strong>:
        </p>
        <pre className="math-block">{`digest = ${OPUHASH}
valid  ⇔  digest ≤ ASERT target`}</pre>
        <p>
          ASERT (aserti3-2d): spacing {TARGET_SPACING_S}s, half-life {ASERT_HALF_LIFE_S}s (
          {ASERT_HALF_LIFE_S / 3600} hours), anchor height {ASERT_ANCHOR_HEIGHT}, per-block clamp{" "}
          {ASERT_CLAMP}. Block id remains SHA256d of the 80-byte header. The retired 64 KiB mix in{" "}
          <code>pow_opus.cpp</code> is not OpuHash.
        </p>
        <p>
          RES is not a PoW reward. RES mint is a body-tx destroy at <code>destroy_cost(h)</code>; the miner
          receives ORU subsidy. The shipped <code>minerd</code> grinds the same digest as{" "}
          <code>CheckProofOfWork</code> (<code>-a rgrpow01</code>). SOLO copies canonical GBT
          coinbase bytes from <code>rigord</code> on RPC {MAINNET_RPC_PORT} and grinds{" "}
          <code>nNonce</code> / time only.
        </p>
        <pre className="mono-json">{`wallet --coinbase-addr=${MAINNET_HRP}1q…
        ↓
minerd  +  local rigord (RPC ${MAINNET_RPC_PORT})
        ↓
getblocktemplate  →  copy coinbase  →  nonce grind  →  submitblock
        ↓
ProcessNewBlock  →  CheckProofOfWork  →  ConnectBlock`}</pre>
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
          <p>ORU subsidy 128 per block forever. No premine. RES is not mined.</p>
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
          Observe the hash in <Link to="/explorer">Explorer</Link>. Advanced auditors can inspect{" "}
          <code>getblock &lt;hash&gt; 2</code> JSON from a local node.
        </p>
        <p className="muted">
          A quarantined pool observer exists at <Link to="/mining/pool">/mining/pool</Link>. It is
          not the SOLO mining path and is not a consensus authority.
        </p>
      </section>
    </div>
  );
}
