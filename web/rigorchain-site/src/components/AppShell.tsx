import { Link, Outlet, useLocation } from "react-router-dom";
import { useSystemState } from "../context/SystemStateContext";
import { VerificationNotice } from "./VerificationNotice";

const NAV = [
  { to: "/", label: "Home" },
  { to: "/protocol", label: "Protocol" },
  { to: "/science", label: "Science" },
  { to: "/lab", label: "Lab" },
  { to: "/status", label: "Status" },
  { to: "/experiment", label: "PROOF-034" },
];

function statusClass(s: string): string {
  switch (s) {
    case "online":
      return "status-verified";
    case "degraded":
      return "status-degraded";
    case "syncing":
      return "status-degraded";
    default:
      return "status-offline";
  }
}

export function AppShell() {
  const { pathname } = useLocation();
  const { state, refresh } = useSystemState();
  const equivFault = state.explorer_status.equivalence_claimed === true;

  return (
    <div className="app-shell">
      <header className="status-bar">
        <span className={`status-pill ${statusClass(state.global_status)}`}>
          {state.global_status.toUpperCase()}
        </span>
        <span className="status-mono">height: {state.chain_height ?? "—"}</span>
        <span className="status-mono">
          RPC: {state.rpc_status.reachable ? `${state.rpc_status.latency_ms}ms` : "offline"}
        </span>
        <span className="status-mono">
          Explorer: {state.explorer_status.reachable ? "OK" : "offline"}
        </span>
        <span className={`equiv-badge ${equivFault ? "equiv-fault" : ""}`}>
          equivalence_claimed: {String(state.explorer_status.equivalence_claimed ?? false)}
        </span>
        <button
          type="button"
          className="btn-refresh"
          onClick={() => void refresh()}
          disabled={state.refreshing}
        >
          {state.refreshing ? "…" : "↻"}
        </button>
      </header>

      <nav className="top-nav">
        <Link to="/" className="brand">
          RIGOR
        </Link>
        <div className="nav-links">
          {NAV.map(({ to, label }) => (
            <Link key={to} to={to} className={pathname === to ? "active" : ""}>
              {label}
            </Link>
          ))}
        </div>
      </nav>

      <VerificationNotice />

      <div className="read-only-banner" role="note">
        Experimental public narrative — read-only observer. No signing, no transaction submission, no
        consensus control. Deterministic execution system under continuous formal verification.
      </div>

      {state.rpc_status.last_error && state.global_status === "offline" && (
        <div className="banner banner-error" role="alert">
          RPC failure: {state.rpc_status.last_error}
        </div>
      )}

      {equivFault && (
        <div className="banner banner-error" role="alert">
          Constitutional fault: equivalence_claimed must remain false.
        </div>
      )}

      <main className="main-content">
        <Outlet />
      </main>

      <footer className="site-footer">
        <p>
          RIGOR is currently in live formal verification (PROOF-034). No equivalence claim is established.
          All runtime results are provisional.
        </p>
        <p className="footer-muted">
          equivalence_claimed = false · Not a finished mainnet system · Not an investment offering ·
          Native assets: ORU and RES
        </p>
        <p className="footer-muted">
          Build: {import.meta.env.VITE_SITE_VERSION ?? "dev"} · www.rigorchain.com
        </p>
      </footer>
    </div>
  );
}
