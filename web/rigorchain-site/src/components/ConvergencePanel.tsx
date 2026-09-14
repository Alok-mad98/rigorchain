import { Rfc020LivePanel } from "./Rfc020LivePanel";
import { useSystemState } from "../context/SystemStateContext";

const MINT_ROWS = [
  { field: "locus", desc: "Body transaction: one OP_CONVERGE destroy + Witness v1 RES" },
  { field: "predicate", desc: "R · (2²⁵ + h)² ≤ B ≪ 40 at the confirming height h" },
  { field: "h", desc: "Confirming block height (tip + 1). Not floor(h / 210000)" },
  { field: "recipient", desc: "The minting transaction, not the miner" },
  { field: "maturity", desc: "Minted RES is spendable after 100 confirmations" },
  { field: "cap", desc: "RES supply ≤ 4,194,304 (2²² RES)" },
];

export function ConvergencePanel() {
  const { state } = useSystemState();

  return (
    <>
      <Rfc020LivePanel title="Convergence" />
      <section className="panel">
        <header className="panel-header">
          <h2>How a mint is admitted</h2>
        </header>
        <p className="panel-intro">
          Convergence destroys ORU via &lt;HASH160&gt; 0xbd and may mint Witness v1 RES in the same
          body transaction when the confirming-height inequality holds. Display destroy_cost(h) = 1024 ·
          (2²⁵ + h)² / 2⁵⁰ is ORU destroyed to mint 1 RES; it must not be used as B. R = 0 is a
          legal plain destroy.
        </p>
        <table className="data-table">
          <thead>
            <tr>
              <th>Component</th>
              <th>Role</th>
            </tr>
          </thead>
          <tbody>
            {MINT_ROWS.map((row) => (
              <tr key={row.field}>
                <td className="mono">{row.field}</td>
                <td>{row.desc}</td>
              </tr>
            ))}
          </tbody>
        </table>
        {state.rpc_status.fetched_at && (
          <p className="source-label">[rpc · {state.rpc_status.fetched_at}]</p>
        )}
      </section>
    </>
  );
}
