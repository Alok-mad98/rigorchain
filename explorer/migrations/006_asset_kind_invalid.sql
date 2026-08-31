-- Script-as-asset fail-closed class: unknown scripts are INVALID, not NONE/ORU.
-- Matches C++ AssetTypeFromScriptPubKey. Not a consensus change.

BEGIN;

ALTER TABLE outputs DROP CONSTRAINT IF EXISTS outputs_asset_kind_check;
ALTER TABLE outputs ADD CONSTRAINT outputs_asset_kind_check
  CHECK (asset_kind IN ('ORU','RES','NONE','INVALID'));

COMMIT;
