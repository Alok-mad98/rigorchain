(**
  LEGACY ACCOUNT MODEL — NOT THE C++ UTXO IMPLEMENTATION (W9).

  [ORU_supply] / [RES_supply] are global account scalars. The C++ node
  tracks spendable value in the UTXO set. Do not treat this [State] as
  ConnectBlock authority.

  Current structural model: ReplayReorgCore/ReplayState.v.
  Honesty: equivalence_claimed = false;
  hash_kernel_shared_binary_with_cpp = false; refinement_proved = false.
*)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Rigor.legacy_account_model.Types.

(* Consensus State *)
Record State := {
  ORU_supply : Z;
  RES_supply : Z;
  current_height : Z;
  last_hash : bytes32;
}.

(* Initial state *)
Definition init_state : State :=
  {| ORU_supply := 0%Z;
     RES_supply := 0%Z;
     current_height := 0%Z;
     last_hash := 0%Z;
  |}.
