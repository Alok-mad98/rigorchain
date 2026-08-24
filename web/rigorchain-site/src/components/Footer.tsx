import { Link } from "react-router-dom";

const FOOTER_LINKS = [
  { to: "/download", label: "Download" },
  { to: "/documentation", label: "Documentation" },
  { to: "/documentation/whitepaper", label: "Whitepaper" },
  { to: "/documentation/security", label: "Security Architecture" },
  { to: "/documentation/formal-verification", label: "Formal Verification" },
  { to: "/zero-trust-verification", label: "Zero-Trust" },
  { to: "/about", label: "About" },
] as const;

export function Footer() {
  return (
    <footer className="site-footer">
      <div className="site-footer-inner">
        <nav className="footer-nav" aria-label="Footer">
          <img src="/assets/rigor-mark.png" alt="" className="footer-mark" width={28} height={28} />
          {FOOTER_LINKS.map((item) => (
            <Link key={item.to} to={item.to}>
              {item.label}
            </Link>
          ))}
        </nav>
        <p>
          RIGOR is a read-only observer. Trust is established by independent verification
          of public bytes and reproducible artifacts, not by this website.
        </p>
        <p className="footer-muted">
          equivalence_claimed = false · Not an investment offering · Native assets: ORU and RES
        </p>
        <p className="footer-muted">
          Build: {import.meta.env.VITE_SITE_VERSION ?? "dev"} · www.rigorchain.com
        </p>
      </div>
    </footer>
  );
}
