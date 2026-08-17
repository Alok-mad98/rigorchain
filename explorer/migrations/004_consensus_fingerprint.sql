-- Verification identity only. Nullable so existing rows stay valid (UI: Not available / N/A).
-- Does not alter block acceptance or consensus storage.

ALTER TABLE blocks ADD COLUMN IF NOT EXISTS consensus_fingerprint TEXT;
