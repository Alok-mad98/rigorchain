(** RigorKernel.Kernel.Chain.ChainTransition — multi-block kernel runs.

    Scope: consensus kernel only.

    Every consensus theorem in this package used to be about a single
    step. `kernel_step_preserves_invariants` says that one acceptable
    input takes a good state to a good state; nothing said what
    happens when a node applies a thousand of them in a row. That gap
    is the distance between "the step rule is sound" and "the
    consensus rules are correct", and this file closes the kernel half
    of it.

    The load-bearing design decision is where acceptability is
    evaluated. [input_ok s i] is not a property of [i] alone: it reads
    [oru_stock (ks_oru s)] and [res_cap (ks_res s)], so what a block
    is allowed to spend depends on what the blocks before it did.
    Writing the chain hypothesis as [Forall (input_ok s0) is] would
    check every block against the INITIAL state, which is both wrong
    and much easier to satisfy. [chain_ok] below walks the running
    state instead, so each input is judged against the state its
    predecessors actually produced.

    NOT CLAIMED: reorganisation, fork choice, block relay, mempool
    policy, C++ ConnectBlock, chain-level extraction.
*)
From Coq Require Import Lists.List.
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.
From RigorKernel Require Import Kernel.UnifiedState.
From RigorKernel Require Import Kernel.UnifiedTransition.
From RigorKernel Require Import Kernel.InvariantBundle.
From RigorKernel.proofs Require Import KernelTransitionSoundness.

Import ListNotations.
Local Open Scope Z_scope.

(** * Running a list of inputs *)

(** The state after applying every input in order. *)
Fixpoint chain_step_of (s : KernelState) (is : list KernelInput)
  : KernelState :=
  match is with
  | [] => s
  | i :: rest => chain_step_of (kernel_step_of s i) rest
  end.

(** Acceptability of a whole run, each input judged at the state its
    predecessors produced. This is the definition the audit should
    look at hardest: weakening it to [Forall (input_ok s) is] would
    make every theorem below easier and none of them true of a real
    chain. *)
Fixpoint chain_ok (s : KernelState) (is : list KernelInput) : Prop :=
  match is with
  | [] => True
  | i :: rest => input_ok s i /\ chain_ok (kernel_step_of s i) rest
  end.

(** Every state the run passes through, the start included. *)
Fixpoint chain_states (s : KernelState) (is : list KernelInput)
  : list KernelState :=
  match is with
  | [] => [s]
  | i :: rest => s :: chain_states (kernel_step_of s i) rest
  end.

(** The relational form, built from the single-step relation. *)
Inductive kernel_chain :
    KernelState -> list KernelInput -> KernelState -> Prop :=
| kchain_nil : forall s, kernel_chain s [] s
| kchain_cons : forall s i s' is s'',
    kernel_apply s i s' ->
    kernel_chain s' is s'' ->
    kernel_chain s (i :: is) s''.

