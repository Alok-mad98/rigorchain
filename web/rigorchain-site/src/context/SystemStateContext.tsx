import {
  createContext,
  useCallback,
  useContext,
  useEffect,
  useMemo,
  useReducer,
  type ReactNode,
} from "react";
import * as explorer from "../lib/explorerClient";
import {
  convergenceFromExplorerStatus,
  explorerBestHash,
  explorerTipHeight,
} from "../lib/explorerDisplay";
import {
  getblockchaininfo,
  getconvergencestatus,
  getconvergenceresult,
  getnetworkinfo,
  getresmintstatus,
  isRpcConfigured,
  type BlockchainInfo,
  type NetworkInfo,
} from "../../lib/rigordRpcClient";
import { isExplorerConfigured } from "../lib/explorerClient";

export type ConnectionStatus = "online" | "degraded" | "offline" | "syncing";

export type SystemState = {
  chain_height: number | null;
  best_block_hash: string | null;
  initial_block_download: boolean | null;
  network_info: NetworkInfo | null;
  convergence_status: Record<string, unknown> | null;
  convergence_result: Record<string, unknown> | null;
  rpc_status: {
    reachable: boolean;
    latency_ms: number | null;
    last_error: string | null;
    fetched_at: string | null;
  };
  explorer_status: {
    reachable: boolean;
    last_error: string | null;
    fetched_at: string | null;
    equivalence_claimed: boolean | null;
  };
  mint_state: {
    res_mints: explorer.ResMintRow[] | null;
    res_mint_status: Record<string, unknown> | null;
    mint_error: string | null;
  };
  explorer_snapshot: explorer.ApiStatus | null;
  recent_blocks: explorer.ExplorerBlockRow[] | null;
  verification_state: {
    proof034_phase: string;
    p03_label: string;
    gv_materialized: number | null;
    gv_total: number;
  };
  global_status: ConnectionStatus;
  refreshing: boolean;
};

type Action =
  | { type: "REFRESH_START" }
  | {
      type: "REFRESH_DONE";
      payload: Omit<SystemState, "refreshing" | "global_status"> & {
        global_status: ConnectionStatus;
      };
    };

const GV_TOTAL = 12;

const initialVerification = {
  proof034_phase: "IN_PROGRESS",
  p03_label: "P03_PARTIAL",
  gv_materialized: 9,
  gv_total: GV_TOTAL,
};

function deriveGlobalStatus(
  rpcReachable: boolean,
  ibd: boolean | null,
  explorerReachable: boolean,
  equivFault: boolean,
  rpcConfigured: boolean,
  explorerConfigured: boolean,
): ConnectionStatus {
  if (rpcConfigured && !rpcReachable) return "offline";
  if (!rpcConfigured && !explorerConfigured) return "offline";
  if (ibd === true) return "syncing";
  if (equivFault || (explorerConfigured && !explorerReachable)) return "degraded";
  if (!rpcConfigured && explorerReachable) return "online";
  return "online";
}

function reducer(state: SystemState, action: Action): SystemState {
  switch (action.type) {
    case "REFRESH_START":
      return { ...state, refreshing: true };
    case "REFRESH_DONE":
      return { ...action.payload, refreshing: false };
    default:
      return state;
  }
}

const initialState: SystemState = {
  chain_height: null,
  best_block_hash: null,
  initial_block_download: null,
  network_info: null,
  convergence_status: null,
  convergence_result: null,
  rpc_status: {
    reachable: false,
    latency_ms: null,
    last_error: null,
    fetched_at: null,
  },
  explorer_status: {
    reachable: false,
    last_error: null,
    fetched_at: null,
    equivalence_claimed: null,
  },
  mint_state: {
    res_mints: null,
    res_mint_status: null,
    mint_error: null,
  },
  explorer_snapshot: null,
  recent_blocks: null,
  verification_state: initialVerification,
  global_status: "offline",
  refreshing: false,
};

type ContextValue = {
  state: SystemState;
  refresh: () => Promise<void>;
};

