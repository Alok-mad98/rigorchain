(** RigorKernel.Script.ScriptClassifier — Phase A classification model.

    Scope: consensus kernel only.

    Decision ladder mirrors [ClassifyRigorTxout] in
    [core/src/script/rigor_classify.cpp]:

      1. witness program?
           v0 ∧ |prog|=20           → T_WITNESS_V0
           v1 ∧ |prog|=32 ∧ native  → T_WITNESS_V1
           otherwise                → T_NONSTANDARD   (* ladder terminates *)
      2. mint shape                 → T_MINT
      3. converge shape             → T_CONVERGE
      4. 0x6a ∧ push-only tail      → T_NULL_DATA
      5. otherwise                  → T_NONSTANDARD

    An unsupported witness program NEVER falls through to mint / converge.

    There is no Script VM, no opcode interpreter, no signature check.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      classifier_refinement against rigor_classify.cpp
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Lia.
Import ListNotations.
From RigorKernel Require Import Core.Bytes.
From RigorKernel Require Import Script.ScriptTypes.

(** mirrors core/src/script/rigor_bytes.h *)
Definition RIGOR_PUSH_0 : byte := 0%nat.
Definition RIGOR_PUSH_1 : byte := 81%nat.
Definition RIGOR_PUSH_16 : byte := 96%nat.
Definition RIGOR_MARKER_RETURN : byte := 106%nat.
Definition RIGOR_PUSHBYTES_20 : byte := 20%nat.
Definition RIGOR_PUSHDATA1 : byte := 76%nat.
Definition RIGOR_PUSHDATA2 : byte := 77%nat.
Definition RIGOR_PUSHDATA4 : byte := 78%nat.
Definition RIGOR_MARKER_RES_MINT : byte := 187%nat.
Definition RIGOR_MARKER_CONVERGE : byte := 189%nat.

Definition cvg2_tag : list byte := [67; 86; 71; 50]%nat.
Definition res2_tag : list byte := [82; 69; 83; 50]%nat.

(** * Witness-program container (BIP141-shaped; v1 is not BIP341) *)

Definition witness_version_of (b : byte) : option nat :=
  if Nat.eqb b RIGOR_PUSH_0 then Some 0%nat
  else if andb (Nat.leb RIGOR_PUSH_1 b) (Nat.leb b RIGOR_PUSH_16)
       then Some (b - 80)%nat
       else None.

Definition parse_witness_program (s : Script) : option (nat * list byte) :=
  let n := length s in
  if orb (Nat.ltb n 4%nat) (Nat.ltb 42%nat n) then None
  else
    match s with
    | ver :: plen :: rest =>
        match witness_version_of ver with
        | None => None
        | Some v =>
            if andb (Nat.ltb 0%nat plen) (Nat.eqb (length rest) plen)
            then Some (v, rest)
            else None
        end
    | _ => None
    end.

Definition prefix4_eq (prog tag : list byte) : bool :=
  match prog, tag with
  | a :: b :: c :: d :: _, x :: y :: z :: w :: nil =>
      andb (Nat.eqb a x) (andb (Nat.eqb b y) (andb (Nat.eqb c z) (Nat.eqb d w)))
  | _, _ => false
  end.

(** C++ [IsRigorResNativeWitnessProgram]: 32 bytes, neither CVG2 nor RES2. *)
Definition is_native_res_program (prog : list byte) : bool :=
  andb (Nat.eqb (length prog) 32%nat)
       (andb (negb (prefix4_eq prog cvg2_tag))
             (negb (prefix4_eq prog res2_tag))).

Definition is_supported_witness (s : Script) : bool :=
  match parse_witness_program s with
  | Some (0%nat, prog) => Nat.eqb (length prog) 20%nat
  | Some (1%nat, prog) => is_native_res_program prog
  | _ => false
  end.

(** * Mint / converge / null-data shapes (consulted only after witness) *)

Definition is_mint_script (s : Script) : bool :=
  match s with
  | sn_op :: rest =>
      (* scriptnum prefix: OP_0 or a 1–4 byte push, then PUSHBYTES_20 || pkh || marker *)
      let after_sn :=
        if Nat.eqb sn_op RIGOR_PUSH_0 then Some rest
        else if andb (Nat.leb 1%nat sn_op) (Nat.leb sn_op 4%nat) then
          match split_at sn_op rest with
          | Some (_, tail) => Some tail
          | None => None
          end
        else None
      in
      match after_sn with
      | Some (plen :: body) =>
          if Nat.eqb plen RIGOR_PUSHBYTES_20 then
            match split_at 20%nat body with
            | Some (_, marker_tl) =>
                match marker_tl with
                | op :: nil => Nat.eqb op RIGOR_MARKER_RES_MINT
                | _ => false
                end
            | None => false
            end
          else false
      | _ => false
      end
  | nil => false
  end.

Definition is_converge_script (s : Script) : bool :=
  match s with
  | plen :: rest =>
      andb (Nat.eqb plen RIGOR_PUSHBYTES_20)
           (andb (Nat.eqb (length rest) 21%nat)
                 (match rev rest with
                  | op :: hash_rev =>
                      andb (Nat.eqb op RIGOR_MARKER_CONVERGE)
                           (Nat.eqb (length hash_rev) 20%nat)
                  | nil => false
                  end))
  | nil => false
  end.

Fixpoint is_push_only_n (fuel : nat) (rest : list byte) : bool :=
  match fuel with
  | O => match rest with nil => true | _ => false end
  | S fuel' =>
      match rest with
      | nil => true
      | op :: tail =>
          if Nat.leb op 75%nat then
            match split_at op tail with
            | Some (_, rest') => is_push_only_n fuel' rest'
            | None => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA1 then
            match tail with
            | ln :: data =>
                match split_at ln data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | nil => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA2 then
            match tail with
            | lo :: hi :: data =>
                match split_at (lo + hi * 256)%nat data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA4 then
            match tail with
            | a :: b :: c :: d :: data =>
                match split_at
                        (a + b * 256 + c * 256 * 256 + d * 256 * 256 * 256)%nat
                        data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.leb op RIGOR_PUSH_16 then
            is_push_only_n fuel' tail
          else false
      end
  end.

Definition is_push_only (rest : list byte) : bool :=
  is_push_only_n (length rest) rest.

Definition is_null_data (s : Script) : bool :=
  match s with
  | x :: rest => andb (Nat.eqb x RIGOR_MARKER_RETURN) (is_push_only rest)
  | nil => false
  end.

(** * THE classifier — a total function of script bytes *)

Definition classify_script (s : Script) : ScriptType :=
  match parse_witness_program s with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then T_WITNESS_V0 else T_NONSTANDARD
  | Some (1%nat, prog) =>
      if is_native_res_program prog then T_WITNESS_V1 else T_NONSTANDARD
  | Some _ => T_NONSTANDARD
  | None =>
      if is_mint_script s then T_MINT
      else if is_converge_script s then T_CONVERGE
      else if is_null_data s then T_NULL_DATA
      else T_NONSTANDARD
  end.

Definition classify_asset (s : Script) : AssetClass :=
  asset_of_script_type (classify_script s).

(** * Theorems *)

(** Determinism of a Coq function is not a theorem about the function.
    [classify_script s = classify_script s] closed by [reflexivity]
    used to stand here under the name [classify_deterministic]; it is
    gone. The extensional statement below is the one with content that
    a classifier could in principle fail: it is stated over the script
    bytes, so it is what an implementation has to match.
    Assumptions: [s1 = s2].
    Scope: consensus kernel only. *)
Theorem classify_extensional :
  forall s1 s2 : Script,
    s1 = s2 -> classify_script s1 = classify_script s2.
Proof. intros s1 s2 H. rewrite H. reflexivity. Qed.

(** * Totality and partition

    [classify_total] used to read

      forall s, exists t, classify_script s = t

    witnessed by [classify_script s] and closed by [reflexivity]. That
    holds of every function in Coq, including one that returns
    [T_MINT] for every script. It said nothing about this classifier.

    Totality is only informative together with a range: which types
    can come out, and can each of them actually come out. Both
    directions are below.

    Partition needs the same care. Stated over the fibres --- no
    script classifies as both [T_MINT] and [T_CONVERGE] --- it is
    again automatic, because [classify_script] is a function. The
    content is one level down, in the recognisers: the ladder consults
    mint, then converge, then null-data, and if those shapes could
    overlap then the ORDER of the ladder would be deciding the class.
    [shape_hits_le_one] rules that out, so the order is presentation,
    not semantics. The witness step is the deliberate exception; it
    terminates the ladder and [witness_ladder_terminal] says so. *)

Definition reachable_classes : list ScriptType :=
  [T_WITNESS_V0; T_WITNESS_V1; T_MINT; T_CONVERGE; T_NULL_DATA;
   T_NONSTANDARD].

Theorem classify_total :
  forall s : Script, In (classify_script s) reachable_classes.
Proof.
  intros s. unfold classify_script, reachable_classes.
  destruct (parse_witness_program s) as [[v prog]|].
  - destruct v as [| v'].
    + destruct (Nat.eqb (length prog) 20%nat); simpl; tauto.
    + destruct v' as [| v''].
      * destruct (is_native_res_program prog); simpl; tauto.
      * simpl; tauto.
  - destruct (is_mint_script s); [simpl; tauto |].
    destruct (is_converge_script s); [simpl; tauto |].
    destruct (is_null_data s); simpl; tauto.
Qed.

(** * The recognisers cannot overlap

    Each shape pins down the first byte, and the three first bytes are
    different. This is what makes the ladder a partition rather than a
    priority list hiding an ambiguity. *)

Lemma mint_first_byte :
  forall s : Script,
    is_mint_script s = true ->
    exists b tl, s = b :: tl /\ (b <= 4)%nat.
Proof.
  intros [| b tl] H; unfold is_mint_script in H; [discriminate |].
  exists b, tl. split; [reflexivity |].
  destruct (Nat.eqb b RIGOR_PUSH_0) eqn:Hz.
  - apply Nat.eqb_eq in Hz. unfold RIGOR_PUSH_0 in Hz. lia.
  - destruct (Nat.leb 1%nat b) eqn:Hlo; simpl in H; [| discriminate].
    destruct (Nat.leb b 4%nat) eqn:Hhi; [| discriminate].
    apply Nat.leb_le in Hhi. lia.
Qed.

Lemma converge_first_byte :
  forall s : Script,
    is_converge_script s = true ->
    exists tl, s = RIGOR_PUSHBYTES_20 :: tl.
Proof.
  intros [| b tl] H; unfold is_converge_script in H; [discriminate |].
  apply andb_true_iff in H as [Hp _].
  apply Nat.eqb_eq in Hp. exists tl. rewrite Hp. reflexivity.
Qed.

Lemma null_data_first_byte :
  forall s : Script,
    is_null_data s = true ->
    exists tl, s = RIGOR_MARKER_RETURN :: tl.
Proof.
  intros [| b tl] H; unfold is_null_data in H; [discriminate |].
  apply andb_true_iff in H as [Hp _].
  apply Nat.eqb_eq in Hp. exists tl. rewrite Hp. reflexivity.
Qed.

(** The three first bytes are 0..4, 20 and 106, so no script can match
    two shapes. *)

Theorem mint_not_converge :
  forall s : Script,
    is_mint_script s = true -> is_converge_script s = false.
Proof.
  intros s Hm.
  destruct (is_converge_script s) eqn:Hc; [| reflexivity]. exfalso.
  apply mint_first_byte in Hm as [b [tl [Hs Hb]]].
  apply converge_first_byte in Hc as [tl' Hs'].
  rewrite Hs in Hs'. injection Hs' as Hb' _.
  unfold RIGOR_PUSHBYTES_20 in Hb'. lia.
Qed.

Theorem mint_not_null_data :
  forall s : Script,
    is_mint_script s = true -> is_null_data s = false.
Proof.
  intros s Hm.
  destruct (is_null_data s) eqn:Hn; [| reflexivity]. exfalso.
  apply mint_first_byte in Hm as [b [tl [Hs Hb]]].
  apply null_data_first_byte in Hn as [tl' Hs'].
  rewrite Hs in Hs'. injection Hs' as Hb' _.
  unfold RIGOR_MARKER_RETURN in Hb'. lia.
Qed.

Theorem converge_not_null_data :
  forall s : Script,
    is_converge_script s = true -> is_null_data s = false.
Proof.
  intros s Hc.
  destruct (is_null_data s) eqn:Hn; [| reflexivity]. exfalso.
  apply converge_first_byte in Hc as [tl Hs].
  apply null_data_first_byte in Hn as [tl' Hs'].
  rewrite Hs in Hs'. injection Hs' as Hb' _.
  unfold RIGOR_PUSHBYTES_20, RIGOR_MARKER_RETURN in Hb'. discriminate.
Qed.

(** How many of the three shapes a script matches. *)
Definition shape_hits (s : Script) : nat :=
  (if is_mint_script s then 1 else 0)
  + (if is_converge_script s then 1 else 0)
  + (if is_null_data s then 1 else 0).

Theorem shape_hits_le_one :
  forall s : Script, (shape_hits s <= 1)%nat.
Proof.
  intros s. unfold shape_hits.
  destruct (is_mint_script s) eqn:Hm.
  - rewrite (mint_not_converge s Hm), (mint_not_null_data s Hm). lia.
  - destruct (is_converge_script s) eqn:Hc.
    + rewrite (converge_not_null_data s Hc). lia.
    + destruct (is_null_data s); lia.
Qed.

(** Because the shapes do not overlap, each recogniser decides the
    class on its own: no earlier rung of the ladder can capture a
    script that a later rung recognises. *)
Theorem ladder_order_immaterial :
  forall s : Script,
    parse_witness_program s = None ->
    (is_mint_script s = true -> classify_script s = T_MINT) /\
    (is_converge_script s = true -> classify_script s = T_CONVERGE) /\
    (is_null_data s = true -> classify_script s = T_NULL_DATA).
Proof.
  intros s Hw.
  pose proof (shape_hits_le_one s) as Hle. unfold shape_hits in Hle.
  unfold classify_script. rewrite Hw.
  repeat split; intros Hshape; rewrite Hshape in *.
  - reflexivity.
  - destruct (is_mint_script s); [lia |]. reflexivity.
  - destruct (is_mint_script s); [lia |].
    destruct (is_converge_script s); [lia |]. reflexivity.
Qed.

(** No shape at all, and not a witness program, is exactly the
    residual class. Together with [ladder_order_immaterial] this makes
    the four outcomes a partition of the non-witness scripts. *)
Theorem no_shape_is_nonstandard :
  forall s : Script,
    parse_witness_program s = None ->
    shape_hits s = 0%nat ->
    classify_script s = T_NONSTANDARD.
Proof.
  intros s Hw Hh. unfold shape_hits in Hh.
  unfold classify_script. rewrite Hw.
  destruct (is_mint_script s); [simpl in Hh; lia |].
  destruct (is_converge_script s); [simpl in Hh; lia |].
  destruct (is_null_data s); [simpl in Hh; lia |]. reflexivity.
Qed.

(** Statement: unsupported witness programs return [T_NONSTANDARD].
    The ladder terminates at step 1: unknown witness never falls
    through to mint or converge.
    Assumptions: [parse_witness_program s] succeeds and the program
    is not a supported v0/20 or v1/32-native witness.
    Scope: consensus kernel only. *)
Theorem witness_ladder_terminal :
  forall s : Script,
    parse_witness_program s <> None ->
    is_supported_witness s = false ->
    classify_script s = T_NONSTANDARD.
Proof.
  intros s Hw Hs.
  unfold classify_script, is_supported_witness in *.
  destruct (parse_witness_program s) as [[v prog]|] eqn:Hp; [| contradiction].
  destruct v as [| v'].
  - simpl in Hs. rewrite Hs. reflexivity.
  - destruct v' as [| v''].
    + simpl in Hs. rewrite Hs. reflexivity.
    + reflexivity.
Qed.

(** Statement: a witness program is never reclassified as mint or converge.
    Assumptions: [s] parses as a witness program.
    Scope: consensus kernel only. *)
Theorem witness_never_falls_to_mint_or_converge :
  forall s : Script,
    parse_witness_program s <> None ->
    classify_script s <> T_MINT /\ classify_script s <> T_CONVERGE.
Proof.
  intros s Hw. unfold classify_script.
  destruct (parse_witness_program s) as [[v prog]|] eqn:Hp; [| contradiction].
  destruct v as [| v'].
  - destruct (Nat.eqb (length prog) 20%nat); split; discriminate.
  - destruct v' as [| v''].
    + destruct (is_native_res_program prog); split; discriminate.
    + split; discriminate.
Qed.

(** Statement: the classifier never returns reserved Bitcoin-legacy types.
    Assumptions: none.
    Scope: consensus kernel only. *)
Theorem classify_never_legacy_p2pkh_p2sh :
  forall s : Script,
    classify_script s <> T_P2PKH /\ classify_script s <> T_P2SH.
Proof.
  intros s. unfold classify_script.
  destruct (parse_witness_program s) as [[v prog]|].
  - destruct v as [| v'].
    + destruct (Nat.eqb (length prog) 20%nat); split; discriminate.
    + destruct v' as [| v''].
      * destruct (is_native_res_program prog); split; discriminate.
      * split; discriminate.
  - destruct (is_mint_script s).
    + split; discriminate.
    + destruct (is_converge_script s).
      * split; discriminate.
      * destruct (is_null_data s); split; discriminate.
Qed.

(** * Payload independence

    "Authority-free" used to be recorded as [classify_script s =
    classify_script s], which is true of every function and says
    nothing. What the property means operationally is that no
    particular payload is privileged: swap the 20-byte hash inside a
    mint script for any other 20 bytes and the class does not move. No
    key, and no allowlisted address, can change how a script is
    classified. That is stated and proved below, and again over the
    asset class in ScriptCorrectness.v. *)

Lemma split_at_app :
  forall p q : list byte, split_at (length p) (p ++ q) = Some (p, q).
Proof.
  induction p as [| x p IH]; intros q; simpl; [reflexivity |].
  rewrite IH. reflexivity.
Qed.

(** A mint script carrying an arbitrary 20-byte payload. *)
Definition mint_script_of (p : list byte) : Script :=
  RIGOR_PUSH_0 :: RIGOR_PUSHBYTES_20 :: p ++ [RIGOR_MARKER_RES_MINT].

Lemma mint_script_of_not_witness :
  forall p : list byte,
    length p = 20%nat -> parse_witness_program (mint_script_of p) = None.
Proof.
  intros p Hp. unfold parse_witness_program, mint_script_of.
  simpl. rewrite app_length, Hp. simpl.
  unfold witness_version_of, RIGOR_PUSH_0, RIGOR_PUSHBYTES_20. simpl.
  reflexivity.
Qed.

Theorem mint_script_of_is_mint :
  forall p : list byte,
    length p = 20%nat -> is_mint_script (mint_script_of p) = true.
Proof.
  intros p Hp.
  assert (Hsplit : split_at 20%nat (p ++ [RIGOR_MARKER_RES_MINT])
                   = Some (p, [RIGOR_MARKER_RES_MINT]))
    by (rewrite <- Hp; apply split_at_app).
  unfold is_mint_script, mint_script_of.
  cbn -[split_at].
  rewrite Hsplit.
  reflexivity.
Qed.

Theorem mint_shape_classifies :
  forall p : list byte,
    length p = 20%nat -> classify_script (mint_script_of p) = T_MINT.
Proof.
  intros p Hp. unfold classify_script.
  rewrite (mint_script_of_not_witness p Hp).
  rewrite (mint_script_of_is_mint p Hp). reflexivity.
Qed.

(** The hash a mint script commits to does not affect its class. *)
Theorem classify_payload_independent :
  forall p q : list byte,
    length p = 20%nat ->
    length q = 20%nat ->
    classify_script (mint_script_of p) = classify_script (mint_script_of q).
Proof.
  intros p q Hp Hq.
  rewrite (mint_shape_classifies p Hp), (mint_shape_classifies q Hq).
  reflexivity.
Qed.

(** Concrete unsupported-witness example: OP_0 + 21-byte program. *)
Definition unsupported_v0_21 : Script :=
  RIGOR_PUSH_0 :: 21%nat :: repeat_byte 21 7%nat.

Theorem unsupported_v0_21_is_nonstandard :
  classify_script unsupported_v0_21 = T_NONSTANDARD.
Proof. vm_compute. reflexivity. Qed.

Theorem unsupported_v0_21_not_mint :
  classify_script unsupported_v0_21 <> T_MINT.
Proof. vm_compute. discriminate. Qed.

(** * Every reachable class is reachable

    [classify_total] bounds the range from above. Without the other
    direction it would also be satisfied by a classifier that answers
    [T_NONSTANDARD] for every script, so each class gets a witness. *)

Definition witness_v0_script : Script :=
  RIGOR_PUSH_0 :: 20%nat :: repeat_byte 20 7%nat.

Definition witness_v1_script : Script :=
  RIGOR_PUSH_1 :: 32%nat :: repeat_byte 32 7%nat.

Definition mint_witness_script : Script :=
  mint_script_of (repeat_byte 20 7%nat).

Definition converge_witness_script : Script :=
  RIGOR_PUSHBYTES_20 :: repeat_byte 20 7%nat ++ [RIGOR_MARKER_CONVERGE].

Definition null_data_witness_script : Script :=
  RIGOR_MARKER_RETURN :: [RIGOR_PUSH_1].

Theorem classify_range_exact :
  (forall s : Script, In (classify_script s) reachable_classes) /\
  (forall t : ScriptType,
      In t reachable_classes -> exists s : Script, classify_script s = t).
Proof.
  split; [exact classify_total |].
  intros t Ht. unfold reachable_classes in Ht. simpl in Ht.
  destruct Ht as [<- | [<- | [<- | [<- | [<- | [<- | []]]]]]].
  - exists witness_v0_script. vm_compute. reflexivity.
  - exists witness_v1_script. vm_compute. reflexivity.
  - exists mint_witness_script. vm_compute. reflexivity.
  - exists converge_witness_script. vm_compute. reflexivity.
  - exists null_data_witness_script. vm_compute. reflexivity.
  - exists unsupported_v0_21. vm_compute. reflexivity.
Qed.
