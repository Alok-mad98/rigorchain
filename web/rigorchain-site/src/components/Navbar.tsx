import { Link, useLocation } from "react-router-dom";
import { useState } from "react";
import { LAB_NAV, PRIMARY_NAV } from "../ia/sitemap";

function isActive(to: string, pathname: string): boolean {
  if (to === "/") return pathname === "/";
  return pathname === to || pathname.startsWith(`${to}/`);
}

export function Navbar() {
  const { pathname } = useLocation();
  const [open, setOpen] = useState(false);

  return (
    <header className={`site-header${open ? " nav-open" : ""}`}>
      <nav className="top-nav" aria-label="Primary">
        <Link to="/" className="brand" onClick={() => setOpen(false)}>
          <img src="/assets/rigor-mark.png" alt="RIGOR" className="brand-mark" width={36} height={36} />
          <span className="brand-text">RIGOR</span>
        </Link>
        <button
          type="button"
          className="nav-toggle"
          aria-expanded={open}
          aria-controls="primary-nav"
          onClick={() => setOpen((v) => !v)}
        >
          {open ? "Close" : "Menu"}
        </button>
        <div id="primary-nav" className="nav-links">
          {PRIMARY_NAV.map(({ to, label }) => {
            if (to === "/documentation") {
              const onSecurity = pathname.startsWith("/documentation/security");
              if (onSecurity) {
                return (
                  <span key={to} className="nav-cluster">
                    <Link to="/documentation" onClick={() => setOpen(false)}>
                      Documentation
                    </Link>
                    <span className="nav-cluster-sep" aria-hidden="true">
                      →
                    </span>
                    <Link
                      to="/documentation/security"
                      className={pathname === "/documentation/security" ? "active" : ""}
                      onClick={() => setOpen(false)}
                    >
                      Security Architecture
                    </Link>
                    <span className="nav-cluster-sep" aria-hidden="true">
                      →
                    </span>
                    <Link
                      to="/documentation/security/minimal-script-architecture"
                      className={
                        pathname === "/documentation/security/minimal-script-architecture"
                          ? "active"
                          : ""
                      }
                      onClick={() => setOpen(false)}
                    >
                      Minimal Script Architecture
                    </Link>
                  </span>
                );
              }
              const onDocs =
                isActive("/documentation", pathname) &&
                pathname !== "/documentation/whitepaper" &&
                pathname !== "/documentation/formal-verification";
              return (
                <span key={to} className="nav-cluster">
                  <Link
                    to="/documentation"
                    className={onDocs ? "active" : ""}
                    onClick={() => setOpen(false)}
                  >
                    Documentation
                  </Link>
                  <span className="nav-cluster-sep" aria-hidden="true">
                    →
                  </span>
                  <Link
                    to="/documentation/whitepaper"
                    className={pathname === "/documentation/whitepaper" ? "active" : ""}
                    onClick={() => setOpen(false)}
                  >
                    Whitepaper
                  </Link>
                  <span className="nav-cluster-sep" aria-hidden="true">
                    →
                  </span>
                  <Link
                    to="/documentation/formal-verification"
                    className={pathname === "/documentation/formal-verification" ? "active" : ""}
                    onClick={() => setOpen(false)}
                  >
                    Formal Verification
                  </Link>
                </span>
              );
            }
            return (
              <Link
                key={to}
                to={to}
                className={isActive(to, pathname) ? "active" : ""}
                onClick={() => setOpen(false)}
              >
                {label}
              </Link>
            );
          })}
        </div>
      </nav>
      <nav className="lab-nav" aria-label="Protocol lab">
        {LAB_NAV.map(({ to, label }) => (
          <Link
            key={to}
            to={to}
            className={pathname === to ? "active" : ""}
            onClick={() => setOpen(false)}
          >
            {label}
          </Link>
        ))}
      </nav>
    </header>
  );
}
