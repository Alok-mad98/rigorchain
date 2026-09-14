/** Same-tx RFC-020 mint: body burn txid equals the RES mint txid. */
export const CONVERGENCE_IS_MINT_SQL = `
  (
    (ce.convergence_txid = b.rigor_winner AND b.rigor_winner <> '')
    OR EXISTS (
      SELECT 1 FROM res_mints rm
      WHERE rm.mint_txid = ce.convergence_txid
        AND rm.block_hash = ce.block_hash
    )
  )
`;
