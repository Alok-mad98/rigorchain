import { explorerBaseUrl } from "../lib/explorerClient";
import { useSystemState } from "../context/SystemStateContext";

export function RESMintPanel() {
  const { state } = useSystemState();
  const mints = state.mint_state.res_mints;
  const explorerDown = !state.explorer_status.reachable;

  return (
    <section className="panel">
      <header className="panel-header">
        <h2>RES mint lifecycle</h2>
        <span className={`panel-state ${explorerDown ? "offline" : "live"}`}>
          {explorerDown ? "OFFLINE" : "LIVE"}
        </span>
      </header>

      {explorerDown && (
        <div className="banner banner-offline">
          Explorer failure: {state.explorer_status.last_error ?? state.mint_state.mint_error ?? "unreachable"}
        </div>
      )}

      <p className="panel-intro">Observer mode — mint list from explorer API only. rigord RPC mint status shown when available.</p>

      {state.mint_state.res_mint_status && (
        <details className="rpc-details">
          <summary>getresmintstatus (RPC)</summary>
          <pre className="mono-json">{JSON.stringify(state.mint_state.res_mint_status, null, 2)}</pre>
        </details>
      )}

      {!mints && !explorerDown && <p className="muted">Loading mint index…</p>}

      {mints && mints.length === 0 && (
        <p className="empty-state">No RES mints indexed (empty set — not mock data).</p>
      )}

      {mints && mints.length > 0 && (
        <table className="data-table">
          <thead>
            <tr>
              <th>txid</th>
              <th>amount</th>
              <th>height</th>
              <th>confirmations</th>
              <th>lineage</th>
            </tr>
          </thead>
          <tbody>
            {mints.map((m, i) => {
              const txid = String(m.txid ?? "");
              const base = explorerBaseUrl();
              return (
                <tr key={txid || `mint-row-${i}`}>
                  <td className="mono truncate">{txid || "—"}</td>
                  <td className="mono">{m.amount != null ? String(m.amount) : "—"}</td>
                  <td className="mono">{m.height != null ? String(m.height) : "—"}</td>
                  <td className="mono">{m.confirmations != null ? String(m.confirmations) : "—"}</td>
                  <td>
                    {txid && base ? (
                      <a href={`${base}/api/lineage/${txid}`} target="_blank" rel="noreferrer">
                        lineage
                      </a>
                    ) : (
                      "—"
                    )}
                  </td>
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
