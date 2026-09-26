import { Button } from "../components/Button";
import { SubNav } from "../components/SubNav";

/** Official website TESTNET hosting. Do not link private GitHub Releases. */
export const TESTNET_DOWNLOAD_BASE = "/downloads/testnet";

const ASSETS = [
  {
    href: `${TESTNET_DOWNLOAD_BASE}/rigor-wallet-v0.2-testnet-windows.exe`,
    label: "Windows wallet (TESTNET)",
    name: "rigor-wallet-v0.2-testnet-windows.exe",
  },
  {
    href: `${TESTNET_DOWNLOAD_BASE}/rigor-wallet-v0.2-testnet-linux-x86_64.tar.gz`,
    label: "Linux wallet (TESTNET)",
    name: "rigor-wallet-v0.2-testnet-linux-x86_64.tar.gz",
  },
  {
    href: `${TESTNET_DOWNLOAD_BASE}/rigor-miner-v0.2-testnet-windows-x86_64.exe`,
    label: "Windows miner (TESTNET)",
    name: "rigor-miner-v0.2-testnet-windows-x86_64.exe",
  },
  {
    href: `${TESTNET_DOWNLOAD_BASE}/rigor-miner-v0.2-testnet-windows-x86_64.zip`,
    label: "Windows miner zip (TESTNET)",
    name: "rigor-miner-v0.2-testnet-windows-x86_64.zip",
  },
  {
    href: `${TESTNET_DOWNLOAD_BASE}/rigor-miner-v0.2-testnet-linux-x86_64.tar.gz`,
    label: "Linux miner (TESTNET)",
    name: "rigor-miner-v0.2-testnet-linux-x86_64.tar.gz",
  },
] as const;

export function DownloadPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Download</h1>
        <p className="muted">
          Official TESTNET artifacts are hosted on this website. This site does not execute binaries.
          Full user journey: <a href="/testnet">/testnet</a>.
        </p>
      </header>

      <section className="card prose-section banner-warning" aria-label="TESTNET warning">
        <p>
          <strong>TESTNET ONLY</strong>
        </p>
        <p>Test coins have no monetary value.</p>
        <p>Do not use TESTNET software on Mainnet.</p>
        <p>Do not use Mainnet software on TESTNET.</p>
        <p>Testnet addresses start with trg1q. The public peer port is 18760.</p>
        <p>Mainnet software is not published here. Do not mix mainnet and testnet wallets.</p>
      </section>

      <section className="card prose-section" id="testnet" aria-labelledby="testnet-heading">
        <h2 id="testnet-heading">RIGOR TESTNET</h2>
        <p>
          Status:
          <br />
          Public Testnet
        </p>
        <p>
          Release:
          <br />
          RIGOR Public Test Network
        </p>
        <p>
          Network:
          <br />
          TESTNET
        </p>
        <p>
          Chain:
          <br />
          test
        </p>
        <p>
          Address:
          <br />
          trg1q
        </p>
        <p>
          P2P:
          <br />
          18760
        </p>
        <p>
          Public TESTNET peers (first start):
          <br />
          Official TESTNET packages add the public P2P peers. You do not open RPC to any public host.
        </p>
        <p>
          Explorer:
          <br />
          <a href="http://explorer.rigorchain.com">explorer.rigorchain.com</a>
          {" "}
          (MAINNET)
        </p>
        <p>
          Faucet:
          <br />
          <a href="/faucet/">www.rigorchain.com/faucet/</a>
          {" "}
          (10 ORU to a trg1q address)
        </p>
        <p>
          Verify download:
          <br />
          Download SHA256SUMS, then <code>sha256sum -c SHA256SUMS</code>
        </p>
        <p>
          Download source:
          <br />
          Official RIGOR website
        </p>
        <p>
          Download:
          <br />
          <a href={`${TESTNET_DOWNLOAD_BASE}/`}>{`www.rigorchain.com${TESTNET_DOWNLOAD_BASE}/`}</a>
        </p>
        <p>TESTNET-labeled packages:</p>
        <ul>
          {ASSETS.map((a) => (
            <li key={a.name}>
              <a href={a.href}>{a.label}</a>
            </li>
          ))}
        </ul>
        <p>
          Compare <a href={`${TESTNET_DOWNLOAD_BASE}/SHA256SUMS`}>SHA256SUMS</a> before running
          anything. Read <a href={`${TESTNET_DOWNLOAD_BASE}/TESTNET-README.txt`}>TESTNET-README.txt</a>
          {", "}
          <a href={`${TESTNET_DOWNLOAD_BASE}/MINER-TESTNET-README.txt`}>SOLO mining (EN)</a>
          {", "}
          <a href={`${TESTNET_DOWNLOAD_BASE}/MINER-TESTNET-README-CN.txt`}>SOLO mining (中文)</a>
          {", and "}
          <a href={`${TESTNET_DOWNLOAD_BASE}/RELEASE-NOTES.txt`}>Release Notes</a>.
        </p>
        <p className="cta-row">
          <Button href={`${TESTNET_DOWNLOAD_BASE}/`} variant="primary">
            Official TESTNET downloads
          </Button>
        </p>
      </section>

      <section className="card prose-section" id="mainnet" aria-labelledby="mainnet-heading">
        <h2 id="mainnet-heading">MAINNET</h2>
        <p>
          Status:
          <br />
          Coming soon
        </p>
        <p>No Mainnet wallet available yet.</p>
      </section>
    </div>
  );
}
