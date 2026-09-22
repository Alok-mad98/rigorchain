import { useSystemState } from "../context/SystemStateContext";
import { oruFromExplorerStatus } from "../lib/explorerDisplay";
import { readRfc020Live } from "../lib/rfc020Status";
export type ProtocolCardVariant = "overview" | "oru" | "res" | "forge" | "live";

type Props = {
  variant: ProtocolCardVariant;
};

export function ProtocolCard({ variant }: Props) {
  const { state } = useSystemState();

  if (variant === "overview") {
    return (
      <article className="card">
        <header className="card-header">
          <span className="tag">SPECIFICATION</span>
          <h3>System overview</h3>
        </header>
        <p>
          RIGOR is a UTXO-based deterministic state machine: S<sub>t+1</sub> = F(S<sub>t</sub>, T<sub>t</sub>).
          No global account state. Script is the compliance/classification layer: ORU/RES class from
          scriptPubKey; legitimacy via consensus. Script alone does not authorize asset creation.
        </p>
      </article>
    );
  }

  if (variant === "oru") {
    const oru = oruFromExplorerStatus(state.explorer_snapshot);
    return (
      <article className="card">
        <header className="card-header">
          <span className="tag">ORU</span>
          <h3>Execution &amp; fee resource</h3>
        </header>
        <p className="math-block">Σ ORU<sub>in</sub> = Σ ORU<sub>out</sub> + fee</p>
        <p>ORU is issued as a fixed per-block subsidy after genesis. There is no premine.</p>
        <LiveStrip
          height={state.chain_height}
          label="chain height"
          source={state.rpc_status.reachable ? "rpc" : "explorer"}
        />
        {oru && (
          <p className="live-strip mono">
            issued {oru.issued} · circulating {oru.circulating} · subsidy {oru.subsidyTip}{" "}
            <span className="muted">(explorer)</span>
          </p>
        )}
      </article>
    );
  }

  if (variant === "res") {
    return (
      <article className="card">
        <header className="card-header">
          <span className="tag">RES</span>
          <h3>Scarce settlement asset</h3>
        </header>
        <p className="math-block">Σ RES<sub>in</sub> ≥ Σ RES<sub>out</sub></p>
        <p>
          Reserve asset created through Convergence. Circulation depends on participation. RES is
          not mined. The complete issuance specification will be published with mainnet launch.
        </p>
        <MintLiveSummary count={state.mint_state.res_mints?.length ?? null} error={state.mint_state.mint_error} />
      </article>
    );
  }

  if (variant === "forge") {
    return (
      <article className="card">
        <header className="card-header">
          <span className="tag">CONVERGENCE</span>
          <h3>Convergence pathway</h3>
        </header>
        <p>
          Convergence destroys ORU and may create RES in the same body transaction when consensus
          admits the operation. RES is not mined.
        </p>
        <p className="muted">
          RES issuance follows deterministic protocol rules designed to balance participation,
          scarcity, and long-term reserve behavior. The complete specification will be published
          with mainnet launch.
        </p>
        <ForgeStatus
          convergence={state.convergence_status}
          source={state.rpc_status.reachable ? "rpc" : "explorer"}
        />
      </article>
    );
  }

  /* live aggregate */
  return (
    <article className="card">
      <header className="card-header">
        <span className="tag">LIVE</span>
        <h3>Protocol observables</h3>
      </header>
      <dl className="kv-grid">
        <dt>Chain height</dt>
        <dd className="mono">{state.chain_height ?? "—"}</dd>
        <dt>Best block</dt>
        <dd className="mono truncate">{state.best_block_hash ?? "—"}</dd>
        <dt>Peers</dt>
        <dd className="mono">{state.network_info?.connections ?? "—"}</dd>
        <dt>IBD</dt>
        <dd className="mono">{String(state.initial_block_download ?? "—")}</dd>
        <dt>RES mints (explorer)</dt>
        <dd className="mono">{state.mint_state.res_mints?.length ?? "—"}</dd>
      </dl>
      <SourceNote fetchedAt={state.rpc_status.fetched_at} source="rpc" />
    </article>
  );
}

function LiveStrip({
  height,
  label,
  source,
}: {
  height: number | null;
  label: string;
  source: string;
}) {
  return (
    <p className="live-strip mono">
      {label}: {height ?? "—"} <span className="muted">({source})</span>
    </p>
  );
}

function MintLiveSummary({ count, error }: { count: number | null; error: string | null }) {
  if (error) {
    return <p className="degraded-text">Explorer mint list: {error}</p>;
  }
  return (
    <p className="live-strip mono">
      indexed mints: {count ?? "—"} <span className="muted">(explorer)</span>
    </p>
  );
}

function ForgeStatus({
  convergence,
  source,
}: {
  convergence: Record<string, unknown> | null;
  source: string;
}) {
  if (!convergence) {
    return <p className="degraded-text">Convergence status unavailable</p>;
  }
  const live = readRfc020Live(convergence);
  return (
    <p className="live-strip mono">
      tip {live.tipHeight ?? "—"} · confirming height {live.confirmHeight ?? "—"} · ORU
      destroyed and RES created are recorded on chain <span className="muted">({source})</span>
    </p>
  );
}

function SourceNote({ fetchedAt, source }: { fetchedAt: string | null; source: string }) {
  if (!fetchedAt) return null;
  return (
    <p className="source-label">
      [{source} · {fetchedAt}]
    </p>
  );
}