(** Reachability: [s'] is some acceptable run away from [s]. *)
Definition reachable (s s' : KernelState) : Prop :=
  exists is : list KernelInput, kernel_chain s is s'.

(** * The chain relation is the graph of the chain function

    Same discipline as [kernel_apply_is_graph] one layer down: state
    the correspondence rather than assert determinism separately.
    Class: A (core verified consensus invariant).
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem kernel_chain_is_graph :
  forall (s s' : KernelState) (is : list KernelInput),
    kernel_chain s is s' <-> chain_ok s is /\ s' = chain_step_of s is.
Proof.
  intros s s' is. revert s s'.
  induction is as [| i rest IH]; intros s s'.
  - split.
    + intros H. inversion H; subst. split; [exact I | reflexivity].
    + intros [_ ->]. apply kchain_nil.
  - split.
    + intros H. inversion H as [| s0 i0 s1 is0 s2 Hstep Hrest]; subst.
      apply kernel_apply_is_graph in Hstep as [Hok ->].
      apply IH in Hrest as [Hchain ->].
      split; [split; assumption | reflexivity].
    + intros [[Hok Hrest] ->].
      eapply kchain_cons.
      * apply kernel_apply_is_graph. split; [exact Hok | reflexivity].
      * apply IH. split; [exact Hrest | reflexivity].
Qed.

(** Determinism over a whole run, inherited from the graph. *)
Theorem kernel_chain_deterministic :
  forall s is s1 s2,
    kernel_chain s is s1 ->
    kernel_chain s is s2 ->
    s1 = s2.
Proof.
  intros s is s1 s2 H1 H2.
  apply kernel_chain_is_graph in H1 as [_ ->].
  apply kernel_chain_is_graph in H2 as [_ ->].
  reflexivity.
Qed.

(** * Splitting and joining runs

    A chain is not a black box between genesis and tip: any prefix is
    itself a run, and appending two runs is running them in sequence.
    Without this, "the chain is valid" could not be checked
    incrementally, which is what a node actually does. *)

Lemma chain_step_of_app :
  forall (is1 is2 : list KernelInput) (s : KernelState),
    chain_step_of s (is1 ++ is2)
      = chain_step_of (chain_step_of s is1) is2.
Proof.
  induction is1 as [| i rest IH]; intros is2 s; simpl; [reflexivity |].
  apply IH.
Qed.

Lemma chain_ok_app :
  forall (is1 is2 : list KernelInput) (s : KernelState),
    chain_ok s (is1 ++ is2)
      <-> chain_ok s is1 /\ chain_ok (chain_step_of s is1) is2.
Proof.
  induction is1 as [| i rest IH]; intros is2 s; simpl.
  - split; [intros H; split; [exact I | exact H] | intros [_ H]; exact H].
  - split.
    + intros [Hok Hrest]. apply IH in Hrest as [H1 H2].
      split; [split; assumption | exact H2].
    + intros [[Hok H1] H2]. split; [exact Hok |].
      apply IH. split; assumption.
Qed.

Theorem kernel_chain_app :
  forall s s' s'' is1 is2,
    kernel_chain s is1 s' ->
    kernel_chain s' is2 s'' ->
    kernel_chain s (is1 ++ is2) s''.
Proof.
  intros s s' s'' is1 is2 H1 H2.
  apply kernel_chain_is_graph in H1 as [Hok1 ->].
  apply kernel_chain_is_graph in H2 as [Hok2 ->].
  apply kernel_chain_is_graph.
  split; [apply chain_ok_app; split; assumption |].
  symmetry. apply chain_step_of_app.
Qed.

(** The three numeric facts the acceptability condition asks about,
    pulled out from behind the invariant names so [lia] can see them. *)
Lemma invariants_numeric :
  forall s : KernelState,
    kernel_invariants s ->
    0 <= oru_stock (ks_oru s) /\
    0 <= res_stock (ks_res s) /\
    res_stock (ks_res s) <= res_cap (ks_res s).
Proof.
  intros s Hinv.
  unfold kernel_invariants, oru_conservation, res_bound in Hinv.
  destruct Hinv as [_ [Ho [[Hr0 Hrc] _]]].
  repeat split; assumption.
Qed.

(** * Invariant preservation over a whole run

    This is the chain-level statement the package was missing. It is
    proved by induction on the run, not by iterating the single-step
    result at the call site, so the number of blocks is arbitrary.
    Class: A (core verified consensus invariant).
    Assumptions: the start state satisfies the invariants and the run
    is acceptable at every step.
    Coq consensus kernel only: yes. *)
Theorem kernel_chain_preserves_invariants :
  forall (is : list KernelInput) (s : KernelState),
    kernel_invariants s ->
    chain_ok s is ->
    kernel_invariants (chain_step_of s is).
Proof.
  induction is as [| i rest IH]; intros s Hinv Hok; simpl in *.
  - exact Hinv.
  - destruct Hok as [Hstep Hrest].
    apply IH; [| exact Hrest].
    apply step_of_preserves; assumption.
Qed.

(** The endpoint is not the only thing that has to be good: a node
    serves every intermediate state to its callers. This says all of
    them satisfy the invariants, which is strictly stronger than the
    theorem above and is what "the chain is valid" should mean. *)
Theorem kernel_chain_states_invariant :
  forall (is : list KernelInput) (s : KernelState),
    kernel_invariants s ->
    chain_ok s is ->
    Forall kernel_invariants (chain_states s is).
Proof.
  induction is as [| i rest IH]; intros s Hinv Hok; simpl in *.
  - apply Forall_cons; [exact Hinv | apply Forall_nil].
  - destruct Hok as [Hstep Hrest].
    apply Forall_cons; [exact Hinv |].
    apply IH; [apply step_of_preserves; assumption | exact Hrest].
Qed.

(** Stated over reachability, which is the form a safety property is
    usually quoted in: nothing an acceptable run can reach violates
    the invariants. *)
Theorem reachable_preserves_invariants :
  forall s s' : KernelState,
    kernel_invariants s ->
    reachable s s' ->
    kernel_invariants s'.
Proof.
  intros s s' Hinv [is Hchain].
  apply kernel_chain_is_graph in Hchain as [Hok ->].
  apply kernel_chain_preserves_invariants; assumption.
Qed.

(** * Conservation across the whole run

    Single-step conservation is definitional unfolding. Over a run it
    is a real accounting statement: the ORU stock at the tip differs
    from the stock at the start by exactly the sum of the declared
    per-block flows, with no term that depends on the length of the
    chain or on anything else. A hidden per-block creation of any
    size would break this. *)

Fixpoint sum_oru_delta (is : list KernelInput) : Z :=
  match is with
  | [] => 0
  | i :: rest => oru_delta i + sum_oru_delta rest
  end.

Fixpoint sum_subsidy_in (is : list KernelInput) : Z :=
  match is with
  | [] => 0
  | i :: rest => ki_subsidy i + sum_subsidy_in rest
  end.

Fixpoint sum_incoming (is : list KernelInput) : Z :=
  match is with
  | [] => 0
  | i :: rest => ki_incoming i + sum_incoming rest
  end.

Fixpoint sum_outgoing (is : list KernelInput) : Z :=
  match is with
  | [] => 0
  | i :: rest => ki_outgoing i + sum_outgoing rest
  end.

Fixpoint sum_burn_in (is : list KernelInput) : Z :=
  match is with
  | [] => 0
  | i :: rest => ki_burn i + sum_burn_in rest
  end.

(** Class: A (core verified consensus invariant).
    Assumptions: none — this is an identity about the fold, so it
    holds even of runs the kernel would reject.
    Coq consensus kernel only: yes. *)
Theorem chain_oru_conservation :
  forall (is : list KernelInput) (s : KernelState),
    oru_stock (ks_oru (chain_step_of s is))
      = oru_stock (ks_oru s) + sum_oru_delta is.
Proof.
  induction is as [| i rest IH]; intros s; simpl; [lia |].
  rewrite IH. simpl. lia.
Qed.

(** The same identity with the flows spelled out, so the statement can
    be read without unfolding [oru_delta]: what comes out is subsidy
    plus transfers in, minus transfers out, minus burn. Nothing else. *)
Theorem chain_no_hidden_creation :
  forall (is : list KernelInput) (s : KernelState),
    oru_stock (ks_oru (chain_step_of s is)) - oru_stock (ks_oru s)
      = sum_subsidy_in is + sum_incoming is
        - sum_outgoing is - sum_burn_in is.
Proof.
  intros is s. rewrite chain_oru_conservation.
  assert (Hsum : forall js : list KernelInput,
             sum_oru_delta js
               = sum_subsidy_in js + sum_incoming js
                 - sum_outgoing js - sum_burn_in js).
  { induction js as [| j rest IH]; simpl; [reflexivity |].
    unfold oru_delta. rewrite IH. lia. }
  rewrite Hsum. lia.
Qed.

(** * RES over a run

    The cap is what the schedule promises will never be exceeded, and
    a single step obviously cannot exceed it because the transition
    takes a [Z.min]. The question a chain asks is different: can the
    cap itself drift, so that a long run ends up measured against a
    looser bound than it started with. It cannot — [res_transition]
    copies [res_cap] through untouched. *)

Theorem chain_res_cap_fixed :
  forall (is : list KernelInput) (s : KernelState),
    res_cap (ks_res (chain_step_of s is)) = res_cap (ks_res s).
Proof.
  induction is as [| i rest IH]; intros s; simpl; [reflexivity |].
  rewrite IH. reflexivity.
Qed.

(** Issuance never runs backwards along a run. *)
Theorem chain_res_monotone :
  forall (is : list KernelInput) (s : KernelState),
    kernel_invariants s ->
    chain_ok s is ->
    res_stock (ks_res s) <= res_stock (ks_res (chain_step_of s is)).
Proof.
  induction is as [| i rest IH]; intros s Hinv Hok; simpl in *;
    [lia |].
  destruct Hok as [Hstep Hrest].
  assert (Hnext : kernel_invariants (kernel_step_of s i))
    by (apply step_of_preserves; assumption).
  specialize (IH (kernel_step_of s i) Hnext Hrest).
  eapply Z.le_trans; [| exact IH].
  apply invariants_numeric in Hinv as [_ [_ Hrc]].
  unfold kernel_step_of, res_transition. simpl.
  apply Z.min_glb; lia.
Qed.

(** The cap holds at the tip of any acceptable run, with the bound
    measured against the cap the run STARTED with. Combining
    [chain_res_cap_fixed] with the invariant is what rules out the
    reading where a chain quietly raises its own ceiling. *)
Theorem chain_res_capped :
  forall (is : list KernelInput) (s : KernelState),
    kernel_invariants s ->
    chain_ok s is ->
    res_stock (ks_res (chain_step_of s is)) <= res_cap (ks_res s).
Proof.
  intros is s Hinv Hok.
  rewrite <- (chain_res_cap_fixed is s).
  apply (kernel_chain_preserves_invariants is s Hinv Hok).
Qed.

(** * The chain hypothesis is satisfiable, and it is not trivial

    A theorem of the form [chain_ok s is -> P] is worth nothing if
    nothing satisfies [chain_ok], and worth little if everything does.
    Both directions are checked here rather than assumed. *)

(** All flags set, all amounts zero: the smallest input the kernel
    accepts. *)
Definition idle_input : KernelInput :=
  {| ki_utxo_ok := true;
     ki_incoming := 0;
     ki_outgoing := 0;
     ki_fees := 0;
     ki_burn := 0;
     ki_subsidy := 0;
     ki_res_mint := 0;
     ki_script_ok := true;
     ki_pow_ok := true;
     ki_asert_ok := true |}.

(** A block that issues [n] units of subsidy and spends nothing. *)
Definition subsidy_input (n : Z) : KernelInput :=
  {| ki_utxo_ok := true;
     ki_incoming := 0;
     ki_outgoing := 0;
     ki_fees := 0;
     ki_burn := 0;
     ki_subsidy := n;
     ki_res_mint := 0;
     ki_script_ok := true;
     ki_pow_ok := true;
     ki_asert_ok := true |}.

Lemma idle_input_ok :
  forall s : KernelState, kernel_invariants s -> input_ok s idle_input.
Proof.
  intros s Hinv.
  apply invariants_numeric in Hinv as [Ho [Hr0 Hrc]].
  unfold input_ok, idle_input. simpl.
  repeat split; try reflexivity; lia.
Qed.

Lemma subsidy_input_ok :
  forall (s : KernelState) (n : Z),
    kernel_invariants s -> 0 <= n -> input_ok s (subsidy_input n).
Proof.
  intros s n Hinv Hn.
  apply invariants_numeric in Hinv as [Ho [Hr0 Hrc]].
  unfold input_ok, subsidy_input. simpl.
  repeat split; try reflexivity; lia.
Qed.

(** Runs of every length exist. Without this, everything above could
    be vacuously true past the first block. *)
Theorem chain_ok_inhabited :
  forall (n : nat) (s : KernelState),
    kernel_invariants s -> chain_ok s (repeat idle_input n).
Proof.
  induction n as [| n IH]; intros s Hinv; simpl; [exact I |].
  split; [apply idle_input_ok, Hinv |].
  apply IH, step_of_preserves; [exact Hinv | apply idle_input_ok, Hinv].
Qed.

(** And runs that actually move money exist, so the satisfiability
    above is not an artefact of the all-zero input. *)
Theorem chain_ok_inhabited_with_issuance :
  forall (n : nat) (s : KernelState),
    kernel_invariants s ->
    chain_ok s (repeat (subsidy_input 50) n) /\
    oru_stock (ks_oru (chain_step_of s (repeat (subsidy_input 50) n)))
      = oru_stock (ks_oru s) + 50 * Z.of_nat n.
Proof.
  intros n s Hinv. split.
  - revert s Hinv.
    induction n as [| n IH]; intros s Hinv; simpl; [exact I |].
    split; [apply subsidy_input_ok; [exact Hinv | lia] |].
    apply IH, step_of_preserves;
      [exact Hinv | apply subsidy_input_ok; [exact Hinv | lia]].
  - rewrite chain_oru_conservation.
    assert (Hsum : forall m : nat,
               sum_oru_delta (repeat (subsidy_input 50) m)
                 = 50 * Z.of_nat m).
    { induction m as [| m IHm]; [cbn; lia |].
      rewrite Nat2Z.inj_succ.
      cbn [repeat sum_oru_delta].
      rewrite IHm. unfold oru_delta.
      cbn [ki_subsidy ki_incoming ki_outgoing ki_burn subsidy_input].
      lia. }
    rewrite Hsum. reflexivity.
Qed.

(** The other direction: the hypothesis rejects things. A block that
    spends more than the stock it is applied to is not acceptable, and
    critically it is the RUNNING stock that decides, which is the
    property a [Forall (input_ok s0) is] formulation would lose. *)
Theorem chain_ok_rejects_overspend :
  forall s : KernelState,
    oru_stock (ks_oru s) = 0 ->
    ~ chain_ok s [subsidy_input 0;
                  {| ki_utxo_ok := true;
                     ki_incoming := 0;
                     ki_outgoing := 1;
                     ki_fees := 0;
                     ki_burn := 0;
                     ki_subsidy := 0;
                     ki_res_mint := 0;
                     ki_script_ok := true;
                     ki_pow_ok := true;
                     ki_asert_ok := true |}].
Proof.
  intros s Hzero [_ [Hbad _]].
  unfold input_ok in Hbad. simpl in Hbad.
  destruct Hbad as [_ [_ [_ [_ [_ [_ [_ [_ [_ [_ [Hbal _]]]]]]]]]]].
  unfold kernel_step_of, oru_transition, oru_delta in Hbal. simpl in Hbal.
  rewrite Hzero in Hbal. lia.
Qed.

(** The same overspend becomes acceptable once an earlier block has
    funded it. This is the pair that shows [chain_ok] is reading the
    running state: the identical input is rejected above and accepted
    here, and the only difference is what came before it. *)
Theorem chain_ok_accepts_funded_spend :
  forall s : KernelState,
    kernel_invariants s ->
    chain_ok s [subsidy_input 1;
                {| ki_utxo_ok := true;
                   ki_incoming := 0;
                   ki_outgoing := 1;
                   ki_fees := 0;
                   ki_burn := 0;
                   ki_subsidy := 0;
                   ki_res_mint := 0;
                   ki_script_ok := true;
                   ki_pow_ok := true;
                   ki_asert_ok := true |}].
Proof.
  intros s Hinv.
  assert (Hfst : input_ok s (subsidy_input 1))
    by (apply subsidy_input_ok; [exact Hinv | lia]).
  split; [exact Hfst |].
  set (s1 := kernel_step_of s (subsidy_input 1)).
  assert (Hnext : kernel_invariants s1)
    by (apply step_of_preserves; assumption).
  apply invariants_numeric in Hnext as [_ [Hr0 Hrc]].
  apply invariants_numeric in Hinv as [Ho _].
  (* The funding block raised the stock by exactly one unit, which is
     what makes the spend below fit. *)
  assert (Hstock : oru_stock (ks_oru s1) = oru_stock (ks_oru s) + 1).
  { unfold s1, kernel_step_of, oru_transition, oru_delta.
    cbn [ks_oru oru_stock ki_subsidy ki_incoming ki_outgoing ki_burn
         subsidy_input].
    lia. }
  split; [| exact I].
  unfold input_ok.
  cbn [ki_utxo_ok ki_script_ok ki_pow_ok ki_asert_ok ki_incoming
       ki_outgoing ki_fees ki_burn ki_subsidy ki_res_mint].
  repeat split; try reflexivity; lia.
Qed.
