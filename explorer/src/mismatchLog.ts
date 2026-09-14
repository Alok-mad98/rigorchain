/** Structured mismatch event logging (TASK-EXPLORER-012). Read-only indexer observability. */

export type MismatchEventKind =
  | "root_committed_computed"
  | "res_probability_target"
  | "import_tick_failed"
  | "recovery_retry"
  | "recovery_success"
  | "recovery_stalled"
  | "res_mint_missing_body";

export type MismatchEvent = {
  ts: string;
  kind: MismatchEventKind;
  height?: number;
  block_hash?: string;
  committed_root?: string;
  computed_root?: string;
  message: string;
  attempt?: number;
};

export function logMismatchEvent(event: Omit<MismatchEvent, "ts">): void {
  const row: MismatchEvent = { ts: new Date().toISOString(), ...event };
  console.error(JSON.stringify({ level: "CRITICAL", component: "indexer", mismatch: row }));
}