const SystemStateContext = createContext<ContextValue | null>(null);

export function SystemStateProvider({ children }: { children: ReactNode }) {
  const [state, dispatch] = useReducer(reducer, initialState);

  const refresh = useCallback(async () => {
    dispatch({ type: "REFRESH_START" });

    const rpcConfigured = isRpcConfigured();
    const explorerConfigured = isExplorerConfigured();

    const [chainRes, netRes, convRes, convResultRes, mintRpcRes, statusRes, mintsRes, blocksRes] =
      await Promise.all([
        getblockchaininfo(),
        getnetworkinfo(),
        getconvergencestatus(),
        getconvergenceresult(),
        getresmintstatus(),
        explorer.getStatus(),
        explorer.getResMints(),
        explorer.getBlocks(25),
      ]);

    const chain = chainRes.result as BlockchainInfo | null;
    const rpcReachable = rpcConfigured && chainRes.error === null && chain !== null;
    const explorerReachable =
      explorerConfigured && statusRes.error === null && statusRes.body !== null;

    const equivalence_claimed = statusRes.body?.equivalence_claimed ?? null;
    const equivFault = equivalence_claimed === true;

    const ibd = chain?.initialblockdownload ?? null;

    const global_status = deriveGlobalStatus(
      rpcReachable,
      ibd,
      explorerReachable,
      equivFault,
      rpcConfigured,
      explorerConfigured,
    );

    let manifestGv: number | null = null;
    const manifestUrl = import.meta.env.VITE_VERIFICATION_MANIFEST_URL?.trim();
    if (manifestUrl) {
      try {
        const m = await fetch(manifestUrl);
        if (m.ok) {
          const j = (await m.json()) as { established?: number };
          if (typeof j.established === "number") manifestGv = j.established;
        }
      } catch {
        /* leave null — no silent substitute */
      }
    }

    const explorerConv = convergenceFromExplorerStatus(statusRes.body);

    dispatch({
      type: "REFRESH_DONE",
      payload: {
        chain_height: chain?.blocks ?? explorerTipHeight(statusRes.body),
        best_block_hash: chain?.bestblockhash ?? explorerBestHash(statusRes.body),
        initial_block_download: ibd,
        network_info: netRes.result,
        convergence_status: convRes.result ?? explorerConv,
        convergence_result: convResultRes.error ? null : convResultRes.result,
        rpc_status: {
          reachable: rpcReachable,
          latency_ms: chainRes.latency_ms,
          last_error: chainRes.error ?? convRes.error,
          fetched_at: chainRes.fetched_at,
        },
        explorer_status: {
          reachable: explorerReachable,
          last_error: statusRes.error ?? mintsRes.error ?? blocksRes.error,
          fetched_at: statusRes.fetched_at,
          equivalence_claimed,
        },
        mint_state: {
          res_mints: mintsRes.body?.mints ?? (mintsRes.error ? null : []),
          res_mint_status: mintRpcRes.error ? null : mintRpcRes.result,
          mint_error: mintsRes.error,
        },
        explorer_snapshot: statusRes.body,
        recent_blocks: blocksRes.body?.blocks ?? (blocksRes.error ? null : []),
        verification_state: {
          proof034_phase: "IN_PROGRESS",
          p03_label: "P03_PARTIAL",
          gv_materialized: manifestGv ?? initialVerification.gv_materialized,
          gv_total: GV_TOTAL,
        },
        global_status,
      },
    });
  }, []);

  useEffect(() => {
    void refresh();
    const id = setInterval(() => void refresh(), 30_000);
    return () => clearInterval(id);
  }, [refresh]);

  const value = useMemo(() => ({ state, refresh }), [state, refresh]);

  return (
    <SystemStateContext.Provider value={value}>{children}</SystemStateContext.Provider>
  );
}

export function useSystemState(): ContextValue {
  const ctx = useContext(SystemStateContext);
  if (!ctx) throw new Error("useSystemState outside provider");
  return ctx;
}
