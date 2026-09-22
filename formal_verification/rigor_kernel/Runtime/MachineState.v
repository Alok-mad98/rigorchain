(** Runtime machine state over bounded words, and the refinement
    obligation it creates against the executable kernel model.

    The point of this module is that [MachineKernelState] is NOT a
    field-renamed copy of [ExecutableKernelState]:

      - its numeric fields live in the bounded word domain, so the
        decode into the model is PARTIAL ([machine_decode] returns
        [option]);
      - its step function is defined INDEPENDENTLY with checked
        arithmetic, not as a wrapper around [executable_step_of], so
        it can fail where the unbounded model succeeds.

    Consequently [machine_step_refines_executable_step] is a real
    simulation obligation: it is discharged by proving the bounded
    computation agrees with the unbounded one whenever it succeeds,
    and [machine_refinement_is_partial] exhibits a state where it does
    not succeed.

    NOT CLAIMED: OCaml runtime, ABI, native binary, CPU semantics.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Kernel.KernelInputBridge.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelState.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelMapping.
From RigorKernel Require Import Refinement.ConcreteKernel.ConcreteKernelTransition.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExecutionBoundary.ExecutableTransition.
From RigorKernel Require Import ExecutionBoundary.ExecutableInvariance.
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.
From RigorKernel Require Import Runtime.MachineWord.

Local Open Scope Z_scope.

(** * Machine state *)

Record MachineKernelState := {
  mk_entry : ExtractedKernelInterface;
  mk_utxo_flag : bool;
  mk_fees : Z;
  mk_burn : Z;
  mk_subsidy : Z;
  mk_res_stock : Z;
  mk_res_cap : Z;
  mk_oru_stock : Z;
  mk_script_ok : bool;
  mk_pow_ok : bool;
  mk_asert_ok : bool
}.

(** Every numeric field must be representable in the word domain. *)
Definition machine_words_wf (m : MachineKernelState) : bool :=
  andb (wf_wordb (mk_fees m))
    (andb (wf_wordb (mk_burn m))
      (andb (wf_wordb (mk_subsidy m))
        (andb (wf_wordb (mk_res_stock m))
          (andb (wf_wordb (mk_res_cap m)) (wf_wordb (mk_oru_stock m)))))).

Definition machine_to_executable (m : MachineKernelState)
  : ExecutableKernelState :=
  {| ek_utxo_flag := mk_utxo_flag m;
     ek_fees := mk_fees m;
     ek_burn := mk_burn m;
     ek_subsidy := mk_subsidy m;
     ek_res_stock := mk_res_stock m;
     ek_res_cap := mk_res_cap m;
     ek_oru_stock := mk_oru_stock m;
     ek_script_ok := mk_script_ok m;
     ek_pow_ok := mk_pow_ok m;
     ek_asert_ok := mk_asert_ok m |}.

(** PARTIAL decode. A machine state whose words are out of range has
    no model counterpart at all. *)
Definition machine_decode (m : MachineKernelState)
  : option ExecutableKernelState :=
  if machine_words_wf m then Some (machine_to_executable m) else None.

Lemma machine_decode_some_inv :
  forall m e,
    machine_decode m = Some e ->
    machine_words_wf m = true /\ e = machine_to_executable m.
Proof.
  intros m e H. unfold machine_decode in H.
  destruct (machine_words_wf m) eqn:E; [| discriminate].
  injection H as <-. split; reflexivity.
Qed.

Lemma machine_words_wf_inv :
  forall m,
    machine_words_wf m = true ->
    wf_word (mk_fees m) /\ wf_word (mk_burn m) /\ wf_word (mk_subsidy m) /\
    wf_word (mk_res_stock m) /\ wf_word (mk_res_cap m) /\
    wf_word (mk_oru_stock m).
Proof.
  intros m H. unfold machine_words_wf in H.
  apply andb_true_iff in H as [H1 H].
  apply andb_true_iff in H as [H2 H].
  apply andb_true_iff in H as [H3 H].
  apply andb_true_iff in H as [H4 H].
  apply andb_true_iff in H as [H5 H6].
  repeat split; apply wf_wordb_true_iff; assumption.
Qed.

Lemma machine_words_wf_intro :
  forall m,
    wf_word (mk_fees m) -> wf_word (mk_burn m) -> wf_word (mk_subsidy m) ->
    wf_word (mk_res_stock m) -> wf_word (mk_res_cap m) ->
    wf_word (mk_oru_stock m) ->
    machine_words_wf m = true.
Proof.
  intros m H1 H2 H3 H4 H5 H6. unfold machine_words_wf.
  apply wf_wordb_true_iff in H1. apply wf_wordb_true_iff in H2.
  apply wf_wordb_true_iff in H3. apply wf_wordb_true_iff in H4.
  apply wf_wordb_true_iff in H5. apply wf_wordb_true_iff in H6.
  rewrite H1, H2, H3, H4, H5, H6. reflexivity.
Qed.

(** * Independent machine step

    Written directly over the word domain with checked arithmetic. It
    does not call [executable_step_of]. Note the RES accumulator: the
    model clamps with [Z.min] over an unbounded intermediate sum, so a
    machine implementation can overflow *before* the clamp applies.
    That intermediate is checked here. *)

Definition machine_step_of (m : MachineKernelState) (i : KernelInput)
  : option MachineKernelState :=
  if negb (andb (wf_wordb (ki_fees i))
            (andb (wf_wordb (ki_burn i)) (wf_wordb (ki_subsidy i))))
  then None
  else
    match w_add (mk_res_stock m) (Z.max 0 (ki_res_mint i)) with
    | None => None
    | Some res_sum =>
        match w_add (mk_oru_stock m) (oru_delta i) with
        | None => None
        | Some oru' =>
            Some {| mk_entry := mk_entry m;
                    mk_utxo_flag := andb (mk_utxo_flag m) (ki_utxo_ok i);
                    mk_fees := ki_fees i;
                    mk_burn := ki_burn i;
                    mk_subsidy := ki_subsidy i;
                    mk_res_stock := Z.min (mk_res_cap m) res_sum;
                    mk_res_cap := mk_res_cap m;
                    mk_oru_stock := oru';
                    mk_script_ok := andb (mk_script_ok m) (ki_script_ok i);
                    mk_pow_ok := andb (mk_pow_ok m) (ki_pow_ok i);
                    mk_asert_ok := andb (mk_asert_ok m) (ki_asert_ok i) |}
        end
    end.

(** * The refinement obligation

    If the machine state decodes and the machine step succeeds, then
    the result decodes and agrees with the model step. This is a
    simulation, not an identity: the two sides are computed by
    different functions over different domains. *)

Theorem machine_step_refines_executable_step :
  forall (m : MachineKernelState) (e : ExecutableKernelState)
         (i : KernelInput) (m' : MachineKernelState),
    machine_decode m = Some e ->
    machine_step_of m i = Some m' ->
    machine_decode m' = Some (executable_step_of e i).
Proof.
  intros m e i m' Hdec Hstep.
  apply machine_decode_some_inv in Hdec as [Hwf ->].
  apply machine_words_wf_inv in Hwf
    as [Hfees [Hburn [Hsub [Hstock [Hcap Horu]]]]].
  unfold machine_step_of in Hstep.
  destruct (negb (andb (wf_wordb (ki_fees i))
             (andb (wf_wordb (ki_burn i)) (wf_wordb (ki_subsidy i)))))
    eqn:Hin; [discriminate |].
  apply negb_false_iff in Hin.
  apply andb_true_iff in Hin as [Hifees Hin].
  apply andb_true_iff in Hin as [Hiburn Hisub].
  apply wf_wordb_true_iff in Hifees.
  apply wf_wordb_true_iff in Hiburn.
  apply wf_wordb_true_iff in Hisub.
  destruct (w_add (mk_res_stock m) (Z.max 0 (ki_res_mint i))) as [res_sum |]
    eqn:Hres; [| discriminate].
  destruct (w_add (mk_oru_stock m) (oru_delta i)) as [oru' |]
    eqn:Horu2; [| discriminate].
  injection Hstep as <-.
  apply w_add_some_inv in Hres as [Hres_eq Hres_wf].
  apply w_add_some_inv in Horu2 as [Horu_eq Horu_wf].
  unfold machine_decode.
  rewrite executable_step_fields.
  assert (Hminwf : wf_word (Z.min (mk_res_cap m) res_sum)).
  { unfold wf_word in *. simpl. lia. }
  rewrite machine_words_wf_intro; simpl; try assumption.
  unfold machine_to_executable. simpl.
  rewrite Hres_eq, Horu_eq. reflexivity.
Qed.

(** * The obligation is not vacuous

    A state that decodes, and an input that the unbounded model
    accepts, on which the machine step fails. Without this witness the
    theorem above could be satisfied by a machine step that never
    succeeds, or by a decode that never succeeds. *)

Definition overflow_witness_state : MachineKernelState :=
  {| mk_entry := EK_UTXO;
     mk_utxo_flag := true;
     mk_fees := 0;
     mk_burn := 0;
     mk_subsidy := 0;
     mk_res_stock := word_max;
     mk_res_cap := word_max;
     mk_oru_stock := 0;
     mk_script_ok := true;
     mk_pow_ok := true;
     mk_asert_ok := true |}.

Definition overflow_witness_input : KernelInput :=
  kernel_input_of mint_one_witness.

Theorem machine_refinement_is_partial :
  exists (m : MachineKernelState) (e : ExecutableKernelState) (i : KernelInput),
    machine_decode m = Some e /\
    machine_step_of m i = None.
Proof.
  exists overflow_witness_state,
         (machine_to_executable overflow_witness_state),
         overflow_witness_input.
  split.
  - unfold machine_decode.
    rewrite machine_words_wf_intro; simpl; try reflexivity;
      unfold wf_word, word_max; lia.
  - unfold machine_step_of, overflow_witness_state, overflow_witness_input.
    pose proof mint_one_witness_flow as [_ [_ [Hfee [Hburn [Hsub Hmint]]]]].
    rewrite Hfee, Hburn, Hsub, Hmint.
    replace (wf_wordb 0) with true
      by (symmetry; apply wf_wordb_true_iff; unfold wf_word, word_max; lia).
    simpl.
    unfold w_add.
    replace (Z.max 0 1) with 1 by lia.
    destruct (wf_wordb (word_max + 1)) eqn:E.
    + apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
    + reflexivity.
Qed.

(** The decode is genuinely partial too. *)
Theorem machine_decode_is_partial :
  exists m : MachineKernelState, machine_decode m = None.
Proof.
  exists {| mk_entry := EK_UTXO;
            mk_utxo_flag := true;
            mk_fees := word_max + 1;
            mk_burn := 0;
            mk_subsidy := 0;
            mk_res_stock := 0;
            mk_res_cap := 0;
            mk_oru_stock := 0;
            mk_script_ok := true;
            mk_pow_ok := true;
            mk_asert_ok := true |}.
  unfold machine_decode, machine_words_wf. simpl.
  destruct (wf_wordb (word_max + 1)) eqn:E; [| reflexivity].
  apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
Qed.

(** * Failure is exactly overflow

    The machine step has no failure mode other than a value leaving
    the word domain. This is what lets the runtime boundary be stated
    honestly: everything the machine rejects and the model accepts is
    a representability failure, not a consensus disagreement. *)

Theorem machine_step_failure_is_overflow :
  forall (m : MachineKernelState) (i : KernelInput),
    machine_step_of m i = None ->
    ~ wf_word (ki_fees i) \/
    ~ wf_word (ki_burn i) \/
    ~ wf_word (ki_subsidy i) \/
    ~ wf_word (mk_res_stock m + Z.max 0 (ki_res_mint i)) \/
    ~ wf_word (mk_oru_stock m + oru_delta i).
Proof.
  intros m i H. unfold machine_step_of in H.
  destruct (negb (andb (wf_wordb (ki_fees i))
             (andb (wf_wordb (ki_burn i)) (wf_wordb (ki_subsidy i)))))
    eqn:Hin.
  - apply negb_true_iff in Hin.
    apply andb_false_iff in Hin as [Hf | Hrest].
    + left. apply wf_wordb_false_iff, Hf.
    + apply andb_false_iff in Hrest as [Hb | Hs].
      * right; left. apply wf_wordb_false_iff, Hb.
      * right; right; left. apply wf_wordb_false_iff, Hs.
  - destruct (w_add (mk_res_stock m) (Z.max 0 (ki_res_mint i))) eqn:Hres.
    + destruct (w_add (mk_oru_stock m) (oru_delta i)) eqn:Horu.
      * discriminate.
      * right; right; right; right. apply w_add_none_inv, Horu.
    + right; right; right; left. apply w_add_none_inv, Hres.
Qed.

(** * The kernel invariant guarantees runtime representability

    This is what a runtime boundary is supposed to establish, and what
    the previous [exact Hinv] version of
    [runtime_projection_preserves_kernel_domain] only appeared to
    establish. Because [machine_decode] is partial, the statement has
    to produce a decode success, which requires the invariant to be
    strong enough to bound every field. It is not: the [res_stock <=
    res_cap <= word_max] chain is what supplies the [res_stock] bound. *)

Definition machine_invariants (m : MachineKernelState) : Prop :=
  mk_utxo_flag m = true /\
  wf_word (mk_fees m) /\
  wf_word (mk_burn m) /\
  wf_word (mk_subsidy m) /\
  0 <= mk_oru_stock m <= word_max /\
  0 <= mk_res_stock m /\
  mk_res_stock m <= mk_res_cap m /\
  mk_res_cap m <= word_max /\
  mk_script_ok m = true /\
  mk_pow_ok m = true /\
  mk_asert_ok m = true.

Theorem machine_invariants_imply_representable :
  forall m : MachineKernelState,
    machine_invariants m ->
    exists e : ExecutableKernelState,
      machine_decode m = Some e /\ executable_invariants e.
Proof.
  intros m [Hutxo [Hfees [Hburn [Hsub [Horu [Hres0
          [Hrescap [Hcap [Hscript [Hpow Hasert]]]]]]]]]].
  assert (Hstock : wf_word (mk_res_stock m)).
  { unfold wf_word in *. lia. }
  assert (Hcapw : wf_word (mk_res_cap m)).
  { unfold wf_word in *. lia. }
  assert (Horuw : wf_word (mk_oru_stock m)).
  { unfold wf_word in *. lia. }
  exists (machine_to_executable m).
  split.
  - unfold machine_decode.
    rewrite machine_words_wf_intro; try assumption. reflexivity.
  - unfold executable_invariants, machine_to_executable. simpl.
    unfold wf_word in *.
    repeat split; try assumption; lia.
Qed.

(** The converse fails: a machine state can decode without satisfying
    the kernel invariant. Decoding is representability, not validity,
    and the two must not be conflated at the boundary. *)
Theorem representable_does_not_imply_valid :
  exists m : MachineKernelState,
    (exists e, machine_decode m = Some e) /\ ~ machine_invariants m.
Proof.
  exists {| mk_entry := EK_UTXO;
            mk_utxo_flag := false;
            mk_fees := 0;
            mk_burn := 0;
            mk_subsidy := 0;
            mk_res_stock := 0;
            mk_res_cap := 0;
            mk_oru_stock := 0;
            mk_script_ok := true;
            mk_pow_ok := true;
            mk_asert_ok := true |}.
  split.
  - eexists. unfold machine_decode.
    rewrite machine_words_wf_intro; simpl; try reflexivity;
      unfold wf_word, word_max; lia.
  - intros [Hutxo _]. simpl in Hutxo. discriminate.
Qed.

(** * Determinism, proved rather than flag-guarded

    [machine_step_of] is a function, so determinism follows from
    functional equality. There is no [machine_step_unique] switch to
    flip: the statement cannot be made false by editing a constant. *)

Theorem machine_step_deterministic_real :
  forall m i r1 r2,
    machine_step_of m i = r1 ->
    machine_step_of m i = r2 ->
    r1 = r2.
Proof.
  intros m i r1 r2 H1 H2. rewrite <- H1, <- H2. reflexivity.
Qed.
