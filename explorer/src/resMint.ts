/**
 * Explorer mint adapter: issuance is the coinbase OP_RES_MINT output.
 * Body PCORB witness is not a mint proof on the live consensus path.
 */

import { isResMintScriptPubKey } from "./script";

export const COINBASE_RES_MINT_SOURCE = "coinbase_op_res_mint";

export type MintVout = {
  vout_index: number;
  value_atoms: bigint;
  script_pubkey: string;
};

export type CoinbaseResMint = {
  mintTxid: string;
  outputs: MintVout[];
  amountAtoms: bigint;
};

export type CoinbaseMintProofItems = {
  source: typeof COINBASE_RES_MINT_SOURCE;
  script_pubkey: string;
  paid_winner_txid: string;
};

export function findCoinbaseOpResMint(
  txs: Array<{
    txid: string;
    is_coinbase: boolean;
    vouts: Array<{ vout_index: number; value_atoms: bigint; script_pubkey: string }>;
  }>,
): CoinbaseResMint | null {
  for (const tx of txs) {
    if (!tx.is_coinbase) continue;
    const outputs = tx.vouts.filter((o) => isResMintScriptPubKey(o.script_pubkey));
    if (outputs.length === 0) continue;
    return {
      mintTxid: tx.txid,
      outputs,
      amountAtoms: outputs.reduce((acc, o) => acc + o.value_atoms, 0n),
    };
  }
  return null;
}

export function coinbaseMintProofItems(args: {
  script_pubkey: string;
  paid_winner_txid: string;
}): CoinbaseMintProofItems {
  return {
    source: COINBASE_RES_MINT_SOURCE,
    script_pubkey: args.script_pubkey,
    paid_winner_txid: args.paid_winner_txid,
  };
}

export function isCoinbaseMintProofItems(value: unknown): value is CoinbaseMintProofItems {
  if (!value || typeof value !== "object") return false;
  return (value as { source?: unknown }).source === COINBASE_RES_MINT_SOURCE;
}
