(** Path A computes Path B's input.

    [Kernel.UnifiedTransition] states its invariants over a
    [KernelInput] whose four decision fields are given, not computed:

      ki_utxo_ok  ki_script_ok  ki_pow_ok  ki_asert_ok

    and whose six numeric fields are equally free. Quantified over an
    arbitrary such record, the tower says "if the input asserts the
    block was valid, the state flags stay set". The six validators in
    [Kernel.*Validator] decide those same questions by computation, on
    structured transaction and header data, but nothing connected the
    two: the types collide by name and differ in kind
    ([UTXOValidator.UTXOState] is a transaction,
    [UnifiedState.UTXOState] is a one-bit flag).

    This is the connection. [kernel_input_of] builds a [KernelInput]
    by running the validators, and takes the subsidy from the schedule
    at the mint height rather than from a field. What was assumed
    about an input is then derived from a check on real data.
*)
From Coq Require Import Bool.Bool.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From Coq Require Import Lists.List.
Import ListNotations.

From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Pow.ASERTModel.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel Require Import Core.Amount.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.

Local Open Scope Z_scope.

(** The data the six validators need, in one record. Each field is the
    argument of exactly one validator; none of them is a decision. *)
Record KernelWitness := {
  kw_tx     : UTXOSoundness.tx;
  kw_ktx    : kernel_tx;
  kw_rtx    : res_tx;
  kw_script : ScriptValidator.ScriptState;
  kw_pow    : PowHeader;
  kw_asert  : ASERTInput
}.

Definition kernel_input_of (w : KernelWitness) : KernelInput :=
  {| ki_utxo_ok   := UTXOValidator.check_utxo (kw_tx w);
     ki_incoming  := kt_oru_in (kw_ktx w);
     ki_outgoing  := kt_oru_out (kw_ktx w);
     ki_fees      := ORUConservation.fee (kw_ktx w);
     ki_burn      := rt_burn_oru (kw_rtx w);
     ki_subsidy   := oru_subsidy (rt_height (kw_rtx w));
     ki_res_mint  := rt_mint_amount (kw_rtx w);
     ki_script_ok := ScriptValidator.check_script (kw_script w);
     ki_pow_ok    := check_pow (kw_pow w);
     ki_asert_ok  := check_asert (kw_asert w) |}.

(** The subsidy is a rule, so its sign is a theorem, not a hypothesis. *)
Lemma subsidy_nonneg : forall h, 0 <= oru_subsidy h.
Proof.
  intros h. unfold oru_subsidy.
  destruct (h <=? 0);
    unfold ORU_GENESIS_BLOCK_SUBSIDY, ORU_FLAT_BLOCK_SUBSIDY, COIN; lia.
Qed.

(** Every numeric side condition [input_ok] imposes on the input is a
    consequence of the ORU and RES checks. Nothing here is assumed. *)
Lemma numeric_side_conditions :
  forall w,
    check_oru (kw_ktx w) = true ->
    check_res (kw_rtx w) = true ->
    0 <= ki_incoming (kernel_input_of w) /\
    0 <= ki_outgoing (kernel_input_of w) /\
    0 <= ki_fees (kernel_input_of w) /\
    0 <= ki_burn (kernel_input_of w) /\
    0 <= ki_subsidy (kernel_input_of w) /\
    0 <= ki_res_mint (kernel_input_of w).
Proof.
  intros w Horu Hres.
  apply check_oru_valid in Horu.
  apply check_res_reflect in Hres.
  unfold valid_tx, valid_oru_tx, to_oru in Horu. simpl in Horu.
  destruct Horu as [Hin [Hout [_ [_ [Hle _]]]]].
  unfold res_created, res_mint_fits_rate, mint_amount, burn_oru in Hres.
  destruct Hres as [_ [_ [Hpos [_ [_ [_ [Hburn _]]]]]]].
  unfold kernel_input_of, ORUConservation.fee. simpl.
  repeat split; try lia.
  apply subsidy_nonneg.
Qed.

