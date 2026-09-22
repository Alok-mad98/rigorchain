import {
  displayBlockConvergeBurn,
  displayBlockHash,
  displayBlockHeight,
  displayBlockOruMinted,
  oruFromExplorerStatus,
} from "../lib/explorerDisplay";
import { useSystemState } from "../context/SystemStateContext";

export function ORUSupplyPanel() {
  const { state } = useSystemState();
  const explorerDown = !state.explorer_status.reachable;
  const oru = oruFromExplorerStatus(state.explorer_snapshot);
  const blocks = state.recent_blocks;

  return (
    <section className="panel">
      <header className="panel-header">
        <h2>ORU supply (observer)</h2>
        <span className={`panel-state ${explorerDown ? "offline" : "live"}`}>
          {explorerDown ? "OFFLINE" : "LIVE"}
        </span>
      </header>

      {explorerDown && (
        <div className="banner banner-offline">
          Explorer failure: {state.explorer_status.last_error ?? "unreachable"}
        </div>
      )}

      <p className="panel-intro">
        Observer mode — ORU figures from explorer <code>/api/status</code> and recent blocks from{" "}
        <code>/api/blocks</code>. Issued, Convergence destroyed, fee destroy, and circulating
        spendable are separate series. This page is not a wallet balance.
      </p>

      {!oru && !explorerDown && <p className="muted">Loading ORU index…</p>}

      {oru && (
        <dl className="kv-grid">
          <dt>Tip height</dt>
          <dd className="mono">{oru.tipHeight}</dd>
          <dt>Confirming height</dt>
          <dd className="mono">{oru.confirmHeight}</dd>
          <dt>Issued ORU</dt>
          <dd className="mono">{oru.issued}</dd>
          <dt>Circulating spendable</dt>
          <dd className="mono">{oru.circulating}</dd>
          <dt>Mature transferable</dt>
          <dd className="mono">{oru.mature}</dd>
          <dt>Immature coinbase</dt>
          <dd className="mono">{oru.immature}</dd>
          <dt>Coinbase total (indexed)</dt>
          <dd className="mono">{oru.coinbaseTotal}</dd>
          <dt>Subsidy this block</dt>
          <dd className="mono">{oru.subsidyTip}</dd>
          <dt>Convergence destroyed</dt>
          <dd className="mono">{oru.convergenceDestroyed}</dd>
          <dt>Plain destroy (no RES)</dt>
          <dd className="mono">{oru.plainDestroy}</dd>
          <dt>RFC-018 fee destroy (cumulative)</dt>
          <dd className="mono">{oru.rfc018Cumulative}</dd>
          <dt>Unspent ORU UTXOs</dt>
          <dd className="mono">{oru.utxoCount}</dd>
        </dl>
      )}

      {blocks && blocks.length === 0 && (
        <p className="empty-state">No blocks indexed (empty set — not mock data).</p>
      )}

      {blocks && blocks.length > 0 && (
        <table className="data-table">
          <thead>
            <tr>
              <th>height</th>
              <th>hash</th>
              <th>ORU minted</th>
              <th>converge destroy</th>
              <th>tx</th>
            </tr>
          </thead>
          <tbody>
            {blocks.map((b, i) => {
              const hash = displayBlockHash(b);
              return (
                <tr key={hash || `oru-block-${i}`}>
                  <td className="mono">{displayBlockHeight(b)}</td>
                  <td className="mono truncate">{hash || "—"}</td>
                  <td className="mono">{displayBlockOruMinted(b)}</td>
                  <td className="mono">{displayBlockConvergeBurn(b)}</td>
                  <td className="mono">{b.tx_count ?? "—"}</td>
                </tr>
              );
            })}
          </tbody>
        </table>
      )}

      <SourceNote fetchedAt={state.explorer_status.fetched_at} />
    </section>
  );
}

function SourceNote({ fetchedAt }: { fetchedAt: string | null }) {
  if (!fetchedAt) return null;
  return <p className="source-label">[explorer · {fetchedAt}]</p>;
}
