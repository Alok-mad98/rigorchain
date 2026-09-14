(** RigorSurface.Kernel.UTXO — [check_utxo]. No theorems. *)
From RigorSurface Require Import Kernel.TxBool.

Definition UTXOState := tx.

Definition check_utxo (s : UTXOState) : bool := valid_tx_b s.
