(** RigorKernel.Core.Amount — Phase A foundation.

    Scope: consensus kernel only.

    Constants mirrored from [core/src/consensus/amount.h]:
      COIN      = 100000000
      MAX_MONEY = 10000000000 * COIN
    [MAX_MONEY] is an overflow sanity bound, not an ORU supply cap.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Definition amount := Z.

(** mirrors core/src/consensus/amount.h COIN *)
Definition COIN : amount := 100000000.

(** mirrors core/src/consensus/amount.h MAX_MONEY *)
Definition MAX_MONEY : amount := 10000000000 * COIN.

(** mirrors core/src/consensus/consensus.h COINBASE_MATURITY.
    Body RES mint uses the same depth ([RES_MINT_MATURITY]). *)
Definition COINBASE_MATURITY : Z := 100.
Definition RES_MINT_MATURITY : Z := COINBASE_MATURITY.

Definition money_range (v : amount) : Prop :=
  0 <= v <= MAX_MONEY.

Definition money_range_b (v : amount) : bool :=
  andb (0 <=? v) (v <=? MAX_MONEY).

Theorem money_range_zero :
  money_range 0.
Proof. unfold money_range, MAX_MONEY, COIN. lia. Qed.

Theorem money_range_coin :
  money_range COIN.
Proof. unfold money_range, MAX_MONEY, COIN. lia. Qed.
