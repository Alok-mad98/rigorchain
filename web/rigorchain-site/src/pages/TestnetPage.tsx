import { Link } from "react-router-dom";
import { Button } from "../components/Button";
import { TESTNET_DOWNLOAD_BASE } from "./DownloadPage";

export function TestnetPage() {
  return (
    <div className="page">
      <header className="page-header">
        <h1>RIGOR TESTNET</h1>
        <p className="muted">Public TESTNET user entry. Separate from future Mainnet pages.</p>
      </header>

      <section className="card prose-section banner-warning" aria-label="TESTNET warning">
        <p>
          <strong>TESTNET ONLY</strong>
        </p>
        <p>Test coins have no monetary value.</p>
        <p>Do not use a Mainnet wallet on TESTNET.</p>
        <p>Do not use TESTNET software on Mainnet.</p>
        <p>Do not use <code>rg1q</code> addresses.</p>
        <p>Do not connect TESTNET software to Mainnet.</p>
      </section>

      <section className="card prose-section" id="network" aria-labelledby="network-heading">
        <h2 id="network-heading">Network</h2>
        <p>
          Network:
          <br />
          RIGOR TESTNET
        </p>
        <p>
          Chain:
          <br />
          test
        </p>
        <p>
          Address:
          <br />
          trg1q...
        </p>
        <p>
          P2P:
          <br />
          18760
        </p>
        <p>
          How to connect:
          <br />
          Official TESTNET packages start on TESTNET and add the public P2P peers. You do not open RPC
          to any public host. Local wallet RPC stays on 127.0.0.1 only.
        </p>
        <p>
          Explorer:
          <br />
          <a href="http://explorer.rigorchain.com">explorer.rigorchain.com</a>
          {" "}
          (HTTP)
        </p>
      </section>

      <section className="card prose-section" id="faucet" aria-labelledby="faucet-heading">
        <h2 id="faucet-heading">Faucet</h2>
        <p>The faucet gives a small amount of TESTNET ORU so you can try receive, send, and Explorer.</p>
        <p>It is for testing. It is not mining. It is not a block reward.</p>
        <p>The faucet sends <strong>TESTNET ORU only</strong>. Claim amount: <strong>10 ORU</strong>.</p>
        <p>The faucet does not create RES. Do not expect RES from the faucet.</p>
        <p>
          RES Convergence is done inside the wallet, not by the faucet.
        </p>
        <p>
          Addresses must start with <code>trg1q</code>. Mainnet <code>rg1q</code> is rejected.
        </p>
        <p>
          <a href="/faucet/">Open the faucet</a>
        </p>
        <h3>Faucet limits</h3>
        <p>TESTNET only. Rate limited. No guarantee of availability. No economic value.</p>
      </section>

      <section className="card prose-section" id="wallet-guide" aria-labelledby="wallet-guide-heading">
        <h2 id="wallet-guide-heading">Wallet guide</h2>
        <ol>
          <li>
            Download the TESTNET wallet from{" "}
            <Link to="/download">the download page</Link> or{" "}
            <a href={`${TESTNET_DOWNLOAD_BASE}/`}>{`www.rigorchain.com${TESTNET_DOWNLOAD_BASE}/`}</a>
            .
          </li>
          <li>
            Verify SHA256. Download <a href={`${TESTNET_DOWNLOAD_BASE}/SHA256SUMS`}>SHA256SUMS</a>,
            then <code>sha256sum -c SHA256SUMS</code>.
          </li>
          <li>
            Install. Windows: run the testnet installer, then the wallet launcher. Linux: extract
            the testnet archive and start the wallet or node wrapper shipped at the top of the
            package. Do not point those programs at a public RPC host.
          </li>
          <li>Create a <code>trg1q</code> address. Do not generate or paste <code>rg1q</code>.</li>
          <li>
            Request faucet ORU at <a href="/faucet/">/faucet/</a> (10 ORU).
          </li>
          <li>Send a TESTNET transaction to a second <code>trg1q</code> address.</li>
          <li>
            Verify on Explorer: open{" "}
            <a href="http://explorer.rigorchain.com">explorer.rigorchain.com</a>, search the txid
            and addresses.
          </li>
        </ol>
        <p className="cta-row">
          <Button to="/download" variant="primary">
            Download TESTNET wallet
          </Button>
        </p>
      </section>

      <section className="card prose-section" id="miner-guide" aria-labelledby="miner-guide-heading">
        <h2 id="miner-guide-heading">Miner guide</h2>
        <p>TESTNET miner only. Do not use Mainnet miner packages on TESTNET.</p>
        <p>The miner needs:</p>
        <ul>
          <li>a local TESTNET node (the TESTNET wallet/node on the same machine)</li>
          <li>localhost RPC only (127.0.0.1:18761) — do not expose RPC publicly</li>
          <li>
            a <code>trg1q</code> coinbase address
          </li>
        </ul>
        <p>
          Use the testnet miner package from <Link to="/download">Download</Link>. The package
          expects a <code>trg1q</code> address and local RPC credentials. Do not aim the miner at
          a public host.
        </p>
        <p>Do not point the miner at a remote RPC port. Do not open 18761 on the internet.</p>
      </section>

      <section className="card prose-section" id="explorer" aria-labelledby="explorer-heading">
        <h2 id="explorer-heading">Explorer</h2>
        <p>
          <a href="http://explorer.rigorchain.com">http://explorer.rigorchain.com</a> is a read-only
          TESTNET viewer. Search a txid, address, or block height. It does not hold keys and does
          not submit transactions.
        </p>
        <p>HTTPS for Explorer is not enabled yet. Use HTTP for now.</p>
      </section>
    </div>
  );
}