(** Boolean packaging of the six Path A checkers. This is **not** a
    reflection of [input_ok]: [check_oru] and [check_res] are strictly
    stronger than the numeric conjuncts of [input_ok], and
    [kernel_input_of] does not store them as record bools. *)
Definition path_a_checks_b (w : KernelWitness) : bool :=
  andb (UTXOValidator.check_utxo (kw_tx w))
  (andb (check_oru (kw_ktx w))
  (andb (check_res (kw_rtx w))
  (andb (ScriptValidator.check_script (kw_script w))
  (andb (check_pow (kw_pow w))
        (check_asert (kw_asert w)))))).

Definition oru_solvency_b (s : KernelState) (w : KernelWitness) : bool :=
  Z.leb (kt_oru_out (kw_ktx w) + rt_burn_oru (kw_rtx w))
        (oru_stock (ks_oru s)
         + oru_subsidy (rt_height (kw_rtx w))
         + kt_oru_in (kw_ktx w)).

(** Mirrors [input_ok] conjuncts 12–13, not [kernel_invariants]:
    [0 <= res_stock] is not required. *)
Definition res_cap_ok_b (s : KernelState) : bool :=
  andb (Z.leb 0 (res_cap (ks_res s)))
       (Z.leb (res_stock (ks_res s)) (res_cap (ks_res s))).

Definition state_ok_b (s : KernelState) (w : KernelWitness) : bool :=
  andb (oru_solvency_b s w) (res_cap_ok_b s).

Definition computed_ok (s : KernelState) (w : KernelWitness) : Prop :=
  path_a_checks_b w = true /\ state_ok_b s w = true.

Definition computed_ok_b (s : KernelState) (w : KernelWitness) : bool :=
  andb (path_a_checks_b w) (state_ok_b s w).

Lemma oru_solvency_b_reflect :
  forall s w,
    oru_solvency_b s w = true <->
    kt_oru_out (kw_ktx w) + rt_burn_oru (kw_rtx w)
      <= oru_stock (ks_oru s)
         + oru_subsidy (rt_height (kw_rtx w))
         + kt_oru_in (kw_ktx w).
Proof.
  intros s w. unfold oru_solvency_b. apply Z.leb_le.
Qed.

Lemma res_cap_ok_b_reflect :
  forall s,
    res_cap_ok_b s = true <->
    0 <= res_cap (ks_res s) /\
    res_stock (ks_res s) <= res_cap (ks_res s).
Proof.
  intros s. unfold res_cap_ok_b.
  rewrite andb_true_iff, !Z.leb_le. tauto.
Qed.

Lemma state_ok_b_reflect :
  forall s w,
    state_ok_b s w = true <->
    kt_oru_out (kw_ktx w) + rt_burn_oru (kw_rtx w)
      <= oru_stock (ks_oru s)
         + oru_subsidy (rt_height (kw_rtx w))
         + kt_oru_in (kw_ktx w) /\
    0 <= res_cap (ks_res s) /\
    res_stock (ks_res s) <= res_cap (ks_res s).
Proof.
  intros s w. unfold state_ok_b.
  rewrite andb_true_iff, oru_solvency_b_reflect, res_cap_ok_b_reflect.
  tauto.
Qed.

Lemma path_a_checks_b_true_iff :
  forall w,
    path_a_checks_b w = true <->
    UTXOValidator.check_utxo (kw_tx w) = true /\
    check_oru (kw_ktx w) = true /\
    check_res (kw_rtx w) = true /\
    ScriptValidator.check_script (kw_script w) = true /\
    check_pow (kw_pow w) = true /\
    check_asert (kw_asert w) = true.
Proof.
  intros w. unfold path_a_checks_b.
  repeat rewrite andb_true_iff. tauto.
Qed.

Theorem computed_ok_reflect :
  forall s w,
    computed_ok s w <-> computed_ok_b s w = true.
Proof.
  intros s w. unfold computed_ok, computed_ok_b.
  rewrite andb_true_iff. tauto.
