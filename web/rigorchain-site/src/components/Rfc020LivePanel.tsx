import { useSystemState } from "../context/SystemStateContext";
import { readRfc020Live } from "../lib/rfc020Status";

type Props = {
  title?: string;
};

export function Rfc020LivePanel({ title = "This node" }: Props) {
  const { state } = useSystemState();
  const live = readRfc020Live(state.convergence_status);
  const explorerLive = state.explorer_status.reachable;
  const chain = state.rpc_status.reachable
    ? "reachable"
    : explorerLive
      ? "explorer observer (RPC not public)"
      : state.rpc_status.last_error ?? "RPC not configured";

  return (
    <section className="panel">
      <header className="panel-header">
        <h2>{title}</h2>
        <span className={`panel-state ${state.rpc_status.reachable || explorerLive ? "live" : "error"}`}>
          {state.rpc_status.reachable ? "LIVE" : explorerLive ? "LIVE" : "OFFLINE"}
        </span>
      </header>
      <p className="panel-intro">
        Convergence destroys ORU and may create RES in the same body transaction. This panel
        records chain observations: height, RES created, and ORU destroyed. It does not publish
        the issuance equation. The complete specification will be published with mainnet launch.
      </p>
      <dl className="kv-grid">
        <dt>Observer</dt>
        <dd className="mono">{chain}</dd>
        <dt>Tip height</dt>
        <dd className="mono">{live.tipHeight ?? "—"}</dd>
        <dt>Confirming height</dt>
        <dd className="mono">{live.confirmHeight ?? "—"}</dd>
        <dt>RES created (chain)</dt>
        <dd className="mono">{live.resTotal}</dd>
        <dt>ORU destroyed (chain)</dt>
        <dd className="mono">{live.cumulativeConvergence}</dd>
        <dt>Last RES height</dt>
        <dd className="mono">{live.lastResHeight ?? "—"}</dd>
      </dl>
      {(state.rpc_status.fetched_at || state.explorer_status.fetched_at) && (
        <p className="source-label">
          [{state.rpc_status.reachable ? "getconvergencestatus" : "explorer /api/status"} ·{" "}
          {state.rpc_status.fetched_at ?? state.explorer_status.fetched_at}]
        </p>
      )}
    </section>
  );
}
