import { Link } from "react-router-dom";
import { COQ_ENFORCED_VECTORS, SHARED_GOLDEN_VECTORS } from "../protocolFacts";

const FOOTER_LINKS = [
  { to: "/download", label: "Download" },
  { to: "/documentation", label: "Documentation" },
  { to: "/documentation/whitepaper", label: "Whitepaper" },
  { to: "/documentation/security", label: "Security Architecture" },
  { to: "/documentation/formal-verification", label: "Formal Verification" },
  { to: "/about", label: "About" },
] as const;

export function Footer() {
  return (
    <footer className="site-footer">
      <div className="site-footer-inner">
        <nav className="footer-nav" aria-label="Footer">
          {FOOTER_LINKS.map((item) => (
            <Link key={item.to} to={item.to}>
              {item.label}
            </Link>
          ))}
        </nav>
        <p>
          Read-only observer. No signing, no transaction submission, no consensus control.
          Trust is established by independent verification of public bytes, not by this website.
        </p>
        <p className="footer-muted">
          Coq model properties · {SHARED_GOLDEN_VECTORS} shared golden vectors ·{" "}
          {COQ_ENFORCED_VECTORS} Coq-enforced · equivalence_claimed = false · Not a refinement
          proof
        </p>
        <p className="footer-muted">
          Not an investment offering · Native assets: ORU and RES
        </p>
        <p className="footer-muted">
          Build: {import.meta.env.VITE_SITE_VERSION ?? "dev"} · www.rigorchain.com
        </p>
      </div>
    </footer>
  );
}
