import type { ApiStatus, ExplorerBlockRow, ResMintRow } from "./explorerClient";
import { formatAtoms } from "./formatAmount";

/** Map explorer `/api/res/mints` rows onto the site table. Display only. */

export function displayMintTxid(m: ResMintRow): string {
  const v = m.txid ?? m.mint_txid;
  return v == null || v === "" ? "" : String(v);
}

export function displayMintAmount(m: ResMintRow): string {
  const v = m.amount ?? m.mint_amount;
  if (v == null || v === "") return "—";
  const formatted = formatAtoms(v);
  return formatted === "—" ? String(v) : `${formatted} RES`;
}

export function displayMintHeight(m: ResMintRow): string {
  const v = m.height ?? m.block_height;
  return v == null || v === "" ? "—" : String(v);
}

function asRecord(v: unknown): Record<string, unknown> | null {
  return v && typeof v === "object" && !Array.isArray(v) ? (v as Record<string, unknown>) : null;
}

/**
 * Project explorer `/api/status` into the keys `readRfc020Live` already understands.
 * Observer copy of indexed/RPC-derived explorer fields — not a second consensus path.
 */
export function convergenceFromExplorerStatus(status: ApiStatus | null): Record<string, unknown> | null {
  if (!status) return null;
  const eco = asRecord(status.economic_snapshot);
  const block = asRecord(status.block_health);
  if (!eco && !block) return null;
  return {
    tip_height: eco?.tip_height ?? block?.current_height_node ?? block?.current_height_indexer,
    confirm_height: eco?.confirm_height,
    destroy_cost: eco?.res_destroy_cost_atoms,
    res_total: eco?.network_res_minted_display,
    cumulative_convergence: eco?.cumulative_convergence_display,
    last_res_height: eco?.last_res_mint_height ?? eco?.last_res_height,
  };
}

export function explorerTipHeight(status: ApiStatus | null): number | null {
  const block = asRecord(status?.block_health);
  const eco = asRecord(status?.economic_snapshot);
  const raw = block?.current_height_node ?? block?.current_height_indexer ?? eco?.tip_height;
  if (typeof raw === "number" && Number.isFinite(raw)) return raw;
  if (typeof raw === "string" && raw !== "" && Number.isFinite(Number(raw))) return Number(raw);
  return null;
}

export function explorerBestHash(status: ApiStatus | null): string | null {
  const block = asRecord(status?.block_health);
  const h = block?.best_block_hash;
  return typeof h === "string" && h ? h : null;
}

function displayText(v: unknown): string {
  if (v == null || v === "") return "—";
  return String(v);
}

function nestedDisplay(root: Record<string, unknown> | null, ...path: string[]): string {
  let cur: unknown = root;
  for (const key of path) {
    const rec = asRecord(cur);
    if (!rec) return "—";
    cur = rec[key];
  }
  return displayText(cur);
}

/** Observer projection of explorer `/api/status` ORU fields. Display only. */
export type OruObserverView = {
  tipHeight: string;
  confirmHeight: string;
  issued: string;
  circulating: string;
  mature: string;
  immature: string;
  coinbaseTotal: string;
  subsidyTip: string;
  schedule: string;
  convergenceDestroyed: string;
  plainDestroy: string;
  rfc018Cumulative: string;
  utxoCount: string;
};

export function oruFromExplorerStatus(status: ApiStatus | null): OruObserverView | null {
  if (!status) return null;
  const eco = asRecord(status.economic_snapshot);
  if (!eco) return null;
  const chart = asRecord(eco.supply_chart);
  const rfc018 = asRecord(eco.rfc018_fee_burn);
  const issued =
    nestedDisplay(chart, "total_issued_oru", "display") !== "—"
      ? nestedDisplay(chart, "total_issued_oru", "display")
      : displayText(eco.oru_supply_cumulative_display);
  return {
    tipHeight: displayText(eco.tip_height),
    confirmHeight: displayText(eco.confirm_height ?? eco.tip_height),
    issued,
    circulating: displayText(
      eco.oru_circulating_spendable_display ?? nestedDisplay(chart, "oru_circulating_spendable", "display"),
    ),
    mature: displayText(eco.oru_mature_transferable_display),
    immature: displayText(eco.oru_immature_coinbase_display),
    coinbaseTotal: displayText(eco.oru_coinbase_total_display),
    subsidyTip: displayText(eco.oru_subsidy_tip_display),
    schedule: displayText(eco.oru_subsidy_schedule_display),
    convergenceDestroyed: displayText(eco.cumulative_convergence_display ?? eco.oru_burned_lifetime_display),
    plainDestroy: displayText(eco.oru_plain_destroy_display),
    rfc018Cumulative: displayText(rfc018?.cumulative_burned_oru_display),
    utxoCount: displayText(eco.oru_utxo_count),
  };
}

export function displayBlockHeight(b: ExplorerBlockRow): string {
  return b.height == null || b.height === "" ? "—" : String(b.height);
}

export function displayBlockHash(b: ExplorerBlockRow): string {
  const v = b.block_hash ?? b.hash;
  return v == null || v === "" ? "" : String(v);
}

export function displayBlockOruMinted(b: ExplorerBlockRow): string {
  if (b.oru_minted_display) return String(b.oru_minted_display);
  if (b.oru_minted != null && b.oru_minted !== "") return `${b.oru_minted} ORU`;
  if (b.oru_minted_atoms != null && b.oru_minted_atoms !== "") {
    const formatted = formatAtoms(b.oru_minted_atoms);
    return formatted === "—" ? String(b.oru_minted_atoms) : `${formatted} ORU`;
  }
  return "—";
}

export function displayBlockConvergeBurn(b: ExplorerBlockRow): string {
  if (b.converge_burn_display) return String(b.converge_burn_display);
  if (b.converge_burn_atoms != null && b.converge_burn_atoms !== "") {
    const formatted = formatAtoms(b.converge_burn_atoms);
    return formatted === "—" ? String(b.converge_burn_atoms) : `${formatted} ORU`;
  }
  return "—";
}
