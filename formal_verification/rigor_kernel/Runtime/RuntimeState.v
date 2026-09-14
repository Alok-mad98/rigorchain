(** Coq runtime *model* state. Not the OCaml RTS and not a CPU.

    [RuntimeKernelState] projects onto [ExecutableKernelState].
    Mapping from [ExtractedKernelInterface] is a model tag only.

    NOT CLAIMED: OCaml runtime, ABI, native binary.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import ExecutionBoundary.ConcreteExecutionState.
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.
From RigorKernel Require Import Runtime.MachineWord.
From RigorKernel Require Import Runtime.MachineState.

Local Open Scope Z_scope.

Record RuntimeKernelState := {
  rk_entry : ExtractedKernelInterface;
  rk_utxo_flag : bool;
  rk_fees : Z;
  rk_burn : Z;
  rk_subsidy : Z;
  rk_res_stock : Z;
  rk_res_cap : Z;
  rk_oru_stock : Z;
  rk_script_ok : bool;
  rk_pow_ok : bool;
  rk_asert_ok : bool
}.

Definition RuntimeKernelInput := KernelInput.

Record RuntimeKernelOutput := {
  rto_ok : bool;
  rto_next : RuntimeKernelState
}.

Definition extracted_to_runtime_tag (e : ExtractedKernelInterface)
  : ExtractedKernelInterface := e.

(** Named so M38 can flip the projected UTXO flag. *)
Definition runtime_project_utxo (r : RuntimeKernelState) : bool :=
  rk_utxo_flag r.

Definition runtime_projection (r : RuntimeKernelState) : ExecutableKernelState :=
  {| ek_utxo_flag := runtime_project_utxo r;
     ek_fees := rk_fees r;
     ek_burn := rk_burn r;
     ek_subsidy := rk_subsidy r;
     ek_res_stock := rk_res_stock r;
     ek_res_cap := rk_res_cap r;
     ek_oru_stock := rk_oru_stock r;
     ek_script_ok := rk_script_ok r;
     ek_pow_ok := rk_pow_ok r;
     ek_asert_ok := rk_asert_ok r |}.

Definition embed_executable
    (e : ExecutableKernelState) (tag : ExtractedKernelInterface)
  : RuntimeKernelState :=
  {| rk_entry := tag;
     rk_utxo_flag := ek_utxo_flag e;
     rk_fees := ek_fees e;
     rk_burn := ek_burn e;
     rk_subsidy := ek_subsidy e;
     rk_res_stock := ek_res_stock e;
     rk_res_cap := ek_res_cap e;
     rk_oru_stock := ek_oru_stock e;
     rk_script_ok := ek_script_ok e;
     rk_pow_ok := ek_pow_ok e;
     rk_asert_ok := ek_asert_ok e |}.

(** The runtime invariant carries word bounds.

    Without them the invariant says nothing about whether a state can
    exist in the bounded domain the extracted validators run on, and
    the boundary theorem below degenerates into [exact Hinv] — which
    is exactly what it used to be. The [res_stock <= res_cap <=
    word_max] chain is what supplies the [res_stock] bound; it is not
    stated directly. *)
Definition runtime_invariants (r : RuntimeKernelState) : Prop :=
  rk_utxo_flag r = true /\
  wf_word (rk_fees r) /\
  wf_word (rk_burn r) /\
  wf_word (rk_subsidy r) /\
  0 <= rk_oru_stock r <= word_max /\
  0 <= rk_res_stock r /\
  rk_res_stock r <= rk_res_cap r /\
  rk_res_cap r <= word_max /\
  rk_script_ok r = true /\
  rk_pow_ok r = true /\
  rk_asert_ok r = true.

Definition runtime_to_machine (r : RuntimeKernelState) : MachineKernelState :=
  {| mk_entry := rk_entry r;
     mk_utxo_flag := runtime_project_utxo r;
     mk_fees := rk_fees r;
     mk_burn := rk_burn r;
     mk_subsidy := rk_subsidy r;
     mk_res_stock := rk_res_stock r;
     mk_res_cap := rk_res_cap r;
     mk_oru_stock := rk_oru_stock r;
     mk_script_ok := rk_script_ok r;
     mk_pow_ok := rk_pow_ok r;
     mk_asert_ok := rk_asert_ok r |}.

(** A runtime state satisfying the kernel invariant is REPRESENTABLE:
    it decodes into the executable model rather than merely being
    field-copied onto it. The decode is partial ([machine_decode]
    returns [option]), so the existential has to be produced, and
    producing it is where the word bounds are consumed. *)
Theorem runtime_projection_preserves_kernel_domain :
  forall r : RuntimeKernelState,
    runtime_invariants r ->
    exists e : ExecutableKernelState,
      machine_decode (runtime_to_machine r) = Some e /\
      executable_invariants e /\
      e = runtime_projection r.
Proof.
  intros r Hinv.
  assert (Hm : machine_invariants (runtime_to_machine r)).
  { unfold machine_invariants, runtime_to_machine, runtime_project_utxo.
    simpl. exact Hinv. }
  destruct (machine_invariants_imply_representable _ Hm) as [e [Hdec Hexec]].
  exists e. split; [exact Hdec |]. split; [exact Hexec |].
  apply machine_decode_some_inv in Hdec as [_ ->].
  unfold machine_to_executable, runtime_to_machine, runtime_projection,
         runtime_project_utxo. reflexivity.
Qed.

(** Representability is not automatic: a runtime record whose fields
    are out of the word domain has no executable counterpart. *)
Theorem runtime_state_can_be_unrepresentable :
  exists r : RuntimeKernelState, machine_decode (runtime_to_machine r) = None.
Proof.
  exists {| rk_entry := EK_UTXO;
            rk_utxo_flag := true;
            rk_fees := word_max + 1;
            rk_burn := 0;
            rk_subsidy := 0;
            rk_res_stock := 0;
            rk_res_cap := 0;
            rk_oru_stock := 0;
            rk_script_ok := true;
            rk_pow_ok := true;
            rk_asert_ok := true |}.
  unfold machine_decode, machine_words_wf, runtime_to_machine. simpl.
  destruct (wf_wordb (word_max + 1)) eqn:E; [| reflexivity].
  apply wf_wordb_true_iff in E. unfold wf_word in E. lia.
Qed.
