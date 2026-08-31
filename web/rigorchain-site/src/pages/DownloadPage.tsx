import { Link } from "react-router-dom";
import { DarkCard } from "../components/DarkCard";
import { SectionTitle } from "../components/SectionTitle";
import { SubNav } from "../components/SubNav";

const DOWNLOADS = [
  {
    title: "Windows Wallet",
    body: "Official Qt wallet installer for Windows. Verify SHA256 before running.",
    to: "/download/windows-wallet",
  },
  {
    title: "Linux Wallet",
    body: "Node and wallet packages for Linux. Compare published checksums after download.",
    to: "/download/linux-wallet",
  },
  {
    title: "CLI Tools",
    body: "rigord and rigor-cli for operators and auditors.",
    to: "/download/cli",
  },
  {
    title: "Miner",
    body: "Windows and Linux SOLO miner (local GBT + submitblock). Not a pool client.",
    to: "/download/miner",
  },
] as const;

export function DownloadPage() {
  return (
    <div className="page">
      <SubNav />
      <header className="page-header">
        <h1>Download</h1>
        <p className="muted">Official RIGOR artifacts. This site does not execute binaries.</p>
      </header>

      <SectionTitle>Packages</SectionTitle>
      <section className="card-grid">
        {DOWNLOADS.map((item) => (
          <DarkCard key={item.to} title={item.title} to={item.to} linkLabel="Download">
            <p>{item.body}</p>
          </DarkCard>
        ))}
      </section>

      <section className="card prose-section">
        <p>
          Always compare published SHA256 sums. After install, confirm chain validity in the browser
          via <Link to="/zero-trust-verification">Zero-Trust Verification</Link>.
        </p>
        <ul>
          <li>
            Core daemon: <code>rigord</code> / <code>rigor-cli</code>
          </li>
          <li>
            Wallet: <code>rigor-wallet</code> (Qt)
          </li>
        </ul>
      </section>
    </div>
  );
}
