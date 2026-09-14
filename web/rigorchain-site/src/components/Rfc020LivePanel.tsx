import { useSystemState } from "../context/SystemStateContext";
import { readRfc020Live } from "../lib/rfc020Status";

type Props = {
  title?: string;
};

export function Rfc020LivePanel({ title = "This node" }: Props) {
  const { state } = useSystemState();
  const live = readRfc020Live(state.convergence_status);
  const chain = state.rpc_status.reachable
    ? "reachable"
    : state.rpc_status.last_error ?? "RPC not configured";

  return (
    <section className="panel">
      <header className="panel-header">
        <h2>{title}</h2>
        <span className={`panel-state ${state.rpc_status.reachable ? "live" : "error"}`}>
          {state.rpc_status.reachable ? "LIVE" : "OFFLINE"}
        </span>
      </header>
      <p className="panel-intro">
        Convergence destroys ORU and may mint RES in the same body transaction.
        Consensus admits the largest R such that R · (2²⁵ + h)² ≤ B ≪ 40 at the confirming height h.
        Display destroy_cost(h) is the continuous destruction cost to mint 1 RES; it is not fed back as B.
        A floor-priced mint left one block late is underpaid.
      </p>
      <dl className="kv-grid">
        <dt>RPC</dt>
        <dd className="mono">{chain}</dd>
        <dt>Tip height</dt>
        <dd className="mono">{live.tipHeight ?? "—"}</dd>
        <dt>Confirming height h</dt>
        <dd className="mono">{live.confirmHeight ?? "—"}</dd>
        <dt>Current destruction cost</dt>
        <dd className="mono">{live.destroyCostOru} ORU / 1 RES</dd>
        <dt>ORU destroyed · 1 RES atom</dt>
        <dd className="mono">{live.minAtomBurnOru} ORU</dd>
        <dt>ORU destroyed to mint 1 RES</dt>
        <dd className="mono">{live.minWholeResBurnOru} ORU</dd>
        <dt>Next-height destruction cost</dt>
        <dd className="mono">
          {live.destroyCostNextOru} ORU at height {live.nextHeight ?? "—"}
        </dd>
        <dt>RES minted (chain)</dt>
        <dd className="mono">{live.resTotal}</dd>
        <dt>ORU destroyed (chain)</dt>
        <dd className="mono">{live.cumulativeConvergence}</dd>
        <dt>Last RES height</dt>
        <dd className="mono">{live.lastResHeight ?? "—"}</dd>
      </dl>
      {state.rpc_status.fetched_at && (
        <p className="source-label">[getconvergencestatus · {state.rpc_status.fetched_at}]</p>
      )}
    </section>
  );
}
