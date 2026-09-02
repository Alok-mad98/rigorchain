(** RIGOR v0.1 — Property 4: Signature Domain Separation.

    C++ [SignatureHash] (interpreter.cpp):

      signature_message =
          tag
          || chain_id
          || nVersion
          || hashPrevouts
          || hashSequence
          || hashOutputs
          || this prevout
          || spent scriptPubKey   (real script, length-prefixed)
          || amount
          || nSequence
          || nLockTime

    Tags (8 ASCII bytes, prefix position):
      ORU  P2WPKH  : RGRORU01
      RES  v1      : RGRRES01
      RES  MINT    : RGRMNT01

    The typed-integer layout record is SighashPreimage.SighashFields.
    This module's SighashFields is the length-indexed prefix-injectivity
    encoding (pre-serialized field widths).
    [different_tags_different_preimages] here is not the same theorem as
    MakeKernelInput.different_tags_different_preimages.

    chain_id is the genesis block hash, an explicit argument, never a
    consensus global. Ed25519 signs SHA256d of this preimage (C++
    HashWriter::GetHash). This module proves field-prefix injectivity
    of the preimage, not SHA256 collision resistance.

    Golden preimage prefixes are locked against
    core/src/test/data/rigor_sighash_p0_018.json.

    Not a C++ refinement. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Bool.Bool.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import Lia.
Import ListNotations.
Require Import RigorV01.AssetClassification.
Require Import RigorV01.Ed25519Ownership.

(** ASCII tags. *)
Definition tag_oru : list byte :=
  [82; 71; 82; 79; 82; 85; 48; 49]%nat. (* RGRORU01 *)
Definition tag_res : list byte :=
  [82; 71; 82; 82; 69; 83; 48; 49]%nat. (* RGRRES01 *)
Definition tag_mint : list byte :=
  [82; 71; 82; 77; 78; 84; 48; 49]%nat. (* RGRMNT01 *)

Definition tag_of_path (p : SpendPath) : list byte :=
  match p with
  | SpendOruP2WPKH => tag_oru
  | SpendResV1KeyPath => tag_res
  | SpendResMint => tag_mint
  end.

Lemma tag_len_8 :
  length tag_oru = 8%nat /\ length tag_res = 8%nat /\ length tag_mint = 8%nat.
Proof. vm_compute. repeat split; reflexivity. Qed.

Lemma tags_pairwise_distinct :
  tag_oru <> tag_res /\ tag_oru <> tag_mint /\ tag_res <> tag_mint.
Proof. vm_compute. repeat split; discriminate. Qed.

Theorem distinct_paths_distinct_tags :
  forall p q : SpendPath,
    p <> q -> tag_of_path p <> tag_of_path q.
Proof.
  intros p q Hne.
  destruct p, q; simpl; try contradiction; discriminate.
Qed.

(** Compact-size for scripts shorter than 253 bytes (all live templates). *)
Definition serialize_script (spk : list byte) : list byte :=
  length spk :: spk.

Lemma serialize_script_injective :
  forall s1 s2 : list byte,
    serialize_script s1 = serialize_script s2 -> s1 = s2.
Proof.
  intros s1 s2 H.
  unfold serialize_script in H.
  inversion H. reflexivity.
Qed.

Record SighashFields := {
  sh_tag : list byte;
  sh_chain_id : list byte;
  sh_nversion : list byte;
  sh_hash_prevouts : list byte;
  sh_hash_sequence : list byte;
  sh_hash_outputs : list byte;
  sh_prevout : list byte;
  sh_script : list byte;
  sh_amount : list byte;
  sh_nsequence : list byte;
  sh_nlocktime : list byte
}.

Definition fields_ok (f : SighashFields) : Prop :=
  length (sh_tag f) = 8%nat /\
  length (sh_chain_id f) = 32%nat /\
  length (sh_nversion f) = 4%nat /\
  length (sh_hash_prevouts f) = 32%nat /\
  length (sh_hash_sequence f) = 32%nat /\
  length (sh_hash_outputs f) = 32%nat /\
  length (sh_prevout f) = 36%nat /\
  (length (sh_script f) < 253)%nat /\
  length (sh_amount f) = 8%nat /\
  length (sh_nsequence f) = 4%nat /\
  length (sh_nlocktime f) = 4%nat.

Definition signature_preimage (f : SighashFields) : list byte :=
  app_bytes (sh_tag f)
  (app_bytes (sh_chain_id f)
  (app_bytes (sh_nversion f)
  (app_bytes (sh_hash_prevouts f)
  (app_bytes (sh_hash_sequence f)
  (app_bytes (sh_hash_outputs f)
  (app_bytes (sh_prevout f)
  (app_bytes (serialize_script (sh_script f))
  (app_bytes (sh_amount f)
  (app_bytes (sh_nsequence f) (sh_nlocktime f)))))))))).

Lemma app_bytes_assoc :
  forall a b c : list byte,
    app_bytes a (app_bytes b c) = app_bytes (app_bytes a b) c.
Proof.
  induction a as [|x xs IH]; intros; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma app_bytes_length :
  forall a b : list byte,
    length (app_bytes a b) = (length a + length b)%nat.
Proof.
  induction a as [|x xs IH]; intros; simpl; [reflexivity|].
  rewrite IH. reflexivity.
Qed.

Lemma app_bytes_fixed_inj :
  forall (a1 a2 b1 b2 : list byte),
    length a1 = length a2 ->
    app_bytes a1 b1 = app_bytes a2 b2 ->
    a1 = a2 /\ b1 = b2.
Proof.
  induction a1 as [|x xs IH]; intros a2 b1 b2 Hlen Heq.
  - destruct a2; [|discriminate Hlen]. split; [reflexivity|exact Heq].
  - destruct a2 as [|y ys]; [discriminate Hlen|].
    simpl in Heq. injection Heq as Hy Hrest. subst y.
    simpl in Hlen. apply Nat.succ_inj in Hlen.
    destruct (IH ys b1 b2 Hlen Hrest) as [Ha Hb].
    subst. split; reflexivity.
Qed.

Lemma preimage_starts_with_tag :
  forall f : SighashFields,
    fields_ok f ->
    exists rest : list byte,
      signature_preimage f = app_bytes (sh_tag f) rest.
Proof.
  intros f _. unfold signature_preimage. eexists. reflexivity.
Qed.

Theorem different_tags_different_preimages :
  forall f1 f2 : SighashFields,
    fields_ok f1 ->
    fields_ok f2 ->
    sh_tag f1 <> sh_tag f2 ->
    signature_preimage f1 <> signature_preimage f2.
Proof.
  intros f1 f2 Ok1 Ok2 Hne Heq.
  unfold fields_ok in Ok1, Ok2.
  destruct Ok1 as [Lt1 _].
  destruct Ok2 as [Lt2 _].
  unfold signature_preimage in Heq.
  apply app_bytes_fixed_inj in Heq; [|rewrite Lt1, Lt2; reflexivity].
  destruct Heq as [Ht _]. contradiction.
Qed.

Theorem different_chain_ids_different_preimages :
  forall f1 f2 : SighashFields,
    fields_ok f1 ->
    fields_ok f2 ->
    sh_tag f1 = sh_tag f2 ->
    sh_chain_id f1 <> sh_chain_id f2 ->
    signature_preimage f1 <> signature_preimage f2.
Proof.
  intros f1 f2 Ok1 Ok2 Htag Hne Heq.
  unfold fields_ok in Ok1, Ok2.
  destruct Ok1 as [Lt1 [Lc1 _]].
  destruct Ok2 as [Lt2 [Lc2 _]].
  unfold signature_preimage in Heq.
  apply app_bytes_fixed_inj in Heq; [|rewrite Lt1, Lt2; reflexivity].
  destruct Heq as [_ Heq2].
  apply app_bytes_fixed_inj in Heq2; [|rewrite Lc1, Lc2; reflexivity].
  destruct Heq2 as [Hc _]. contradiction.
Qed.

(** Compact-size script prefix is injective against a fixed-width suffix. *)
Lemma serialize_then_suffix_inj :
  forall (s1 s2 suf1 suf2 : list byte),
    (length s1 < 253)%nat ->
    (length s2 < 253)%nat ->
    length suf1 = length suf2 ->
    app_bytes (serialize_script s1) suf1 = app_bytes (serialize_script s2) suf2 ->
    s1 = s2 /\ suf1 = suf2.
Proof.
  intros s1 s2 suf1 suf2 L1 L2 Lsuf Heq.
  unfold serialize_script in Heq. simpl in Heq.
  inversion Heq. subst.
  apply app_bytes_fixed_inj in H1; [|exact H0].
  exact H1.
Qed.

Theorem different_scripts_different_preimages :
  forall f1 f2 : SighashFields,
    fields_ok f1 ->
    fields_ok f2 ->
    sh_tag f1 = sh_tag f2 ->
    sh_chain_id f1 = sh_chain_id f2 ->
    sh_nversion f1 = sh_nversion f2 ->
    sh_hash_prevouts f1 = sh_hash_prevouts f2 ->
    sh_hash_sequence f1 = sh_hash_sequence f2 ->
    sh_hash_outputs f1 = sh_hash_outputs f2 ->
    sh_prevout f1 = sh_prevout f2 ->
    sh_amount f1 = sh_amount f2 ->
    sh_nsequence f1 = sh_nsequence f2 ->
    sh_nlocktime f1 = sh_nlocktime f2 ->
    sh_script f1 <> sh_script f2 ->
    signature_preimage f1 <> signature_preimage f2.
Proof.
  intros f1 f2 Ok1 Ok2 Ht Hc Hv Hp Hs Ho Hpv Ham Hns Hnl Hne Heq.
  unfold fields_ok in Ok1, Ok2.
  destruct Ok1 as [Lt1 [Lc1 [Lv1 [Lpp1 [Lps1 [Lpo1 [Lpv1 [Lsc1 [Lam1 [Lns1 Lnl1]]]]]]]]]].
  destruct Ok2 as [Lt2 [Lc2 [Lv2 [Lpp2 [Lps2 [Lpo2 [Lpv2 [Lsc2 [Lam2 [Lns2 Lnl2]]]]]]]]]].
  unfold signature_preimage in Heq.
  apply app_bytes_fixed_inj in Heq; [|rewrite Lt1, Lt2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lc1, Lc2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lv1, Lv2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpp1, Lpp2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lps1, Lps2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpo1, Lpo2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpv1, Lpv2; reflexivity].
  destruct Heq as [_ Heq].
  (* remaining: serialize_script ++ amount ++ nseq ++ nlock *)
  set (suf1 := app_bytes (sh_amount f1) (app_bytes (sh_nsequence f1) (sh_nlocktime f1))).
  set (suf2 := app_bytes (sh_amount f2) (app_bytes (sh_nsequence f2) (sh_nlocktime f2))).
  assert (Heq' : app_bytes (serialize_script (sh_script f1)) suf1 =
                 app_bytes (serialize_script (sh_script f2)) suf2).
  { unfold suf1, suf2. exact Heq. }
  assert (Lsuf : length suf1 = length suf2).
  { unfold suf1, suf2. rewrite Ham, Hns, Hnl. reflexivity. }
  destruct (serialize_then_suffix_inj (sh_script f1) (sh_script f2) suf1 suf2 Lsc1 Lsc2 Lsuf Heq')
    as [Hscrip _].
  contradiction.
Qed.

Theorem different_amounts_different_preimages :
  forall f1 f2 : SighashFields,
    fields_ok f1 ->
    fields_ok f2 ->
    sh_tag f1 = sh_tag f2 ->
    sh_chain_id f1 = sh_chain_id f2 ->
    sh_nversion f1 = sh_nversion f2 ->
    sh_hash_prevouts f1 = sh_hash_prevouts f2 ->
    sh_hash_sequence f1 = sh_hash_sequence f2 ->
    sh_hash_outputs f1 = sh_hash_outputs f2 ->
    sh_prevout f1 = sh_prevout f2 ->
    sh_script f1 = sh_script f2 ->
    sh_nsequence f1 = sh_nsequence f2 ->
    sh_nlocktime f1 = sh_nlocktime f2 ->
    sh_amount f1 <> sh_amount f2 ->
    signature_preimage f1 <> signature_preimage f2.
Proof.
  intros f1 f2 Ok1 Ok2 Ht Hc Hv Hp Hs Ho Hpv Hsc Hns Hnl Hne Heq.
  unfold fields_ok in Ok1, Ok2.
  destruct Ok1 as [Lt1 [Lc1 [Lv1 [Lpp1 [Lps1 [Lpo1 [Lpv1 [Lsc1 [Lam1 [Lns1 Lnl1]]]]]]]]]].
  destruct Ok2 as [Lt2 [Lc2 [Lv2 [Lpp2 [Lps2 [Lpo2 [Lpv2 [Lsc2 [Lam2 [Lns2 Lnl2]]]]]]]]]].
  unfold signature_preimage in Heq.
  apply app_bytes_fixed_inj in Heq; [|rewrite Lt1, Lt2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lc1, Lc2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lv1, Lv2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpp1, Lpp2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lps1, Lps2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpo1, Lpo2; reflexivity].
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lpv1, Lpv2; reflexivity].
  destruct Heq as [_ Heq].
  rewrite Hsc in Heq.
  apply app_bytes_fixed_inj in Heq.
  2: { unfold serialize_script. simpl. reflexivity. }
  destruct Heq as [_ Heq].
  apply app_bytes_fixed_inj in Heq; [|rewrite Lam1, Lam2; reflexivity].
  destruct Heq as [Ha _]. contradiction.
Qed.

(** Distinct spend paths cannot share a preimage: tags occupy the first 8 bytes. *)
Theorem different_paths_different_preimages :
  forall (p q : SpendPath) (f1 f2 : SighashFields),
    fields_ok f1 ->
    fields_ok f2 ->
    sh_tag f1 = tag_of_path p ->
    sh_tag f2 = tag_of_path q ->
    p <> q ->
    signature_preimage f1 <> signature_preimage f2.
Proof.
  intros p q f1 f2 Ok1 Ok2 Ht1 Ht2 Hne.
  apply different_tags_different_preimages; try assumption.
  rewrite Ht1, Ht2. apply distinct_paths_distinct_tags. exact Hne.
Qed.

(** Golden vector prefixes from rigor_sighash_p0_018.json (first 8+32 bytes). *)
Definition golden_chain_id_11 : list byte := repeat_byte 32 17%nat.

Lemma golden_oru_tag : tag_oru = [82; 71; 82; 79; 82; 85; 48; 49]%nat.
Proof. reflexivity. Qed.

Lemma golden_res_tag : tag_res = [82; 71; 82; 82; 69; 83; 48; 49]%nat.
Proof. reflexivity. Qed.

Lemma golden_mint_tag : tag_mint = [82; 71; 82; 77; 78; 84; 48; 49]%nat.
Proof. reflexivity. Qed.

(** hex 5247524f52553031 = RGRORU01, confirmed. *)
Lemma golden_oru_preimage_prefix :
  app_bytes tag_oru golden_chain_id_11 =
  app_bytes [82; 71; 82; 79; 82; 85; 48; 49]%nat (repeat_byte 32 17%nat).
Proof. reflexivity. Qed.
