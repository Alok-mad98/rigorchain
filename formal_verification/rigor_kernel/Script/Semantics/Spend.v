(** RigorKernel.Script.Semantics.Spend — spendability by script class.

    Scope: consensus kernel only.

    Rigor has no opcode VM. Four locking shapes plus two reject
    classes. This module is the spend matrix, not an interpreter.

    C++ cite (confirmed before freezing the T_MINT row):
      [core/src/consensus/tx_verify.cpp:185]
        IsConvergenceScript → bad-txns-spend-convergence
      [core/src/script/predicate_verify.cpp]
        IsResMintScript → VerifyResMintSpend (scriptSig <sig> <pk>,
          Ed25519, Hash160 match, script locktime)
        else witness v0 / v1 → VerifyWitnessProgram + Ed25519
        else SCRIPT_ERR_RIGOR_SCRIPT_TYPE
      null-data / nonstandard also fail the asset-input gate
        (ASSET_NONE / ASSET_INVALID).

    [sig_ok] is an argument, not Ed25519. Hash160, mint-script
    locktime, SCRIPT_VERIFY_WITNESS, and empty-scriptSig checks
    are not modelled. Coin-level maturity is [Kernel.Tx.Maturity],
    not this matrix.

    [Witness] here is abstract authorization material (C++ witness
    stack or mint scriptSig). Not a VM stack.

    NOT CLAIMED:
      opcode interpreter / EvalScript
      Ed25519 unforgeability (OP-C1)
      classifier ≡ rigor_classify.cpp (C-7)
      ConnectBlock / valid_tx hook
*)
From Coq Require Import Bool.Bool.
From RigorKernel Require Import Script.ScriptTypes.
From RigorKernel Require Import Script.ScriptClassifier.

Definition Witness := Script.

(** Auth classes: native witness v0 / v1, and the mint locking
    program (C++ [VerifyResMintSpend]). *)
Definition is_auth_class (c : ScriptType) : bool :=
  match c with
  | T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => true
  | _ => false
  end.

Definition spendable_by_class
    (sig_ok : Witness -> Script -> bool)
    (cls : ScriptType) (wit : Witness) (script : Script) : bool :=
  match cls with
  | T_CONVERGE => false
  | T_NULL_DATA => false
  | T_NONSTANDARD => false
  | T_P2PKH => false
  | T_P2SH => false
  | T_WITNESS_V0 | T_WITNESS_V1 | T_MINT => sig_ok wit script
  end.

Definition script_spend_ok
    (sig_ok : Witness -> Script -> bool)
    (script : Script) (wit : Witness) : bool :=
  spendable_by_class sig_ok (classify_script script) wit script.

(** Statement: a converge lock cannot be spent, for any [sig_ok].
    Assumptions: [classify_script s = T_CONVERGE].
    Consensus kernel only: yes. Not ConnectBlock. *)
Theorem converge_unspendable :
  forall (sig_ok : Witness -> Script -> bool) s w,
    classify_script s = T_CONVERGE ->
    script_spend_ok sig_ok s w = false.
Proof.
  intros sig_ok s w Hc.
  unfold script_spend_ok, spendable_by_class.
  rewrite Hc.
  reflexivity.
Qed.

(** Statement: OP_RETURN / null-data cannot be spent.
    Assumptions: [classify_script s = T_NULL_DATA]. *)
Theorem nulldata_unspendable :
  forall (sig_ok : Witness -> Script -> bool) s w,
    classify_script s = T_NULL_DATA ->
    script_spend_ok sig_ok s w = false.
Proof.
  intros sig_ok s w Hc.
  unfold script_spend_ok, spendable_by_class.
  rewrite Hc.
  reflexivity.
Qed.

(** Statement: a nonstandard lock cannot be spent.
    Assumptions: [classify_script s = T_NONSTANDARD]. *)
Theorem nonstandard_unspendable :
  forall (sig_ok : Witness -> Script -> bool) s w,
    classify_script s = T_NONSTANDARD ->
    script_spend_ok sig_ok s w = false.
Proof.
  intros sig_ok s w Hc.
  unfold script_spend_ok, spendable_by_class.
  rewrite Hc.
  reflexivity.
Qed.

(** Statement: witness v0 / v1 and mint require [sig_ok].
    Assumptions: the classifier returns an auth class and
    [sig_ok w s = false]. Inline: isolation must not share a
    lemma and still let this go through. *)
Theorem witness_requires_sig_ok :
  forall (sig_ok : Witness -> Script -> bool) s w,
    is_auth_class (classify_script s) = true ->
    sig_ok w s = false ->
    script_spend_ok sig_ok s w = false.
Proof.
  intros sig_ok s w Hcls Hsig.
  unfold script_spend_ok, spendable_by_class, is_auth_class in *.
  destruct (classify_script s); try discriminate; exact Hsig.
Qed.

(** Statement: an auth-class lock with [sig_ok] succeeds.
    Assumptions: classifier returns an auth class and
    [sig_ok w s = true]. Not “signatures are unforgeable”. *)
Theorem witness_sig_ok_spends :
  forall (sig_ok : Witness -> Script -> bool) s w,
    is_auth_class (classify_script s) = true ->
    sig_ok w s = true ->
    script_spend_ok sig_ok s w = true.
Proof.
  intros sig_ok s w Hcls Hsig.
  unfold script_spend_ok, spendable_by_class, is_auth_class in *.
  destruct (classify_script s); try discriminate; exact Hsig.
Qed.

(** Statement: spend consults [classify_script], not a second
    recognizer ladder. *)
Theorem classifier_then_spend :
  forall (sig_ok : Witness -> Script -> bool) s w,
    script_spend_ok sig_ok s w =
    spendable_by_class sig_ok (classify_script s) w s.
Proof.
  intros sig_ok s w.
  unfold script_spend_ok.
  reflexivity.
Qed.
