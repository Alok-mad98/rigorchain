/** In-memory indexer recovery state — no DB schema changes. */

import { logMismatchEvent } from "./mismatchLog";

export type ImporterRecoveryState = {
  stalled_height: number | null;
  last_failure_at: string | null;
  last_error: string | null;
  attempt_count: number;
  last_retry_at: string | null;
};

const state: ImporterRecoveryState = {
  stalled_height: null,
  last_failure_at: null,
  last_error: null,
  attempt_count: 0,
  last_retry_at: null,
};

export function getImporterRecoveryState(): ImporterRecoveryState {
  return { ...state };
}

export function recordImporterFailure(height: number, error: string): void {
  state.stalled_height = height;
  state.last_failure_at = new Date().toISOString();
  state.last_error = error;
  state.attempt_count += 1;
  logMismatchEvent({
    kind: "import_tick_failed",
    height,
    message: error,
    attempt: state.attempt_count,
  });
}

export function shouldRetryImport(nowMs: number, retryIntervalMs: number): boolean {
  if (state.stalled_height == null) return true;
  if (!state.last_failure_at) return true;
  const last = Date.parse(state.last_failure_at);
  if (!Number.isFinite(last)) return true;
  return nowMs - last >= retryIntervalMs;
}

export function recordRecoveryRetry(height: number): void {
  state.last_retry_at = new Date().toISOString();
  logMismatchEvent({
    kind: "recovery_retry",
    height,
    message: `Retrying import at height ${height}`,
    attempt: state.attempt_count,
  });
}

export function clearImporterRecovery(successHeight?: number): void {
  if (state.stalled_height != null) {
    logMismatchEvent({
      kind: "recovery_success",
      height: successHeight ?? state.stalled_height,
      message: "Indexer import resumed after mismatch stall",
      attempt: state.attempt_count,
    });
  }
  state.stalled_height = null;
  state.last_failure_at = null;
  state.last_error = null;
  state.attempt_count = 0;
  state.last_retry_at = null;
}

export function resetImporterRecoveryForTests(): void {
  state.stalled_height = null;
  state.last_failure_at = null;
  state.last_error = null;
  state.attempt_count = 0;
  state.last_retry_at = null;
}
