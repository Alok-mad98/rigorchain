(** RigorSurface.Kernel.TxBool — [valid_tx_b] slice.

    Lifted out of [proofs/UTXO_Soundness/UTXOSoundness.v].
    Theorems stay in the cone.
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorSurface Require Import Core.Bytes.
From RigorSurface Require Import Core.Amount.

Local Open Scope Z_scope.

Definition hash32 := list byte.

Record outpoint := { op_txid : hash32; op_index : nat }.

Record txout := { txo_value : Z; txo_spk : list byte }.

Record tx := {
  tx_id : hash32;
  tx_in : list outpoint;
  tx_out : list txout;
  tx_coinbase : bool
}.

Fixpoint bytes_eqb (a b : list byte) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (Nat.eqb x y) (bytes_eqb xs ys)
  | _, _ => false
  end.

Definition outpoint_eqb (o1 o2 : outpoint) : bool :=
  andb (bytes_eqb (op_txid o1) (op_txid o2))
       (Nat.eqb (op_index o1) (op_index o2)).

Definition is_output_op_b (t : tx) (o : outpoint) : bool :=
  andb (bytes_eqb (op_txid o) (tx_id t))
       (Nat.ltb (op_index o) (length (tx_out t))).

Definition inputs_fresh_b (t : tx) : bool :=
  forallb (fun o => negb (is_output_op_b t o)) (tx_in t).

Fixpoint sum_values (outs : list txout) : Z :=
  match outs with
  | nil => 0
  | o :: rest => txo_value o + sum_values rest
  end.

Fixpoint nodupb (l : list outpoint) : bool :=
  match l with
  | nil => true
  | x :: xs => andb (negb (existsb (outpoint_eqb x) xs)) (nodupb xs)
  end.

Definition nonemptyb {A} (l : list A) : bool :=
  match l with nil => false | _ => true end.

Definition valid_tx_b (t : tx) : bool :=
  andb (negb (tx_coinbase t))
  (andb (nonemptyb (tx_in t))
  (andb (nonemptyb (tx_out t))
  (andb (nodupb (tx_in t))
  (andb (forallb (fun o => money_range_b (txo_value o)) (tx_out t))
  (andb (money_range_b (sum_values (tx_out t)))
        (inputs_fresh_b t)))))).
