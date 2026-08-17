import { Link } from "react-router-dom";
import { SectionHub } from "../components/SectionHub";
import { SubNav } from "../components/SubNav";
import { SECTIONS } from "../ia/sitemap";

const SECTION = SECTIONS.find((s) => s.id === "wallet")!;

export function WalletPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Wallet</h1>
        <p className="muted">How to obtain a public TXID for independent verification.</p>
      </header>
      <SectionHub section={SECTION} />
      <section className="card prose-section">
        <p>The wallet is a spending tool. It is not an authority for consensus.</p>
        <h2>Get a transaction ID</h2>
        <pre className="mono-json">{`gettransaction <txid>
sendtoaddress  # returns the TXID`}</pre>
        <p>
          Paste that TXID into{" "}
          <Link to="/zero-trust-verification">Zero-Trust Verification</Link>. The browser recomputes
          ConvergenceRoot, Sigma, and winner selection locally.
        </p>
      </section>
    </div>
  );
}
