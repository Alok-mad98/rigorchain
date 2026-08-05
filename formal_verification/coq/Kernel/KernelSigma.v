(** PHASE-4A kernel proofs: Sigma-PoW-01 byte layout (REAL, no admit).

    The sigma digest is always 32 bytes, a direct consequence of the executable
    SHA256d, and the preimage is exactly 100 bytes laid out as
    txid ‖ C ‖ B_{h-1} ‖ LE32(height) by [sigma_preimage_bytes].

    SECURITY-001-A8: the seed B_{h-1} occupies its own 32-byte slot, so a draw resolved
    under one parent has a different σ preimage from the same draw under any other
    parent — see [sigma_preimage_differs_on_seed]. Whether the resulting *digests*
    differ is a SHA256d collision question, which this layer does not assume; the
    executable spec exercises it on concrete seeds instead. *)
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.
Require Import Types.
From Rigor Require Import RigorBytes.
Include RigorBytes.
Require Import RigorHash.
Require Import SigmaPoW01.

Theorem sigma_bytes_length :
  forall txid C B h, length (sigma_bytes txid C B h) = 32%nat.
Proof.
  intros txid C B h. unfold sigma_bytes. apply hash256_bytes_length.
Qed.

Lemma app_bytes_length :
  forall xs ys, length (app_bytes xs ys) = (length xs + length ys)%nat.
Proof.
  induction xs as [| x xs IH]; intros ys; simpl; [ reflexivity | rewrite IH; reflexivity ].
Qed.

(** Concatenation of byte strings of known equal length splits back uniquely. This is
    what makes the fixed-offset layout unambiguous. *)
Lemma app_bytes_split :
  forall xs ys zs ws,
    length xs = length ys ->
    app_bytes xs zs = app_bytes ys ws ->
    xs = ys /\ zs = ws.
Proof.
  induction xs as [| x xs IH]; intros ys zs ws Hlen Heq.
  - destruct ys as [| y ys]; simpl in Hlen; [ | discriminate ].
    simpl in Heq. split; [ reflexivity | exact Heq ].
  - destruct ys as [| y ys]; simpl in Hlen; [ discriminate | ].
    injection Hlen as Hlen. simpl in Heq. injection Heq as Hx Hrest.
    destruct (IH ys zs ws Hlen Hrest) as [Hxs Hzw].
    split; [ rewrite Hx, Hxs; reflexivity | exact Hzw ].
Qed.

(** RFC-009 §4: len(input) MUST be exactly 100, otherwise bad-hash-length. *)
Theorem sigma_preimage_length :
  forall txid C B h,
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    length (sigma_preimage_bytes txid C B h) = 100%nat.
Proof.
  intros txid C B h Ht Hc Hb.
  unfold sigma_preimage_bytes, app_bytes32.
  rewrite !app_bytes_length, Ht, Hc, Hb. reflexivity.
Qed.

(** The preimage determines every field: the seed cannot be confused with the txid, the
    root or the height, so no other input can compensate for a change of parent. *)
Theorem sigma_preimage_injective :
  forall txid txid' C C' B B' h h',
    length txid = 32%nat -> length txid' = 32%nat ->
    length C = 32%nat -> length C' = 32%nat ->
    length B = 32%nat -> length B' = 32%nat ->
    sigma_preimage_bytes txid C B h = sigma_preimage_bytes txid' C' B' h' ->
    txid = txid' /\ C = C' /\ B = B' /\ le32_bytes h = le32_bytes h'.
Proof.
  intros txid txid' C C' B B' h h' Ht Ht' Hc Hc' Hb Hb' Heq.
  unfold sigma_preimage_bytes, app_bytes32 in Heq.
  destruct (app_bytes_split txid txid' _ _ (eq_trans Ht (eq_sym Ht')) Heq) as [Htxid Hrest].
  destruct (app_bytes_split C C' _ _ (eq_trans Hc (eq_sym Hc')) Hrest) as [Hroot Hrest'].
  destruct (app_bytes_split B B' _ _ (eq_trans Hb (eq_sym Hb')) Hrest') as [Hseed Hheight].
  repeat split; assumption.
Qed.

(** Contrapositive on the seed slot: a different parent is a different σ input. *)
Corollary sigma_preimage_differs_on_seed :
  forall txid C B B' h,
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    length B' = 32%nat ->
    B <> B' ->
    sigma_preimage_bytes txid C B h <> sigma_preimage_bytes txid C B' h.
Proof.
  intros txid C B B' h Ht Hc Hb Hb' Hne Heq.
  destruct (sigma_preimage_injective txid txid C C B B' h h Ht Ht Hc Hc Hb Hb' Heq)
    as [_ [_ [Hseed _]]].
  exact (Hne Hseed).
Qed.

(** And on the payout height: h is the only thing the paying block contributes. *)
Corollary sigma_preimage_differs_on_height :
  forall txid C B h h',
    length txid = 32%nat ->
    length C = 32%nat ->
    length B = 32%nat ->
    le32_bytes h <> le32_bytes h' ->
    sigma_preimage_bytes txid C B h <> sigma_preimage_bytes txid C B h'.
Proof.
  intros txid C B h h' Ht Hc Hb Hne Heq.
  destruct (sigma_preimage_injective txid txid C C B B h h' Ht Ht Hc Hc Hb Hb Heq)
    as [_ [_ [_ Hheight]]].
  exact (Hne Hheight).
Qed.
