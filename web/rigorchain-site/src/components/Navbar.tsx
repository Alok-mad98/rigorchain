import { Link, useLocation } from "react-router-dom";
import { useState } from "react";
import { PRIMARY_NAV } from "../ia/sitemap";

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
          {PRIMARY_NAV.map(({ to, label }) => (
            <Link
              key={to}
              to={to}
              className={isActive(to, pathname) ? "active" : ""}
              onClick={() => setOpen(false)}
            >
              {label}
            </Link>
          ))}
        </div>
      </nav>
    </header>
  );
}
