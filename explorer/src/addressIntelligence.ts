/** Address intelligence presentation mapping (TASK-EXPLORER-008). Indexed data only. */

import { bech32, bech32m } from "bech32";
import type { CompliancePayload } from "./compliance";
import type { ChainStateRow } from "./db";

export const ADDRESS_READONLY_NOTICE =
  "Address information is derived from indexed rigord data. This page does not create, sign, or broadcast transactions.";

const VALID_HRPS = new Set(["rg", "trg", "rgrt", "dvt", "slt"]);

export type AddressEncoding = "bech32" | "bech32m";

export type AddressTypeLabel =
  | "ORU (Witness v0)"
  | "ORU (Witness v1)"
  | "RES (Witness v1)"
  | "Mixed ORU/RES"
  | "Unknown";

export type AddressClassification =
  | "ORU Address"
  | "RES Address"
  | "Historical RFC-014 / retired Witness v1 CVG2 (not OP_CONVERGE)"
  | "Mixed Address"
  | "Unused Address";

/** Parse RIGOR bech32/bech32m address (any checksum-valid witness encoding, not only trg1q/trg1p). */
export function parseRigorAddress(address: string): { hrp: string; encoding: AddressEncoding } | null {
  const raw = address.trim().toLowerCase();
  try {
    const dec = bech32.decode(raw, 1023);
    if (!VALID_HRPS.has(dec.prefix)) return null;
    return { hrp: dec.prefix, encoding: "bech32" };
  } catch {
    /* try bech32m */
  }
  try {
    const dec = bech32m.decode(raw, 1023);
    if (!VALID_HRPS.has(dec.prefix)) return null;
    return { hrp: dec.prefix, encoding: "bech32m" };
  } catch {
    return null;
  }
}

export function isValidAddressFormat(address: string): boolean {
  return parseRigorAddress(address) !== null;
}

/** Witness v0 uses Bech32; v1 uses Bech32m (RFC-ADDRESS-002). Live Convergence is OP_CONVERGE (no receive address). */
export function inferAddressTypeFromBech32(address: string): AddressTypeLabel {
  const parsed = parseRigorAddress(address);
  if (!parsed) return "Unknown";
  if (parsed.encoding === "bech32") return "ORU (Witness v0)";
  return "RES (Witness v1)";
}

export function classifyAddress(
  hasOru: boolean,
  hasRes: boolean,
  hasActivity: boolean,
): { address_type: AddressTypeLabel; classification: AddressClassification } {
  if (!hasActivity) {
    return { address_type: "Unknown", classification: "Unused Address" };
  }
  if (hasOru && hasRes) {
    return { address_type: "Mixed ORU/RES", classification: "Mixed Address" };
  }
  if (hasRes) return { address_type: "RES (Witness v1)", classification: "RES Address" };
  if (hasOru) return { address_type: "ORU (Witness v0)", classification: "ORU Address" };
  return { address_type: "Unknown", classification: "Unused Address" };
}

export function classifyAddressWithHint(
  hasOru: boolean,
  hasRes: boolean,
  hasActivity: boolean,
  address: string,
): { address_type: AddressTypeLabel; classification: AddressClassification } {
  const hint = inferAddressTypeFromBech32(address);
  if (!hasActivity) {
    const classification: AddressClassification =
      hint === "RES (Witness v1)" ? "RES Address" : hint === "ORU (Witness v0)" ? "ORU Address" : "Unused Address";
    return { address_type: hint, classification };
  }
  // Bech32m + ORU-only activity is Historical RFC-014 / retired Witness v1 CVG2, not live OP_CONVERGE.
  if (hasOru && !hasRes && hint === "RES (Witness v1)") {
    return {
      address_type: "ORU (Witness v1)",
      classification: "Historical RFC-014 / retired Witness v1 CVG2 (not OP_CONVERGE)",
    };
  }
  return classifyAddress(hasOru, hasRes, hasActivity);
}

export { atomsToOruDisplay, atomsToResDisplay } from "./amount";

export function buildAddressIntegrity(
  compliance: CompliancePayload,
  chainState: ChainStateRow,
  indexerTipHeight: number,
  nodeHeight: number | null,
): Record<string, unknown> {
  return {
    chain_name: compliance.chain_name,
    network: compliance.network,
    genesis_hash_short: compliance.genesis_hash_short,
    equivalence_claimed: false,
    rpc_authority_notice: "rigord: authoritative",
    indexer_sync_lag_blocks:
      nodeHeight != null && indexerTipHeight >= 0 ? Math.max(0, nodeHeight - indexerTipHeight) : null,
    indexer_height: indexerTipHeight,
    mismatch_active: compliance.mismatch_active,
    mismatch_messages: compliance.mismatch_messages,
    indexer_updated_at: chainState.updated_at,
  };
}