Qed.

(** [input_ok] on a computed input, restated over [computed_ok_b].

    Equivalent to the previous nine-hypothesis form: the four decision
    fields hold because the validators returned [true]; the six numeric
    fields hold by [numeric_side_conditions]; the three state facts are
    the boolean [state_ok_b]. This implication is **one way**. The
    converse [input_ok s (kernel_input_of w) -> computed_ok_b s w = true]
    is false, because [check_oru] / [check_res] are stronger than
    [input_ok] and are not stored on the Path B record. *)
Theorem input_ok_of_checks :
  forall (s : KernelState) (w : KernelWitness),
    computed_ok_b s w = true ->
    input_ok s (kernel_input_of w).
Proof.
  intros s w Hcomp.
  apply computed_ok_reflect in Hcomp.
  destruct Hcomp as [Hpath Hstate].
  apply path_a_checks_b_true_iff in Hpath.
  destruct Hpath as [Hutxo [Horu [Hres [Hscript [Hpow Hasert]]]]].
  apply state_ok_b_reflect in Hstate.
  destruct Hstate as [Hsolv [Hcap Hstock]].
  pose proof (numeric_side_conditions w Horu Hres)
    as [Hin [Hout [Hfee [Hburn [Hsub Hmint]]]]].
  unfold input_ok, kernel_input_of in *. simpl in *.
  repeat split; assumption.
Qed.

(** The step function therefore has a computed input: for any witness
    whose checks pass on a well-formed state, the transition relation
    relates [s] to the state the kernel function produces. *)
Corollary kernel_apply_of_checks :
  forall (s : KernelState) (w : KernelWitness),
    computed_ok_b s w = true ->
    kernel_apply s (kernel_input_of w)
                 (kernel_step_of s (kernel_input_of w)).
Proof.
  intros s w Hcomp.
  apply kapp_fun.
  apply input_ok_of_checks.
  exact Hcomp.
Qed.

(** * The bridge is not vacuous

    [input_ok_of_checks] would be worthless if no witness could pass
    all six checks at once. One does. The values are the smallest that
    clear each bound rather than round numbers: [rt_burn_oru] is 1024
    because the RES rate bound at height 0 reads
    [mint * res_destroy_pole_sq 0 <= Z.shiftl burn 40], and
    [res_destroy_pole_sq 0] is [2^50], so a unit mint needs [2^10]
    burned and no less. *)
Definition sample_witness : KernelWitness := {|
  kw_tx := {| tx_id := 1%nat :: nil;
              tx_in := {| op_txid := 2%nat :: nil; op_index := 0%nat |} :: nil;
              tx_out := {| txo_value := 50 * COIN; txo_spk := nil |} :: nil;
              tx_coinbase := false |};
  kw_ktx := {| kt_dom := DomORU;
               kt_oru_in := 100; kt_oru_out := 90;
               kt_res_in := 0; kt_res_out := 0;
               kt_old_supply := 0; kt_subsidy := 0 |};
  kw_rtx := {| rt_coinbase := false; rt_mint_ok := true;
               rt_mint_amount := 1; rt_height := 0;
               rt_burn_oru := 1024; rt_dom := DomRES;
               rt_res_in := 0; rt_res_out := 0; rt_oru_in := 0 |};
  kw_script := {| ss_script := witness_v0_script;
                  ss_expected := classify_script witness_v0_script |};
  kw_pow := {| pow_digest_z := 1; pow_target := 1000; pow_compact_t := 0 |};
  kw_asert := {| asert_te := -1; asert_tau := 600;
                 asert_expected_q := exp_q_raw (-1) 600 |}
|}.

Theorem sample_witness_passes_every_check :
  UTXOValidator.check_utxo (kw_tx sample_witness) = true /\
  check_oru (kw_ktx sample_witness) = true /\
  check_res (kw_rtx sample_witness) = true /\
  ScriptValidator.check_script (kw_script sample_witness) = true /\
  check_pow (kw_pow sample_witness) = true /\
  check_asert (kw_asert sample_witness) = true.
