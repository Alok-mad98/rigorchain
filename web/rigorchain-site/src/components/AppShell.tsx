import { Outlet } from "react-router-dom";
import { useSystemState } from "../context/SystemStateContext";
import { Footer } from "./Footer";
import { Navbar } from "./Navbar";
import { StatusBadge } from "./StatusBadge";
import { VerificationNotice } from "./VerificationNotice";

function statusTone(s: string): "ok" | "warn" | "err" {
  if (s === "online") return "ok";
  if (s === "degraded" || s === "syncing") return "warn";
  return "err";
}

export function AppShell() {
  const { state, refresh } = useSystemState();
  const equivFault = state.explorer_status.equivalence_claimed === true;

  return (
    <div className="app-shell">
      <div className="status-bar">
        <StatusBadge label={state.global_status.toUpperCase()} tone={statusTone(state.global_status)} />
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
      </div>

      <Navbar />
      <VerificationNotice />

      <div className="read-only-banner" role="note">
        Read-only observer. No signing, no transaction submission, no consensus control.
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

      <Footer />
    </div>
  );
}
