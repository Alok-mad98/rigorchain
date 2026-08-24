/**
 * Explorer/indexer RFC-018 ORU fee-burn statistics.
 * Presentation only. Does not change consensus, chainstate, or ConnectBlock.
 *
 * Eligibility matches IsORBClaimTx: RES input domain, not coinbase, not ORU-funded mint.
 * destroyed_fee_oru(tx) = floor(ORU_leg_fee / 2) with NUM=1 DEN=2.
 *
 * equivalence_claimed = false
 */

export const ORU_COIN = 100000000n;
export const RES_COIN = 1000000n;
export const ORU_RES_TRANSFER_FEE_BURN_NUM = 1n;
export const ORU_RES_TRANSFER_FEE_BURN_DEN = 2n;

export type IndexedAsset = "ORU" | "RES" | "NONE";

export type IndexedPrevout = {
  asset_kind: IndexedAsset;
  value_atoms: bigint;
};

export type IndexedOutput = {
  asset_kind: IndexedAsset;
  value_atoms: bigint;
};

/** Floor share matching Consensus::OruResTransferFeeBurnShare. */
export function oruResTransferFeeBurnShare(fee: bigint): bigint {
  if (fee <= 0n) return 0n;
  return (fee * ORU_RES_TRANSFER_FEE_BURN_NUM) / ORU_RES_TRANSFER_FEE_BURN_DEN;
}

export function oruLegFee(inputs: IndexedPrevout[], outputs: IndexedOutput[]): bigint {
  let oruIn = 0n;
  let oruOut = 0n;
  for (const i of inputs) {
    if (i.asset_kind === "ORU") oruIn += i.value_atoms;
  }
  for (const o of outputs) {
    if (o.asset_kind === "ORU") oruOut += o.value_atoms;
  }
  return oruIn - oruOut;
}

export function rfc018DestroyedFeeOru(args: {
  isCoinbase: boolean;
  isResMint: boolean;
  inputs: Array<IndexedPrevout | null>;
  outputs: IndexedOutput[];
}): bigint {
  if (args.isCoinbase || args.isResMint) return 0n;
  if (args.inputs.some((i) => i === null)) return 0n;
  const resolved = args.inputs as IndexedPrevout[];
  const hasResInput = resolved.some((i) => i.asset_kind === "RES");
  if (!hasResInput) return 0n;
  return oruResTransferFeeBurnShare(oruLegFee(resolved, args.outputs));
}

export function destroyedFeeOruForBlock(
  txs: Array<{
    isCoinbase: boolean;
    isResMint: boolean;
    inputs: Array<IndexedPrevout | null>;
    outputs: IndexedOutput[];
  }>,
): bigint {
  let sum = 0n;
  for (const tx of txs) sum += rfc018DestroyedFeeOru(tx);
  return sum;
}

export function nextCumulativeBurn(parentCumulative: bigint, blockBurn: bigint): bigint {
  return parentCumulative + blockBurn;
}

/** Disconnect of the tip: subtract that block's burned_oru from the chain cumulative. */
export function rollbackCumulativeBurn(tipCumulative: bigint, disconnectedBlockBurn: bigint): bigint {
  return tipCumulative - disconnectedBlockBurn;
}

export type Rfc018FeeBurnView = {
  block_burned_oru: string;
  cumulative_burned_oru: string;
  block_burned_oru_display: string;
  cumulative_burned_oru_display: string;
  equivalence_claimed: false;
};

export function rfc018FeeBurnView(blockBurn: bigint, cumulative: bigint, formatOru: (atoms: bigint) => string): Rfc018FeeBurnView {
  return {
    block_burned_oru: blockBurn.toString(),
    cumulative_burned_oru: cumulative.toString(),
    block_burned_oru_display: formatOru(blockBurn),
    cumulative_burned_oru_display: formatOru(cumulative),
    equivalence_claimed: false,
  };
}
