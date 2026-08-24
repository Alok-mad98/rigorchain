import { useSystemState } from "../context/SystemStateContext";

const PCORB_ROWS = [
  { field: "Layout", desc: "Frozen ORB body structure per RES rules" },
  { field: "Ed25519", desc: "Signature over frozen digest domain" },
  { field: "convergence_txid", desc: "Binds mint to winner context" },
  { field: "cap gate", desc: "RES supply cap at milestone heights" },
];

export function ConvergencePanel() {
  const { state } = useSystemState();
  const result = state.convergence_result;
  const status = state.convergence_status;
  const degraded = !result && state.rpc_status.reachable;

  return (
    <section className="panel">
      <header className="panel-header">
        <h2>Convergence</h2>
        <span className={`panel-state ${degraded ? "degraded" : state.rpc_status.reachable ? "live" : "error"}`}>
          {degraded ? "DEGRADED" : state.rpc_status.reachable ? "LIVE" : "OFFLINE"}
        </span>
      </header>

      {!state.rpc_status.reachable && (
        <div className="banner banner-error">RPC failure — convergence data unavailable</div>
      )}

      <p className="panel-intro">
        Deterministic selection over candidate set F. Eligibility uses integer comparison H &lt; T·f.
        Deterministic Reserve Issuance: E = 246, C = 1024 ORU per RES mint (expected wins ≈ n / 1024).
        Historical RFC-014 / retired design 1/65536 is not the shipping identity.
      </p>

      <dl className="kv-grid">
        <dt>|F|</dt>
        <dd className="mono">{field(result, "candidate_set_size", "convergence_root_set_size")}</dd>
        <dt>root</dt>
        <dd className="mono truncate">{field(result, "convergence_root", "root")}</dd>
        <dt>sigma (σ)</dt>
        <dd className="mono truncate">{field(result, "sigma")}</dd>
        <dt>winner txid</dt>
        <dd className="mono truncate">{field(result, "winner_txid", "winner")}</dd>
        <dt>res_probability_target</dt>
        <dd className="mono truncate">{field(result, "res_probability_target")}</dd>
        <dt>tip (status)</dt>
        <dd className="mono">{field(status, "tip_height", "height")}</dd>
      </dl>

      <h3 className="subheading">PCORB verification vector</h3>
      <table className="data-table">
        <thead>
          <tr>
            <th>Component</th>
            <th>Role</th>
          </tr>
        </thead>
        <tbody>
          {PCORB_ROWS.map((row) => (
            <tr key={row.field}>
              <td className="mono">{row.field}</td>
              <td>{row.desc}</td>
            </tr>
          ))}
        </tbody>
      </table>

      {degraded && (
        <p className="degraded-text">getconvergenceresult unavailable — partial RPC response</p>
      )}

      <SourceNote fetchedAt={state.rpc_status.fetched_at} />
    </section>
  );
}

function field(obj: Record<string, unknown> | null, ...keys: string[]): string {
  if (!obj) return "—";
  for (const k of keys) {
    const v = obj[k];
    if (v !== undefined && v !== null && v !== "") return String(v);
  }
  return "—";
}

function SourceNote({ fetchedAt }: { fetchedAt: string | null }) {
  if (!fetchedAt) return null;
  return <p className="source-label">[rpc · {fetchedAt}]</p>;
}