Proof. repeat split; vm_compute; reflexivity. Qed.

Definition sample_kstate : KernelState := {|
  ks_utxo := {| utxo_wf_flag := true |};
  ks_econ := {| econ_fees := 0; econ_burn := 0; econ_subsidy := 0 |};
  ks_res := {| res_stock := 0; res_cap := RES_MAX_SUPPLY |};
  ks_oru := {| oru_stock := 100000 |};
  ks_script := {| script_class_ok := true |};
  ks_pow := {| pow_ok := true |};
  ks_asert := {| asert_ok := true |}
|}.

Theorem sample_computed_ok :
  computed_ok_b sample_kstate sample_witness = true.
Proof. vm_compute. reflexivity. Qed.

(** The hypotheses of [input_ok_of_checks] are jointly satisfiable, so
    the conclusion is reached by something and not by nothing. *)
Theorem bridge_is_inhabited :
  input_ok sample_kstate (kernel_input_of sample_witness).
Proof.
  apply input_ok_of_checks.
  exact sample_computed_ok.
Qed.

(** * Computed examples used by inhabitance / rejection lemmas

    These are [KernelWitness] values, not free Path B records. Every
    [KernelInput] derived from them goes through [kernel_input_of].
    They are not production checkers and not a unique-entry claim. *)

Definition zero_ktx : kernel_tx := {|
  kt_dom := DomORU;
  kt_oru_in := 0; kt_oru_out := 0;
  kt_res_in := 0; kt_res_out := 0;
  kt_old_supply := 0; kt_subsidy := 0
|}.

Definition zero_flow_rtx (h mint burn : Z) : res_tx := {|
  rt_coinbase := false; rt_mint_ok := false;
  rt_mint_amount := mint; rt_height := h;
  rt_burn_oru := burn; rt_dom := DomORU;
  rt_res_in := 0; rt_res_out := 0; rt_oru_in := 0
|}.

Definition witness_from_flow
    (ktx : kernel_tx) (rtx : res_tx) (asert : ASERTInput) : KernelWitness := {|
  kw_tx := kw_tx sample_witness;
  kw_ktx := ktx;
  kw_rtx := rtx;
  kw_script := kw_script sample_witness;
  kw_pow := kw_pow sample_witness;
  kw_asert := asert
|}.

Definition idle_witness : KernelWitness :=
  witness_from_flow zero_ktx (zero_flow_rtx 0 0 0) (kw_asert sample_witness).

Definition issuance_witness : KernelWitness :=
  witness_from_flow zero_ktx (zero_flow_rtx 1 0 0) (kw_asert sample_witness).

Definition overspend_witness : KernelWitness :=
  witness_from_flow zero_ktx (zero_flow_rtx 0 0 1) (kw_asert sample_witness).

Definition mint_one_witness : KernelWitness :=
  witness_from_flow zero_ktx (zero_flow_rtx 0 1 0) (kw_asert sample_witness).

Definition drain_witness : KernelWitness :=
  witness_from_flow
    {| kt_dom := DomORU;
       kt_oru_in := 0; kt_oru_out := 200000;
       kt_res_in := 0; kt_res_out := 0;
       kt_old_supply := 0; kt_subsidy := 0 |}
    (zero_flow_rtx 0 0 0) (kw_asert sample_witness).

Definition asert_fail_witness : KernelWitness :=
  witness_from_flow zero_ktx (zero_flow_rtx 0 0 0)
    {| asert_te := 0; asert_tau := 600; asert_expected_q := 0 |}.

Lemma flags_of_passing_asert_witness :
  forall ktx rtx,
    let i := kernel_input_of
               (witness_from_flow ktx rtx (kw_asert sample_witness)) in
    ki_utxo_ok i = true /\
    ki_script_ok i = true /\
    ki_pow_ok i = true /\
    ki_asert_ok i = true.
