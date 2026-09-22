import { explorerBaseUrl } from "../lib/explorerClient";
import { displayMintAmount, displayMintHeight, displayMintTxid } from "../lib/explorerDisplay";
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

      <p className="panel-intro">
        Observer mode. Each row is RES created on chain. This list does not authorize creation.
      </p>

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
              const txid = displayMintTxid(m);
              const base = explorerBaseUrl();
              const heightLabel = displayMintHeight(m);
              const heightNum = Number(heightLabel);
              const tip = state.chain_height;
              const confirmations =
                m.confirmations != null
                  ? String(m.confirmations)
                  : tip != null && Number.isFinite(heightNum)
                    ? String(tip - heightNum + 1)
                    : "—";
              return (
                <tr key={txid || `mint-row-${i}`}>
                  <td className="mono truncate">{txid || "—"}</td>
                  <td className="mono">{displayMintAmount(m)}</td>
                  <td className="mono">{heightLabel}</td>
                  <td className="mono">{confirmations}</td>
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
