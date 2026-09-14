(** Coq-model serialization of the executable kernel state.

    Proves encode/decode identity on Gallina records only.

    NOT CLAIMED:
      network message correctness
      disk database correctness
      C++ serialization equivalence
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.

Local Open Scope Z_scope.

Definition encode_flag (b : bool) : Z :=
  if b then 1 else 0.

Definition decode_flag (z : Z) : option bool :=
  if z =? 0 then Some false
  else if z =? 1 then Some true
  else None.

Definition encode_kernel_state (e : ExecutableKernelState) : list Z :=
  [encode_flag (ek_utxo_flag e);
   ek_fees e;
   ek_burn e;
   ek_subsidy e;
   ek_res_stock e;
   ek_res_cap e;
   ek_oru_stock e;
   encode_flag (ek_script_ok e);
   encode_flag (ek_pow_ok e);
   encode_flag (ek_asert_ok e)].

Definition decode_kernel_state (xs : list Z) : option ExecutableKernelState :=
  match xs with
  | u :: fe :: bu :: su :: rs :: rc :: os :: sc :: po :: ae :: nil =>
      match decode_flag u with
      | None => None
      | Some uf =>
          match decode_flag sc with
          | None => None
          | Some sf =>
              match decode_flag po with
              | None => None
              | Some pf =>
                  match decode_flag ae with
                  | None => None
                  | Some af =>
                      Some {| ek_utxo_flag := uf;
                              ek_fees := fe;
                              ek_burn := bu;
                              ek_subsidy := su;
                              ek_res_stock := rs;
                              ek_res_cap := rc;
                              ek_oru_stock := os;
                              ek_script_ok := sf;
                              ek_pow_ok := pf;
                              ek_asert_ok := af |}
                  end
              end
          end
      end
  | _ => None
  end.

Lemma decode_encode_flag :
  forall b : bool, decode_flag (encode_flag b) = Some b.
Proof.
  intros b. destruct b; reflexivity.
Qed.

Theorem decode_encode_identity :
  forall e : ExecutableKernelState,
    decode_kernel_state (encode_kernel_state e) = Some e.
Proof.
  intros [uf fe bu su rs rc os sf pf af].
  unfold encode_kernel_state, decode_kernel_state.
  simpl.
  rewrite !decode_encode_flag.
  reflexivity.
Qed.
