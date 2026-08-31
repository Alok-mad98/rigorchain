(** CoinbaseLayoutBridge.v — P052 coinbase predicate mapping.

    C++ [RigorResCoinbaseExpected] is fees + bootstrap + mint.
    This file records that identity. It is not a coinbase builder.

    [BuildCoinbase] / GBT assembly is outside the Kernel.

    Does not prove C++ = Coq. No [Axiom] / [Admitted] / [Parameter]. *)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorV01 Require Import AssetClassification.

Local Open Scope Z_scope.

Definition refinement_proved : bool := false.
Definition equivalence_proved : bool := false.

Theorem coinbase_bridge_refinement_not_proved : refinement_proved = false.
Proof. reflexivity. Qed.

Theorem coinbase_bridge_equivalence_not_proved : equivalence_proved = false.
Proof. reflexivity. Qed.

(** Predicate, not a constructor. *)
Definition cpp_res_coinbase_expected_bridge
  (fees_res bootstrap mint : Z) : Z :=
  fees_res + bootstrap + mint.

Theorem cpp_res_coinbase_expected_is_sum :
  forall fees_res bootstrap mint,
    cpp_res_coinbase_expected_bridge fees_res bootstrap mint =
    fees_res + bootstrap + mint.
Proof. intros. reflexivity. Qed.

Theorem genesis_res_coinbase_schedule_only :
  cpp_res_coinbase_expected_bridge 0 0 0 = 0.
Proof. reflexivity. Qed.

(** Mint script shape lives in [classify] / mint parsers. Not a builder. *)
Definition mint_script_is_res (spk : list byte) : bool :=
  match classify spk with
  | AssetRES => true
  | _ => false
  end.

Definition BuildCoinbase_is_kernel : bool := false.

Theorem build_coinbase_is_not_kernel :
  BuildCoinbase_is_kernel = false.
Proof. reflexivity. Qed.
