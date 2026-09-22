import { Rfc020LivePanel } from "./Rfc020LivePanel";
import { useSystemState } from "../context/SystemStateContext";

const MINT_ROWS = [
  { field: "Where", desc: "A body transaction destroys ORU and may create RES in that same transaction" },
  { field: "Rule", desc: "Consensus admits the operation. The public parameter sheet is reserved for mainnet" },
  { field: "Height", desc: "The confirming block height determines which rule instance applies" },
  { field: "Recipient", desc: "Created RES belongs to the transaction, not to the miner as a subsidy" },
  { field: "Spendability", desc: "Newly created RES becomes spendable after the protocol confirmation depth" },
  { field: "Scarcity", desc: "Issuance is bounded. Participation determines how much is actually created" },
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
          RES events display protocol-defined reserve operations. Each event records transparent
          ORU destruction and corresponding RES creation according to consensus rules. Destroying
          ORU without creating RES remains possible.
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
