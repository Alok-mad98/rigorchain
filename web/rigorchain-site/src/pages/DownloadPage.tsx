import { Link } from "react-router-dom";
import { SectionHub } from "../components/SectionHub";
import { SubNav } from "../components/SubNav";
import { SECTIONS } from "../ia/sitemap";

const SECTION = SECTIONS.find((s) => s.id === "download")!;

export function DownloadPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Download</h1>
        <p className="muted">Official AlphaPlus node and wallet artifacts. Verify checksums before use.</p>
      </header>
      <SectionHub section={SECTION} />
      <section className="card prose-section">
        <p>
          RIGOR AlphaPlus ships as signed/checksummed packages. This site does not execute binaries
          and does not substitute a download for independent consensus verification.
        </p>
        <ul>
          <li>Core daemon: <code>rigord</code> / <code>rigor-cli</code></li>
          <li>Wallet: <code>rigor-wallet</code> (Qt)</li>
          <li>Always compare published SHA256 sums after download</li>
        </ul>
        <p>
          After install, any third party can confirm chain validity in the browser via{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link> — no node RPC required
          for the default TXID / block-hash modes.
        </p>
      </section>
    </div>
  );
}
