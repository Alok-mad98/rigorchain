import { formatAtoms, pickNumber, pickString } from "./formatAmount";

/** Projection of getconvergencestatus under RFC-020. */
export type Rfc020Live = {
  tipHeight: number | null;
  confirmHeight: number | null;
  destroyCostAtoms: number | null;
  destroyCostOru: string;
  minAtomBurnAtoms: number | null;
  minAtomBurnOru: string;
  minWholeResBurnAtoms: number | null;
  minWholeResBurnOru: string;
  nextHeight: number | null;
  destroyCostNextOru: string;
  resTotal: string;
  cumulativeConvergence: string;
  lastResHeight: number | null;
  issuanceAuthorized: boolean | null;
};

export function readRfc020Live(status: Record<string, unknown> | null): Rfc020Live {
  const destroyCostAtoms = pickNumber(status, "destroy_cost", "rate");
  const minAtom = pickNumber(status, "min_atom_burn");
  const minWhole = pickNumber(status, "min_whole_res_burn");
  const destroyCostNext = pickNumber(status, "destroy_cost_next", "next_rate");
  const last = pickNumber(status, "last_res_height");
  return {
    tipHeight: pickNumber(status, "tip_height", "height"),
    confirmHeight: pickNumber(status, "confirm_height"),
    destroyCostAtoms,
    destroyCostOru: formatAtoms(destroyCostAtoms),
    minAtomBurnAtoms: minAtom,
    minAtomBurnOru: formatAtoms(minAtom),
    minWholeResBurnAtoms: minWhole,
    minWholeResBurnOru: formatAtoms(minWhole),
    nextHeight: pickNumber(status, "next_height"),
    destroyCostNextOru: formatAtoms(destroyCostNext),
    resTotal: pickString(status, "res_total") ?? "—",
    cumulativeConvergence: pickString(status, "cumulative_convergence") ?? "—",
    lastResHeight: last !== null && last < 0 ? null : last,
    issuanceAuthorized:
      typeof status?.issuance_authorized === "boolean" ? status.issuance_authorized : null,
  };
}