Proof.
  intros ktx rtx.
  pose proof sample_witness_passes_every_check
    as [Hu [_ [_ [Hs [Hp Ha]]]]].
  unfold kernel_input_of, witness_from_flow. simpl.
  repeat split; assumption.
Qed.

Lemma idle_witness_flow :
  let i := kernel_input_of idle_witness in
  ki_incoming i = 0 /\
  ki_outgoing i = 0 /\
  ki_fees i = 0 /\
  ki_burn i = 0 /\
  ki_subsidy i = 0 /\
  ki_res_mint i = 0.
Proof.
  unfold idle_witness, witness_from_flow, zero_ktx, zero_flow_rtx,
         kernel_input_of, ORUConservation.fee, oru_subsidy,
         ORU_GENESIS_BLOCK_SUBSIDY.
  simpl. repeat split; reflexivity.
Qed.

Lemma idle_witness_delta :
  oru_delta (kernel_input_of idle_witness) = 0.
Proof.
  pose proof idle_witness_flow as [Hin [Hout [_ [Hburn [Hsub _]]]]].
  unfold oru_delta. rewrite Hin, Hout, Hburn, Hsub. lia.
Qed.

Lemma issuance_witness_flow :
  let i := kernel_input_of issuance_witness in
  ki_incoming i = 0 /\
  ki_outgoing i = 0 /\
  ki_fees i = 0 /\
  ki_burn i = 0 /\
  ki_subsidy i = oru_subsidy 1 /\
  ki_res_mint i = 0.
Proof.
  unfold issuance_witness, witness_from_flow, zero_ktx, zero_flow_rtx,
         kernel_input_of, ORUConservation.fee.
  simpl. repeat split; reflexivity.
Qed.

Lemma issuance_witness_delta :
  oru_delta (kernel_input_of issuance_witness) = oru_subsidy 1.
Proof.
  pose proof issuance_witness_flow as [Hin [Hout [_ [Hburn [Hsub _]]]]].
  unfold oru_delta. rewrite Hin, Hout, Hburn, Hsub. lia.
Qed.

Lemma overspend_witness_flow :
  let i := kernel_input_of overspend_witness in
  ki_incoming i = 0 /\
  ki_outgoing i = 0 /\
  ki_fees i = 0 /\
  ki_burn i = 1 /\
  ki_subsidy i = 0 /\
  ki_res_mint i = 0.
Proof.
  unfold overspend_witness, witness_from_flow, zero_ktx, zero_flow_rtx,
         kernel_input_of, ORUConservation.fee, oru_subsidy,
         ORU_GENESIS_BLOCK_SUBSIDY.
  simpl. repeat split; reflexivity.
Qed.

Lemma mint_one_witness_flow :
  let i := kernel_input_of mint_one_witness in
  ki_incoming i = 0 /\
  ki_outgoing i = 0 /\
  ki_fees i = 0 /\
  ki_burn i = 0 /\
  ki_subsidy i = 0 /\
  ki_res_mint i = 1.
Proof.
  unfold mint_one_witness, witness_from_flow, zero_ktx, zero_flow_rtx,
         kernel_input_of, ORUConservation.fee, oru_subsidy,
         ORU_GENESIS_BLOCK_SUBSIDY.
  simpl. repeat split; reflexivity.
Qed.

Lemma drain_witness_delta :
  oru_delta (kernel_input_of drain_witness) = -200000.
Proof.
  unfold drain_witness, witness_from_flow, zero_flow_rtx, kernel_input_of,
         oru_delta, ORUConservation.fee, oru_subsidy, ORU_GENESIS_BLOCK_SUBSIDY.
  simpl. reflexivity.
Qed.

Lemma asert_fail_flag_down :
  ki_asert_ok (kernel_input_of asert_fail_witness) = false /\
  ki_utxo_ok (kernel_input_of asert_fail_witness) = true /\
  ki_script_ok (kernel_input_of asert_fail_witness) = true /\
  ki_pow_ok (kernel_input_of asert_fail_witness) = true.
Proof.
  vm_compute. repeat split; reflexivity.
Qed.
