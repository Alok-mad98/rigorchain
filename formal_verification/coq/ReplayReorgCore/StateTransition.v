(** ReplayReorgCore/StateTransition.v — structural ApplyBlock / ApplyTx. *)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
From Coq Require Import Lia.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.ReplayState.
Require Import Rigor.ReplayReorgCore.ScriptShape.
Require Import Rigor.ReplayReorgCore.CvgCovenant.
Require ScriptMintWitness.

(** Decidable OutPoint equality (needed for list UTXO updates). *)
Definition outpoint_eqb (a b : OutPoint) : bool :=
  match a, b with
  | MkOutPoint ha ia, MkOutPoint hb ib =>
      andb (Nat.eqb ia ib)
           (match hash32_eq_dec ha hb with left _ => true | right _ => false end)
  end.

Fixpoint utxo_lookup (s : UTXOSet) (op : OutPoint) : option TxOut :=
  match s with
  | nil => None
  | (op', o) :: t =>
      if outpoint_eqb op op' then Some o else utxo_lookup t op
  end.

Definition utxo_remove (s : UTXOSet) (op : OutPoint) : UTXOSet :=
  filter (fun p => negb (outpoint_eqb (fst p) op)) s.

Definition utxo_insert (s : UTXOSet) (op : OutPoint) (o : TxOut) : UTXOSet :=
  utxo_remove s op ++ [(op, o)].

(** Insert created outputs of [tx] starting at vout [base] under [tx_id tx]. *)
Definition fold_create_outputs (creates : list TxOut) (u : UTXOSet)
                                (hid : Hash32) (base : nat) : UTXOSet :=
  fst (fold_left
         (fun (acc : UTXOSet * nat) (o : TxOut) =>
            let (u', idx) := acc in
            (utxo_insert u' (MkOutPoint hid idx) o, S idx))
         creates (u, base)).

(** All spends must exist; creates are [(tx_id tx, 0) .. (tx_id tx, n-1)].

    [tx_index] is the position of [tx] in the block (kept for call-site
    compatibility). It is *not* used as an outpoint identity: two
    transactions in different blocks both applied at index 0 must not
    overwrite each other's outputs. *)
Definition apply_tx_utxo (s : UTXOSet) (tx : TxAbs) (tx_index : nat)
  : option UTXOSet :=
  let spends_ok :=
    forallb (fun op => match utxo_lookup s op with Some _ => true | None => false end)
            (tx_spends tx) in
  if spends_ok then
    let s1 := fold_left (fun u op => utxo_remove u op) (tx_spends tx) s in
    Some (fold_create_outputs (tx_creates tx) s1 (tx_id tx) 0)
  else
    None.

(** Header connects to current tip (height + prev hash). *)
Definition valid_header_connect (cm : ChainMeta) (hdr : BlockHeaderAbs) : bool :=
  andb (Nat.eqb (hdr_height hdr) (S (cm_height cm)))
       (match hash32_eq_dec (hdr_prev_hash hdr) (cm_tip_hash cm) with
        | left _ => true
        | right _ => false
        end).

(** Chainwork strictly increases along the active chain (INV-01 structural). *)
Definition valid_chainwork_extend (cm : ChainMeta) (hdr : BlockHeaderAbs) : bool :=
  Z.gtb (hdr_chainwork hdr) (cm_chainwork cm).

Require Import Rigor.ReplayReorgCore.ConnectBlockMint.
From Rigor Require Import ResSchedule.

(** SECURITY-001-F G-2: the third conjunct bounds ORU subsidy. Coq P1 adds the
    fourth: winner-must-mint / H-1 ([valid_res_mint_obligation]). Without it a
    winner block could omit the mint while [W(h) > 0], which production rejects
    as [bad-rigor-res-winner-mismatch].

    Fifth conjunct: cap overshoot is an explicit reject ([valid_res_cap],
    [bad-rigor-res-max-supply]). Combined with obligation this is fail-closed
    when [W(h) > 0]: omitting the mint is winner-mismatch; including a mint
    that would breach the cap is max-supply. *)
Definition valid_block_connect (s : ReplayState) (b : BlockAbs) : bool :=
  let hdr := blk_header b in
  andb (andb (andb (andb (valid_header_connect (rs_chain s) hdr)
                         (valid_chainwork_extend (rs_chain s) hdr))
                   (cb_oru_subsidy_ok (hdr_height hdr) (blk_mint b)))
             (valid_res_mint_obligation b))
       (valid_res_cap s b).

Lemma valid_block_connect_inv :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    valid_header_connect (rs_chain s) (blk_header b) = true /\
    valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
    cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
    valid_res_mint_obligation b = true /\
    valid_res_cap s b = true.
Proof.
  intros s b H. unfold valid_block_connect in H.
  apply andb_prop in H as [Hrest Hcap].
  apply andb_prop in Hrest as [Hrest Hob].
  apply andb_prop in Hrest as [Hrest Hsub].
  apply andb_prop in Hrest as [Hh Hw].
  repeat split; assumption.
Qed.

(** The issuance bound a connected block obeys, stated on the delta actually applied. *)
Lemma valid_block_connect_bounds_oru_mint :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    (cb_oru_mint_delta s b <= cb_ORU_INITIAL_SUBSIDY)%Z.
Proof.
  intros s b H. apply valid_block_connect_inv in H.
  destruct H as [_ [_ [Hsub _]]].
  unfold cb_oru_subsidy_ok in Hsub. apply Z.leb_le in Hsub.
  unfold cb_oru_mint_delta, cb_oru_subsidy.
  pose proof (cb_expected_oru_subsidy_le_initial (hdr_height (blk_header b))) as Hsched.
  unfold cb_ORU_INITIAL_SUBSIDY in *. lia.
Qed.

Lemma valid_block_connect_res_mint_obligation :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    valid_res_mint_obligation b = true.
Proof.
  intros s b H. apply valid_block_connect_inv in H.
  destruct H as [_ [_ [_ [Hob _]]]]. exact Hob.
Qed.

Lemma valid_block_connect_res_cap :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true ->
    valid_res_cap s b = true.
Proof.
  intros s b H. apply valid_block_connect_inv in H.
  destruct H as [_ [_ [_ [_ Hcap]]]]. exact Hcap.
Qed.

Lemma valid_block_connect_intro :
  forall (s : ReplayState) (b : BlockAbs),
    valid_header_connect (rs_chain s) (blk_header b) = true ->
    valid_chainwork_extend (rs_chain s) (blk_header b) = true ->
    cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true ->
    valid_res_mint_obligation b = true ->
    valid_res_cap s b = true ->
    valid_block_connect s b = true.
Proof.
  intros s b Hh Hw Hs Hob Hcap.
  unfold valid_block_connect.
  apply andb_true_iff; split; [|exact Hcap].
  apply andb_true_iff; split; [|exact Hob].
  apply andb_true_iff; split; [|exact Hs].
  apply andb_true_iff; split; [exact Hh | exact Hw].
Qed.

Theorem valid_block_connect_iff :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = true <->
      (valid_header_connect (rs_chain s) (blk_header b) = true /\
       valid_chainwork_extend (rs_chain s) (blk_header b) = true /\
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = true /\
       valid_res_mint_obligation b = true /\
       valid_res_cap s b = true).
Proof.
  intros s b. split.
  - apply valid_block_connect_inv.
  - intros [Hh [Hw [Hs [Hob Hcap]]]].
    apply valid_block_connect_intro; assumption.
Qed.

Theorem valid_block_connect_false_iff :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = false <->
      (valid_header_connect (rs_chain s) (blk_header b) = false \/
       valid_chainwork_extend (rs_chain s) (blk_header b) = false \/
       cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b) = false \/
       valid_res_mint_obligation b = false \/
       valid_res_cap s b = false).
Proof.
  intros s b.
  split.
  - intro Hf.
    destruct (valid_header_connect (rs_chain s) (blk_header b)) eqn:Hh;
      [ | now left ].
    destruct (valid_chainwork_extend (rs_chain s) (blk_header b)) eqn:Hw;
      [ | now (right; left) ].
    destruct (cb_oru_subsidy_ok (hdr_height (blk_header b)) (blk_mint b)) eqn:Hs;
      [ | now (right; right; left) ].
    destruct (valid_res_mint_obligation b) eqn:Hob;
      [ | now (right; right; right; left) ].
    destruct (valid_res_cap s b) eqn:Hcap;
      [ | now (right; right; right; right) ].
    exfalso.
    apply Bool.eq_true_false_abs with (b := valid_block_connect s b);
      [ apply valid_block_connect_intro; assumption | exact Hf ].
  - intros [H | [H | [H | [H | H]]]];
      apply Bool.not_true_iff_false; intro Hc;
      apply valid_block_connect_iff in Hc;
      destruct Hc as [Hh [Hw [Hs [Hob Hcap]]]];
      congruence.
Qed.

(** * Dual-pool gate (TASK-FV-MODEL-DUALPOOL-APPLY-TX-001)

    Mirrors [AccumulateTxInputPools] + [CheckTxInputs_RES_DualPool] and the
    [CheckOruFundedResMintShape] exception, without a script interpreter.
    Pooling class is [output_asset] = [asset_kind_of_script out_script]
    (C++ [AssetTypeFromScriptPubKey] shape). [valid_dual_pool_tx] is a
    conjunct of [apply_tx]: ordinary RES cannot inflate, and the only
    exceptions are ORU-funded mint-shape and the covenant-funded
    [cvg_mint_shape] (exactly one RES out of amount [W(h)]).
    Not a C++ refinement. *)

Definition asset_kind_of_script (spk : list nat) : AssetKind :=
  match ScriptMintWitness.asset_from_script spk with
  | ScriptMintWitness.SmwORU => AssetORU
  | ScriptMintWitness.SmwRES => AssetRES
  | ScriptMintWitness.SmwNone => AssetNone
  | ScriptMintWitness.SmwInvalid => AssetInvalid
  end.

Definition output_asset (o : TxOut) : AssetKind :=
  asset_kind_of_script (out_script o).

Definition spk_oru : list nat := ScriptMintWitness.canonical_p2wpkh.
Definition spk_res : list nat := ScriptMintWitness.canonical_native_res.
Definition spk_mint : list nat := ScriptMintWitness.canonical_res_mint_script.

Definition mk_oru (v : Z) : TxOut := {| out_value := v; out_script := spk_oru |}.
Definition mk_res (v : Z) : TxOut := {| out_value := v; out_script := spk_res |}.
Definition mk_mint (v : Z) : TxOut := {| out_value := v; out_script := spk_mint |}.

Definition txout_asset_value (k : AssetKind) (o : TxOut) : Z :=
  if asset_eqb (output_asset o) k then out_value o else 0%Z.

Fixpoint sum_asset (k : AssetKind) (outs : list TxOut) : Z :=
  match outs with
  | nil => 0%Z
  | o :: rest => (txout_asset_value k o + sum_asset k rest)%Z
  end.

Definition count_asset (k : AssetKind) (outs : list TxOut) : nat :=
  length (filter (fun o => asset_eqb (output_asset o) k) outs).

(** Vacuous after the tag was removed: every [TxOut] pools as [f(script)]. *)
Definition apply_tx_derived_assets_obligation (tx : TxAbs) : bool :=
  forallb (fun o => asset_eqb (output_asset o) (asset_kind_of_script (out_script o)))
          (tx_creates tx).

Lemma output_asset_is_script :
  forall o : TxOut, output_asset o = asset_kind_of_script (out_script o).
Proof. intros o. reflexivity. Qed.

Lemma apply_tx_derived_assets_obligation_true :
  forall tx : TxAbs, apply_tx_derived_assets_obligation tx = true.
Proof.
  intros tx. unfold apply_tx_derived_assets_obligation.
  induction (tx_creates tx) as [|o rest IH]; [reflexivity|].
  simpl. unfold output_asset.
  destruct (asset_kind_of_script (out_script o)); simpl; exact IH.
Qed.

Lemma txout_asset_value_from_script :
  forall (k : AssetKind) (o : TxOut),
    txout_asset_value k o =
    (if asset_eqb (asset_kind_of_script (out_script o)) k then out_value o else 0%Z).
Proof.
  intros k o. unfold txout_asset_value, output_asset. reflexivity.
Qed.

Lemma output_asset_cvg :
  forall o : TxOut,
    is_cvg_covenant_output o = true ->
    output_asset o = AssetInvalid.
Proof.
  intros o H.
  unfold output_asset, asset_kind_of_script, ScriptMintWitness.asset_from_script.
  unfold is_cvg_covenant_output in H. rewrite H. reflexivity.
Qed.

Lemma txout_cvg_not_oru :
  forall o : TxOut,
    is_cvg_covenant_output o = true ->
    txout_asset_value AssetORU o = 0%Z.
Proof.
  intros o H. unfold txout_asset_value. rewrite (output_asset_cvg o H). reflexivity.
Qed.

Lemma txout_cvg_not_res :
  forall o : TxOut,
    is_cvg_covenant_output o = true ->
    txout_asset_value AssetRES o = 0%Z.
Proof.
  intros o H. unfold txout_asset_value. rewrite (output_asset_cvg o H). reflexivity.
Qed.

(** * W3 covenant spend (owner-hash coupling)

    [cvg_spend_ok] is the count gate. [valid_cvg_covenant_spend] additionally
    requires the unique CVG input's [cvg_owner_hash160] to equal the unique
    [OP_RES_MINT] output's pubkey hash. [apply_tx] uses [valid_cvg_spend_tx]. *)

Fixpoint natlist_eqb (a b : list nat) : bool :=
  match a, b with
  | nil, nil => true
  | x :: xs, y :: ys => andb (Nat.eqb x y) (natlist_eqb xs ys)
  | _, _ => false
  end.

Lemma natlist_eqb_refl : forall a, natlist_eqb a a = true.
Proof.
  induction a as [|x xs IH]; simpl; [reflexivity|].
  rewrite Nat.eqb_refl. exact IH.
Qed.

Lemma natlist_eqb_true :
  forall a b : list nat, natlist_eqb a b = true -> a = b.
Proof.
  induction a as [|x xs IH]; intros b H; destruct b as [|y ys]; simpl in H;
    try discriminate H.
  - reflexivity.
  - apply andb_true_iff in H as [Hx Hrest].
    apply Nat.eqb_eq in Hx. subst y. apply IH in Hrest. subst ys. reflexivity.
Qed.

Definition is_res_mint_output (o : TxOut) : bool :=
  ScriptMintWitness.is_res_mint_script (out_script o).

Definition unique_mint_owner_hash (outs : list TxOut) : option (list nat) :=
  match filter is_res_mint_output outs with
  | o :: nil => ScriptMintWitness.parse_res_mint_pubkey_hash (out_script o)
  | _ => None
  end.

Definition valid_cvg_covenant_spend (ins : list TxOut) (tx : TxAbs) : bool :=
  match count_cvg ins with
  | 0%nat => true
  | 1%nat =>
      andb (Nat.eqb (count_cvg (tx_creates tx)) 0%nat)
           (match cvg_unique_owner_hash ins,
                  unique_mint_owner_hash (tx_creates tx) with
            | Some hc, Some hm => natlist_eqb hc hm
            | _, _ => false
            end)
  | _ => false
  end.

Lemma valid_cvg_covenant_spend_no_cvg :
  forall (ins : list TxOut) (tx : TxAbs),
    count_cvg ins = 0%nat ->
    valid_cvg_covenant_spend ins tx = true.
Proof.
  intros ins tx H. unfold valid_cvg_covenant_spend. rewrite H. reflexivity.
Qed.

Lemma valid_cvg_covenant_spend_implies_cvg_spend_ok :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_covenant_spend ins tx = true ->
    cvg_spend_ok ins tx = true.
Proof.
  intros ins tx H.
  unfold valid_cvg_covenant_spend, cvg_spend_ok in *.
  destruct (count_cvg ins) as [|n]; [reflexivity|].
  destruct n; [|discriminate H].
  apply andb_true_iff in H as [Hc _]. exact Hc.
Qed.

Lemma valid_cvg_covenant_spend_no_recreate :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_covenant_spend ins tx = true ->
    count_cvg ins = 1%nat ->
    count_cvg (tx_creates tx) = 0%nat.
Proof.
  intros ins tx H Hc.
  unfold valid_cvg_covenant_spend in H. rewrite Hc in H.
  apply andb_true_iff in H as [Hz _]. apply Nat.eqb_eq in Hz. exact Hz.
Qed.

Theorem valid_cvg_covenant_spend_owner_eq :
  forall (ins : list TxOut) (tx : TxAbs) (hc hm : list nat),
    valid_cvg_covenant_spend ins tx = true ->
    count_cvg ins = 1%nat ->
    cvg_unique_owner_hash ins = Some hc ->
    unique_mint_owner_hash (tx_creates tx) = Some hm ->
    hc = hm.
Proof.
  intros ins tx hc hm H Hc Hcvg Hmint.
  unfold valid_cvg_covenant_spend in H. rewrite Hc in H.
  apply andb_true_iff in H as [_ Heq].
  rewrite Hcvg, Hmint in Heq.
  apply natlist_eqb_true in Heq. exact Heq.
Qed.

Theorem valid_cvg_covenant_spend_has_matching_hashes :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_covenant_spend ins tx = true ->
    count_cvg ins = 1%nat ->
    exists (hc hm : list nat),
      cvg_unique_owner_hash ins = Some hc /\
      unique_mint_owner_hash (tx_creates tx) = Some hm /\
      hc = hm.
Proof.
  intros ins tx H Hc.
  unfold valid_cvg_covenant_spend in H. rewrite Hc in H.
  apply andb_true_iff in H as [_ Heq].
  destruct (cvg_unique_owner_hash ins) as [hc|] eqn:E1; [|discriminate Heq].
  destruct (unique_mint_owner_hash (tx_creates tx)) as [hm|] eqn:E2;
    [|discriminate Heq].
  exists hc, hm. split; [reflexivity|]. split; [reflexivity|].
  apply natlist_eqb_true. exact Heq.
Qed.

Lemma valid_cvg_covenant_spend_canonical_owner :
  valid_cvg_covenant_spend [mk_cvg 1%Z]
    {| tx_id := hash32_placeholder;
       tx_spends := nil;
       tx_creates :=
         [{| out_value := 1%Z; out_script := spk_mint |}];
       tx_pcorb := None |} = true.
Proof. vm_compute. reflexivity. Qed.

(** * Ticket birth

    [valid_cvg_covenant_spend] governs *spending* a ticket. Creating one is
    additionally rejected by [cvg_create_gate] and by [is_allowed_script]
    (TASK-010 C-4: [OP_CVG_CLAIM] classifies as [SmwInvalid]). The predicates
    below remain as the design-study birth rule if the opcode is ever revived.

    A ticket is a receipt for ORU committed to a convergence draw, so birth requires ORU input
    value, forbids creating RES in the same transaction (birth is not a mint), admits at most one
    ticket, and enforces the face-value floor.

    What is *not* modelled: binding the ticket's owner hash to the A-5 payout declaration, and
    the equality between the ticket's face value and the draw weight C(t). This layer has no
    declaration and no draw, so both remain C++ obligations at ticket birth. Requiring ORU input
    value is strictly weaker than "the ticket is backed by exactly its face value in ORU": there
    is no ORU value-conservation rule in this model to derive the stronger statement from. *)
Definition valid_cvg_create (ins : list TxOut) (tx : TxAbs) : bool :=
  match count_cvg (tx_creates tx) with
  | 0%nat => true
  | 1%nat =>
      andb (Z.gtb (sum_asset AssetORU ins) 0)
      (andb (Nat.eqb (count_asset AssetRES (tx_creates tx)) 0%nat)
            (cvg_creates_value_ok (tx_creates tx)))
  | _ => false
  end.

Lemma valid_cvg_create_no_create :
  forall (ins : list TxOut) (tx : TxAbs),
    count_cvg (tx_creates tx) = 0%nat ->
    valid_cvg_create ins tx = true.
Proof.
  intros ins tx H. unfold valid_cvg_create. rewrite H. reflexivity.
Qed.

(** At most one ticket per transaction: a batch of tickets is not a batch of draws. *)
Lemma valid_cvg_create_rejects_many :
  forall (ins : list TxOut) (tx : TxAbs) (n : nat),
    count_cvg (tx_creates tx) = S (S n) ->
    valid_cvg_create ins tx = false.
Proof.
  intros ins tx n H. unfold valid_cvg_create. rewrite H. reflexivity.
Qed.

Lemma valid_cvg_create_oru_funded :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_create ins tx = true ->
    count_cvg (tx_creates tx) = 1%nat ->
    (0 < sum_asset AssetORU ins)%Z.
Proof.
  intros ins tx H Hc.
  unfold valid_cvg_create in H. rewrite Hc in H.
  apply andb_true_iff in H as [Ho _].
  apply Z.gtb_lt in Ho. exact Ho.
Qed.

(** Birth is not a mint: the transaction that creates a ticket creates no RES. *)
Lemma valid_cvg_create_no_res :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_create ins tx = true ->
    count_cvg (tx_creates tx) = 1%nat ->
    count_asset AssetRES (tx_creates tx) = 0%nat.
Proof.
  intros ins tx H Hc.
  unfold valid_cvg_create in H. rewrite Hc in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [Hr _].
  apply Nat.eqb_eq in Hr. exact Hr.
Qed.

Lemma valid_cvg_create_value_floor :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_create ins tx = true ->
    count_cvg (tx_creates tx) = 1%nat ->
    cvg_creates_value_ok (tx_creates tx) = true.
Proof.
  intros ins tx H Hc.
  unfold valid_cvg_create in H. rewrite Hc in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ Hv]. exact Hv.
Qed.

(** A covenant spend never mints a ticket, so the creation rule is vacuous on the spend path:
    the two rules constrain disjoint transaction shapes and cannot deadlock each other. *)
Lemma valid_cvg_create_vacuous_on_covenant_spend :
  forall (ins : list TxOut) (tx : TxAbs),
    valid_cvg_covenant_spend ins tx = true ->
    count_cvg ins = 1%nat ->
    valid_cvg_create ins tx = true.
Proof.
  intros ins tx H Hc.
  apply valid_cvg_create_no_create.
  apply (valid_cvg_covenant_spend_no_recreate ins tx H Hc).
Qed.

Fixpoint gather_inputs (u : UTXOSet) (ops : list OutPoint)
  : option (list TxOut) :=
  match ops with
  | nil => Some nil
  | op :: rest =>
      match utxo_lookup u op with
      | None => None
      | Some o =>
          match gather_inputs u rest with
          | None => None
          | Some os => Some (o :: os)
          end
      end
  end.

Definition res_in_of (u : UTXOSet) (tx : TxAbs) : option Z :=
  match gather_inputs u (tx_spends tx) with
  | Some ins => Some (sum_asset AssetRES ins)
  | None => None
  end.

Definition res_out_of (tx : TxAbs) : Z :=
  sum_asset AssetRES (tx_creates tx).

Definition no_res_to_oru (ins outs : list TxOut) : bool :=
  if Z.eqb (sum_asset AssetORU ins) 0 then Nat.eqb (count_asset AssetORU outs) 0
  else true.

Definition no_oru_to_res (ins outs : list TxOut) : bool :=
  if Z.eqb (sum_asset AssetRES ins) 0 then Nat.eqb (count_asset AssetRES outs) 0
  else true.

Definition valid_ordinary_dual_pool (ins : list TxOut) (tx : TxAbs) : bool :=
  andb (Z.geb (sum_asset AssetRES ins) (sum_asset AssetRES (tx_creates tx)))
  (andb (no_res_to_oru ins (tx_creates tx))
        (no_oru_to_res ins (tx_creates tx))).

Definition valid_non_mint_tx (u : UTXOSet) (tx : TxAbs) : bool :=
  match gather_inputs u (tx_spends tx) with
  | Some ins => valid_ordinary_dual_pool ins tx
  | None => false
  end.

(** Shared mint *output* shape: zero RES in, exactly one RES out of [reward]. *)
Definition res_mint_output_shape (tx : TxAbs) (ins : list TxOut) (reward : Z) : bool :=
  andb (Z.gtb reward 0)
  (andb (Z.eqb (sum_asset AssetRES ins) 0)
  (andb (Nat.eqb (count_asset AssetRES (tx_creates tx)) 1)
        (Z.eqb (sum_asset AssetRES (tx_creates tx)) reward))).

(** Abstract mint-shape: ORU-funded, zero RES in, exactly one RES out of
    amount [reward] = [W(h)]. Not a draw / owner / PCORB proof. *)
Definition res_mint_shape (tx : TxAbs) (ins : list TxOut) (reward : Z) : bool :=
  andb (Z.gtb reward 0)
  (andb (Z.eqb (sum_asset AssetRES ins) 0)
  (andb (Z.gtb (sum_asset AssetORU ins) 0)
  (andb (Nat.eqb (count_asset AssetRES (tx_creates tx)) 1)
        (Z.eqb (sum_asset AssetRES (tx_creates tx)) reward)))).

(** Covenant-funded mint: one CVG input, no ORU in or out, same RES output
    shape as [res_mint_shape], and [valid_cvg_covenant_spend] (owner-hash
    coupling). CVG is not ORU, so this path must not move the ORU pool. *)
Definition cvg_mint_shape (tx : TxAbs) (ins : list TxOut) (reward : Z) : bool :=
  andb (res_mint_output_shape tx ins reward)
  (andb (Nat.eqb (count_cvg ins) 1)
  (andb (Z.eqb (sum_asset AssetORU ins) 0)
  (andb (Nat.eqb (count_asset AssetORU (tx_creates tx)) 0)
        (valid_cvg_covenant_spend ins tx)))).

Definition is_scheduled_mint_shape (tx : TxAbs) (ins : list TxOut) (reward : Z) : bool :=
  orb (res_mint_shape tx ins reward) (cvg_mint_shape tx ins reward).

Lemma res_mint_shape_rejects_wrong_amount :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    (sum_asset AssetRES (tx_creates tx) <> reward)%Z ->
    res_mint_shape tx ins reward = false.
Proof.
  intros tx ins reward Hne.
  unfold res_mint_shape.
  apply Bool.andb_false_iff. right.
  apply Bool.andb_false_iff. right.
  apply Bool.andb_false_iff. right.
  apply Bool.andb_false_iff. right.
  apply Z.eqb_neq. exact Hne.
Qed.

Lemma res_mint_shape_rejects_zero_reward :
  forall (tx : TxAbs) (ins : list TxOut),
    res_mint_shape tx ins 0%Z = false.
Proof.
  intros tx ins. unfold res_mint_shape. reflexivity.
Qed.

Lemma cvg_mint_shape_rejects_zero_reward :
  forall (tx : TxAbs) (ins : list TxOut),
    cvg_mint_shape tx ins 0%Z = false.
Proof.
  intros tx ins. unfold cvg_mint_shape, res_mint_output_shape. reflexivity.
Qed.

Lemma cvg_mint_shape_zero_res_in :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    sum_asset AssetRES ins = 0%Z.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape, res_mint_output_shape in H.
  apply andb_true_iff in H as [Ho _].
  apply andb_true_iff in Ho as [_ Ho].
  apply andb_true_iff in Ho as [Hin _].
  apply Z.eqb_eq. exact Hin.
Qed.

Lemma cvg_mint_shape_reward_positive :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    (0 < reward)%Z.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape, res_mint_output_shape in H.
  apply andb_true_iff in H as [Ho _].
  apply andb_true_iff in Ho as [Hp _].
  apply Z.gtb_lt in Hp. exact Hp.
Qed.

Lemma cvg_mint_shape_one_res_out :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    count_asset AssetRES (tx_creates tx) = 1%nat /\
    sum_asset AssetRES (tx_creates tx) = reward.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape, res_mint_output_shape in H.
  apply andb_true_iff in H as [Ho _].
  apply andb_true_iff in Ho as [_ Ho].
  apply andb_true_iff in Ho as [_ Ho].
  apply andb_true_iff in Ho as [Hc Hv].
  apply Nat.eqb_eq in Hc. apply Z.eqb_eq in Hv.
  split; assumption.
Qed.

Lemma cvg_mint_shape_oru_unchanged :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    sum_asset AssetORU ins = 0%Z /\
    count_asset AssetORU (tx_creates tx) = 0%nat.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [Hin H].
  apply andb_true_iff in H as [Hc _].
  apply Z.eqb_eq in Hin. apply Nat.eqb_eq in Hc.
  split; assumption.
Qed.

Lemma cvg_mint_shape_count_cvg_1 :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    count_cvg ins = 1%nat.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [Hc _].
  apply Nat.eqb_eq in Hc. exact Hc.
Qed.

Lemma cvg_mint_shape_cvg_spend :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    valid_cvg_covenant_spend ins tx = true.
Proof.
  intros tx ins reward H.
  unfold cvg_mint_shape in H.
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ H].
  apply andb_true_iff in H as [_ Hsp].
  exact Hsp.
Qed.

Lemma cvg_mint_increases_res_by_exactly_W :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    (sum_asset AssetRES (tx_creates tx) - sum_asset AssetRES ins = reward)%Z /\
    (sum_asset AssetRES (tx_creates tx) = reward)%Z /\
    (sum_asset AssetRES ins = 0)%Z /\
    (0 < reward)%Z.
Proof.
  intros tx ins reward H.
  pose proof (cvg_mint_shape_zero_res_in tx ins reward H) as Hin.
  pose proof (cvg_mint_shape_one_res_out tx ins reward H) as [_ Hv].
  pose proof (cvg_mint_shape_reward_positive tx ins reward H) as Hp.
  repeat split; lia.
Qed.

Lemma is_scheduled_mint_shape_true_left :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    res_mint_shape tx ins reward = true ->
    is_scheduled_mint_shape tx ins reward = true.
Proof.
  intros tx ins reward H. unfold is_scheduled_mint_shape. rewrite H. reflexivity.
Qed.

Lemma is_scheduled_mint_shape_true_right :
  forall (tx : TxAbs) (ins : list TxOut) (reward : Z),
    cvg_mint_shape tx ins reward = true ->
    is_scheduled_mint_shape tx ins reward = true.
Proof.
  intros tx ins reward H. unfold is_scheduled_mint_shape.
  apply orb_true_iff. right. exact H.
Qed.

Definition valid_dual_pool_tx (u : UTXOSet) (tx : TxAbs) (reward : Z) : bool :=
  match gather_inputs u (tx_spends tx) with
  | None => false
  | Some ins =>
      if is_scheduled_mint_shape tx ins reward then true
      else valid_ordinary_dual_pool ins tx
  end.

(** Byte-shape conjunct of [apply_tx] (TASK-FV-MODEL-SCRIPT-SHAPE-LIFT-001).
    Missing inputs fail at [apply_tx_utxo]; this helper then skips the
    spend-CVG2 walk so DualPool fixtures are not double-rejected. *)
Definition valid_script_shape_tx (u : UTXOSet) (tx : TxAbs) : bool :=
  match gather_inputs u (tx_spends tx) with
  | None =>
      andb (forallb created_script_shape_ok (tx_creates tx)) (pcorb_shape_ok tx)
  | Some ins => valid_script_shape_on_ins ins tx
  end.

Lemma valid_script_shape_tx_of_on_ins :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut),
    gather_inputs u (tx_spends tx) = Some ins ->
    valid_script_shape_on_ins ins tx = true ->
    valid_script_shape_tx u tx = true.
Proof.
  intros u tx ins Hg Hok.
  unfold valid_script_shape_tx. rewrite Hg. exact Hok.
Qed.

Lemma valid_script_shape_tx_placeholder :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut),
    gather_inputs u (tx_spends tx) = Some ins ->
    tx_pcorb tx = None ->
    tx_creates tx = nil ->
    forallb (fun o => match out_script o with nil => true | _ => false end)
            ins = true ->
    valid_script_shape_tx u tx = true.
Proof.
  intros u tx ins Hg Hp Hc Hi.
  apply (valid_script_shape_tx_of_on_ins u tx ins Hg).
  apply valid_script_shape_on_ins_placeholder; assumption.
Qed.

Definition valid_cvg_spend_tx (u : UTXOSet) (tx : TxAbs) : bool :=
  match gather_inputs u (tx_spends tx) with
  | None => true
  | Some ins => valid_cvg_covenant_spend ins tx
  end.

Lemma valid_cvg_spend_tx_no_cvg :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut),
    gather_inputs u (tx_spends tx) = Some ins ->
    count_cvg ins = 0%nat ->
    valid_cvg_spend_tx u tx = true.
Proof.
  intros u tx ins Hg Hz.
  unfold valid_cvg_spend_tx. rewrite Hg.
  apply valid_cvg_covenant_spend_no_cvg. exact Hz.
Qed.

(** Creation gate: the model's counterpart to the covenant being retired from C++.

    The node no longer has the mechanism at all — 0xbc is above MAX_OPCODE and ASSET_CVG,
    TxoutType::CVG_CLAIM and both covenant shape checks are deleted — so no script it can
    classify is a covenant output and a C++ UTXO set is [utxo_cvg_free] by construction. This
    gate is what gives the model the same property without deleting the covenant definitions,
    which would rewrite the core of [apply_tx] and discard proved theorems. See
    [OpaqueBoundary] section 2 on the resulting over-approximation.

    [valid_cvg_create] above states what a ticket birth would have to satisfy and stays proved;
    with this gate in place it is only ever consulted at [count_cvg (tx_creates tx) = 0], where
    it holds vacuously. Anything reviving the mechanism owes the node an opcode first, and then
    owes this model the winner-authorization conjunct that a covenant mint never carried. *)
Definition cvg_create_gate (tx : TxAbs) : bool :=
  Nat.eqb (count_cvg (tx_creates tx)) 0%nat.

(** [None] mirrors [valid_cvg_spend_tx]: a transaction whose spends are not in the UTXO set is
    already rejected by [apply_tx_utxo], so the creation rule has nothing to say about it. The
    gate is outside that match because it reads only [tx_creates], exactly like the C++ check,
    which classifies outputs without consulting the coins view. *)
Definition valid_cvg_create_tx (u : UTXOSet) (tx : TxAbs) : bool :=
  andb (cvg_create_gate tx)
       (match gather_inputs u (tx_spends tx) with
        | None => true
        | Some ins => valid_cvg_create ins tx
        end).

Lemma valid_cvg_create_tx_no_create :
  forall (u : UTXOSet) (tx : TxAbs),
    count_cvg (tx_creates tx) = 0%nat ->
    valid_cvg_create_tx u tx = true.
Proof.
  intros u tx H.
  unfold valid_cvg_create_tx, cvg_create_gate. rewrite H. simpl.
  destruct (gather_inputs u (tx_spends tx)) as [ins|]; [|reflexivity].
  apply valid_cvg_create_no_create. exact H.
Qed.

Lemma valid_cvg_create_tx_of_create :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut),
    gather_inputs u (tx_spends tx) = Some ins ->
    valid_cvg_create_tx u tx = true ->
    valid_cvg_create ins tx = true.
Proof.
  intros u tx ins Hg H.
  unfold valid_cvg_create_tx in H. rewrite Hg in H.
  apply andb_true_iff in H as [_ H]. exact H.
Qed.

(** The gate is live: creating a ticket is not a replay-admissible shape at all, whatever the
    inputs look like. This is the conjunct [valid_cvg_create] alone cannot supply, since that
    rule admits a funded single ticket. *)
Theorem valid_cvg_create_tx_rejects_creation :
  forall (u : UTXOSet) (tx : TxAbs) (n : nat),
    count_cvg (tx_creates tx) = S n ->
    valid_cvg_create_tx u tx = false.
Proof.
  intros u tx n H.
  unfold valid_cvg_create_tx, cvg_create_gate. rewrite H. reflexivity.
Qed.

(** Mint-shape exception uses [W(h)] of the connecting block. Header connect
    requires [hdr_height = S (cm_height)], so this equals [cb_res_mint_amount]
    on the [apply_block] success path (RFC-017 / H-1: [W(h)=0] admits no mint). *)
Definition apply_tx_mint_reward (s : ReplayState) : Z :=
  get_res_mint_reward (Z.of_nat (S (cm_height (rs_chain s)))).

Lemma apply_tx_mint_reward_connecting :
  forall (s : ReplayState) (b : BlockAbs),
    valid_header_connect (rs_chain s) (blk_header b) = true ->
    apply_tx_mint_reward s = cb_res_mint_amount b.
Proof.
  intros s b H.
  unfold valid_header_connect in H.
  apply andb_true_iff in H as [Hh _].
  apply Nat.eqb_eq in Hh.
  unfold apply_tx_mint_reward, cb_res_mint_amount.
  rewrite Hh. reflexivity.
Qed.

(** ConnectBlock-aligned supply delta (ORU subsidy + capped RES mint). *)
Definition apply_supply_delta (s : ReplayState) (b : BlockAbs) : ReplayState :=
  apply_connectblock_supply_delta s b.

(** Snapshot before extension (rollback anchor). *)
Definition push_history (s_pre s_post : ReplayState) : ReplayState :=
  {| rs_utxo := rs_utxo s_post;
     rs_chain := rs_chain s_post;
     rs_oru_supply := rs_oru_supply s_post;
     rs_res_supply := rs_res_supply s_post;
     rs_history := snapshot_of_state s_pre :: rs_history s_post |}.

(** Apply one transaction. [tx_index] is the in-block position only.
    Succeeds only if spends exist, [valid_dual_pool_tx] holds for
    [apply_tx_mint_reward s] (ordinary [res_in ≥ res_out], or mint-shape),
    [apply_tx_derived_assets_obligation] holds (pooling = [f(script)]),
    [valid_cvg_spend_tx] holds ([cvg_spend_ok] plus owner-hash coupling),
    [valid_script_shape_tx] holds (created-script / spend-CVG2 /
    optional PCORB sizes), *and* [valid_cvg_create_tx] holds — which under the
    shipped [cvg_create_gate] means the transaction creates no ticket at all, and
    on the reopen path would mean ticket birth is ORU-funded, single, RES-free and
    above the face-value floor. Dual-pool is checked first so
    [apply_tx_rejects_without_dual_pool] is unchanged, and the creation gate is
    checked last so every earlier reject lemma is unchanged. *)
Definition apply_tx (s : ReplayState) (tx : TxAbs) (tx_index : nat)
  : option ReplayState :=
  match apply_tx_utxo (rs_utxo s) tx tx_index with
  | None => None
  | Some u' =>
      if valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s) then
        if apply_tx_derived_assets_obligation tx then
          if valid_cvg_spend_tx (rs_utxo s) tx then
            if valid_script_shape_tx (rs_utxo s) tx then
              if valid_cvg_create_tx (rs_utxo s) tx then
                Some (replay_set_utxo s u')
              else
                None
            else
              None
          else
            None
        else
          None
      else
        None
  end.

Lemma apply_tx_some_inv :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    exists u' : UTXOSet,
      apply_tx_utxo (rs_utxo s) tx idx = Some u' /\
      valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s) = true /\
      valid_script_shape_tx (rs_utxo s) tx = true /\
      s' = replay_set_utxo s u'.
Proof.
  intros s tx idx s' H.
  unfold apply_tx in H.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|] eqn:Eu; [|discriminate H].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s)) eqn:Ed;
    [|discriminate H].
  rewrite apply_tx_derived_assets_obligation_true in H.
  destruct (valid_cvg_spend_tx (rs_utxo s) tx) eqn:Ecvg; [|discriminate H].
  destruct (valid_script_shape_tx (rs_utxo s) tx) eqn:Esh; [|discriminate H].
  destruct (valid_cvg_create_tx (rs_utxo s) tx) eqn:Ecr; [|discriminate H].
  inversion H. subst s'.
  exists u'. repeat split; assumption.
Qed.

Theorem apply_tx_succeeds_implies_dual_pool :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s) = true.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [_ [_ [Hd _]]]. exact Hd.
Qed.

Theorem apply_tx_rejects_without_dual_pool :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat),
    valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s) = false ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx Hdp.
  unfold apply_tx.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|reflexivity].
  rewrite Hdp. reflexivity.
Qed.

Theorem apply_tx_succeeds_implies_script_shape :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    valid_script_shape_tx (rs_utxo s) tx = true.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [_ [_ [_ [Hsh _]]]]. exact Hsh.
Qed.

Theorem apply_tx_succeeds_implies_derived_assets :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    apply_tx_derived_assets_obligation tx = true.
Proof.
  intros s tx idx s' H. apply apply_tx_derived_assets_obligation_true.
Qed.

Theorem apply_tx_succeeds_implies_cvg_spend :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    valid_cvg_spend_tx (rs_utxo s) tx = true.
Proof.
  intros s tx idx s' H.
  unfold apply_tx in H.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|discriminate H].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s));
    [|discriminate H].
  rewrite apply_tx_derived_assets_obligation_true in H.
  destruct (valid_cvg_spend_tx (rs_utxo s) tx) eqn:Ecvg; [|discriminate H].
  reflexivity.
Qed.

Theorem apply_tx_succeeds_implies_cvg_create :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    valid_cvg_create_tx (rs_utxo s) tx = true.
Proof.
  intros s tx idx s' H.
  unfold apply_tx in H.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|discriminate H].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s));
    [|discriminate H].
  rewrite apply_tx_derived_assets_obligation_true in H.
  destruct (valid_cvg_spend_tx (rs_utxo s) tx); [|discriminate H].
  destruct (valid_script_shape_tx (rs_utxo s) tx); [|discriminate H].
  destruct (valid_cvg_create_tx (rs_utxo s) tx) eqn:Ecr; [|discriminate H].
  reflexivity.
Qed.

Theorem apply_tx_rejects_without_cvg_create :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat),
    valid_cvg_create_tx (rs_utxo s) tx = false ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx Hcr.
  unfold apply_tx.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|reflexivity].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s));
    [|reflexivity].
  rewrite apply_tx_derived_assets_obligation_true.
  destruct (valid_cvg_spend_tx (rs_utxo s) tx); [|reflexivity].
  destruct (valid_script_shape_tx (rs_utxo s) tx); [|reflexivity].
  rewrite Hcr. reflexivity.
Qed.

(** No reachable state was produced by a transaction that minted a ticket, so the model agrees
    with the node that no covenant ticket ever enters the UTXO set. *)
Theorem apply_tx_rejects_cvg_creation :
  forall (s : ReplayState) (tx : TxAbs) (idx n : nat),
    count_cvg (tx_creates tx) = S n ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx n H.
  apply apply_tx_rejects_without_cvg_create.
  apply (valid_cvg_create_tx_rejects_creation (rs_utxo s) tx n H).
Qed.

Theorem apply_tx_succeeds_implies_no_ticket_birth :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    count_cvg (tx_creates tx) = 0%nat.
Proof.
  intros s tx idx s' H.
  destruct (count_cvg (tx_creates tx)) as [|n] eqn:Ec; [reflexivity|].
  rewrite (apply_tx_rejects_cvg_creation s tx idx n Ec) in H. discriminate H.
Qed.

(** Ticket birth facts available from block replay alone, with no extra hypotheses beyond
    "the transaction was applied". This is the model half of the C++ obligation that
    [OP_CVG_CLAIM] outputs are only ever produced by a funded convergence transaction.

    Under [cvg_create_gate] the [count_cvg = 1] hypothesis is unsatisfiable together with a
    successful [apply_tx] (see [apply_tx_succeeds_implies_no_ticket_birth]), so this and
    [cvg_ticket_birth_creates_no_res] are vacuous as shipped. They are kept because they state
    what the reopen path must re-establish, and they still discharge the [valid_cvg_create]
    conjuncts rather than assuming them. *)
Theorem apply_tx_ticket_birth_is_funded :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    count_cvg (tx_creates tx) = 1%nat ->
    (0 < sum_asset AssetORU ins)%Z /\
    count_asset AssetRES (tx_creates tx) = 0%nat /\
    cvg_creates_value_ok (tx_creates tx) = true.
Proof.
  intros s tx idx s' ins Happ Hg Hc.
  pose proof (apply_tx_succeeds_implies_cvg_create s tx idx s' Happ) as Hcr.
  pose proof (valid_cvg_create_tx_of_create (rs_utxo s) tx ins Hg Hcr) as Hcreate.
  split; [apply (valid_cvg_create_oru_funded ins tx Hcreate Hc)|].
  split; [apply (valid_cvg_create_no_res ins tx Hcreate Hc)|].
  apply (valid_cvg_create_value_floor ins tx Hcreate Hc).
Qed.

(** More than one ticket in a transaction is not a replay-admissible shape. *)
Theorem apply_tx_rejects_two_tickets :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (ins : list TxOut) (n : nat),
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    count_cvg (tx_creates tx) = S (S n) ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx ins n Hg Hc.
  apply (apply_tx_rejects_cvg_creation s tx idx (S n) Hc).
Qed.

Theorem apply_tx_succeeds_implies_cvg_spend_ok :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState)
         (ins : list TxOut),
    apply_tx s tx idx = Some s' ->
    gather_inputs (rs_utxo s) (tx_spends tx) = Some ins ->
    cvg_spend_ok ins tx = true.
Proof.
  intros s tx idx s' ins H Hg.
  pose proof (apply_tx_succeeds_implies_cvg_spend s tx idx s' H) as Hcvg.
  unfold valid_cvg_spend_tx in Hcvg. rewrite Hg in Hcvg.
  apply valid_cvg_covenant_spend_implies_cvg_spend_ok. exact Hcvg.
Qed.

Theorem apply_tx_rejects_without_cvg_spend :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat),
    valid_cvg_spend_tx (rs_utxo s) tx = false ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx Hcvg.
  unfold apply_tx.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|reflexivity].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s));
    [|reflexivity].
  rewrite apply_tx_derived_assets_obligation_true.
  rewrite Hcvg. reflexivity.
Qed.

Theorem apply_tx_rejects_without_script_shape :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat),
    valid_script_shape_tx (rs_utxo s) tx = false ->
    apply_tx s tx idx = None.
Proof.
  intros s tx idx Hsh.
  unfold apply_tx.
  destruct (apply_tx_utxo (rs_utxo s) tx idx) as [u'|]; [|reflexivity].
  destruct (valid_dual_pool_tx (rs_utxo s) tx (apply_tx_mint_reward s));
    [|reflexivity].
  rewrite apply_tx_derived_assets_obligation_true.
  destruct (valid_cvg_spend_tx (rs_utxo s) tx); [|reflexivity].
  rewrite Hsh. reflexivity.
Qed.

Lemma apply_tx_preserves_chain :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    rs_chain s' = rs_chain s.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u [_ [_ [_ Hs]]]]. subst s'. reflexivity.
Qed.

Lemma apply_tx_preserves_history :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    rs_history s' = rs_history s.
Proof.
  intros s tx idx s' H.
  apply apply_tx_some_inv in H.
  destruct H as [u [_ [_ [_ Hs]]]]. subst s'. reflexivity.
Qed.

Lemma apply_tx_preserves_mint_reward :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    apply_tx_mint_reward s' = apply_tx_mint_reward s.
Proof.
  intros s tx idx s' H.
  unfold apply_tx_mint_reward.
  rewrite (apply_tx_preserves_chain s tx idx s' H). reflexivity.
Qed.

Fixpoint apply_txs (s : ReplayState) (txs : list TxAbs) (idx : nat)
  : option ReplayState :=
  match txs with
  | nil => Some s
  | t :: rest =>
      match apply_tx s t idx with
      | None => None
      | Some s' => apply_txs s' rest (S idx)
      end
  end.

Lemma apply_txs_cons_inv :
  forall (s : ReplayState) (t : TxAbs) (rest : list TxAbs) (idx : nat)
         (s' : ReplayState),
    apply_txs s (t :: rest) idx = Some s' ->
    exists s1 : ReplayState,
      apply_tx s t idx = Some s1 /\
      apply_txs s1 rest (S idx) = Some s'.
Proof.
  intros s t rest idx s' H.
  simpl in H.
  destruct (apply_tx s t idx) as [s1|] eqn:E; [|discriminate H].
  exists s1. split; [reflexivity | exact H].
Qed.

Lemma apply_txs_preserves_history :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    rs_history s' = rs_history s.
Proof.
  intros s txs idx s'. revert s idx.
  induction txs as [|t rest IH]; intros s idx H; simpl in H.
  - injection H as ->. reflexivity.
  - destruct (apply_tx s t idx) as [s1|] eqn:Hat; [|discriminate H].
    rewrite (IH s1 (S idx) H).
    apply apply_tx_preserves_history in Hat. exact Hat.
Qed.

Lemma apply_txs_preserves_chain :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    rs_chain s' = rs_chain s.
Proof.
  intros s txs idx s'. revert s idx.
  induction txs as [|t rest IH]; intros s idx H; simpl in H.
  - injection H as ->. reflexivity.
  - destruct (apply_tx s t idx) as [s1|] eqn:Hat; [|discriminate H].
    rewrite (IH s1 (S idx) H).
    apply apply_tx_preserves_chain in Hat. exact Hat.
Qed.

(** * Ticket-freeness of reachable states

    No transaction may create a ticket, therefore no ticket is ever in the UTXO set, therefore
    no covenant mint can be presented. [cvg_create_gate] is the first step; this section is the
    second.

    It is what makes [cvg_mint_shape] *unreachable* rather than merely rejected downstream, which
    matters because that shape carries no winner authorization: it checks amounts, owner-hash
    coupling and one-shot consumption, and says nothing about who won the draw. The node reached
    the same place by deleting the mechanism outright, so these theorems apply to it with the
    hypothesis discharged by construction.

    A ticket present in the *initial* UTXO set is outside the claim, and that is the honest
    boundary: the theorems below say a replay that starts ticket-free stays ticket-free, not that
    an injected ticket is harmless. The [DualPoolConservation] covenant fixtures inject one
    deliberately, which is why they still exercise the spend path. *)

Definition utxo_cvg_free (u : UTXOSet) : bool :=
  forallb (fun e => negb (is_cvg_covenant_output (snd e))) u.

Lemma count_cvg_zero_forallb :
  forall outs : list TxOut,
    count_cvg outs = 0%nat ->
    forallb (fun o => negb (is_cvg_covenant_output o)) outs = true.
Proof.
  intros outs H. unfold count_cvg in H.
  induction outs as [|o rest IH]; [reflexivity|].
  simpl in H. simpl.
  destruct (is_cvg_covenant_output o); simpl in H |- *.
  - discriminate H.
  - apply IH. exact H.
Qed.

Lemma forallb_filter :
  forall (A : Type) (f g : A -> bool) (l : list A),
    forallb f l = true ->
    forallb f (filter g l) = true.
Proof.
  intros A f g l. induction l as [|x xs IH]; intros H; [reflexivity|].
  simpl in H. apply andb_true_iff in H as [Hx Hxs].
  simpl. destruct (g x).
  - simpl. rewrite Hx. simpl. apply IH. exact Hxs.
  - apply IH. exact Hxs.
Qed.

Lemma utxo_cvg_free_remove :
  forall (u : UTXOSet) (op : OutPoint),
    utxo_cvg_free u = true ->
    utxo_cvg_free (utxo_remove u op) = true.
Proof.
  intros u op H. unfold utxo_cvg_free, utxo_remove in *.
  apply forallb_filter. exact H.
Qed.

Lemma utxo_cvg_free_fold_remove :
  forall (ops : list OutPoint) (u : UTXOSet),
    utxo_cvg_free u = true ->
    utxo_cvg_free (fold_left (fun u' op => utxo_remove u' op) ops u) = true.
Proof.
  intros ops. induction ops as [|op rest IH]; intros u H; simpl; [exact H|].
  apply IH. apply utxo_cvg_free_remove. exact H.
Qed.

Lemma utxo_cvg_free_app :
  forall a b : UTXOSet,
    utxo_cvg_free a = true ->
    utxo_cvg_free b = true ->
    utxo_cvg_free (a ++ b) = true.
Proof.
  intros a b. unfold utxo_cvg_free.
  induction a as [|e rest IH]; intros Ha Hb; simpl in *; [exact Hb|].
  apply andb_true_iff in Ha as [He Hrest].
  rewrite He. simpl. apply IH; assumption.
Qed.

Lemma utxo_cvg_free_insert :
  forall (u : UTXOSet) (op : OutPoint) (o : TxOut),
    utxo_cvg_free u = true ->
    is_cvg_covenant_output o = false ->
    utxo_cvg_free (utxo_insert u op o) = true.
Proof.
  intros u op o Hu Ho. unfold utxo_insert.
  apply utxo_cvg_free_app.
  - apply utxo_cvg_free_remove. exact Hu.
  - unfold utxo_cvg_free. simpl. rewrite Ho. reflexivity.
Qed.

Lemma utxo_cvg_free_fold_create :
  forall (creates : list TxOut) (u : UTXOSet) (hid : Hash32) (base : nat),
    utxo_cvg_free u = true ->
    forallb (fun o => negb (is_cvg_covenant_output o)) creates = true ->
    utxo_cvg_free (fold_create_outputs creates u hid base) = true.
Proof.
  unfold fold_create_outputs.
  intros creates. induction creates as [|o rest IH]; intros u hid base Hu Hc; simpl.
  - exact Hu.
  - simpl in Hc. apply andb_true_iff in Hc as [Ho Hrest].
    apply negb_true_iff in Ho.
    apply IH.
    + apply utxo_cvg_free_insert; assumption.
    + exact Hrest.
Qed.

(** Spends can only remove and creates carry no ticket, so the set cannot acquire one. *)
Lemma apply_tx_utxo_preserves_cvg_free :
  forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (u' : UTXOSet),
    apply_tx_utxo u tx idx = Some u' ->
    utxo_cvg_free u = true ->
    count_cvg (tx_creates tx) = 0%nat ->
    utxo_cvg_free u' = true.
Proof.
  intros u tx idx u' Happ Hu Hc.
  unfold apply_tx_utxo in Happ. cbv zeta in Happ.
  destruct (forallb
              (fun op => match utxo_lookup u op with Some _ => true | None => false end)
              (tx_spends tx)); [|discriminate Happ].
  injection Happ as Heq. rewrite <- Heq.
  apply utxo_cvg_free_fold_create.
  - apply utxo_cvg_free_fold_remove. exact Hu.
  - apply count_cvg_zero_forallb. exact Hc.
Qed.

(** [apply_tx] supplies the [count_cvg (tx_creates tx) = 0] side condition itself, via
    [cvg_create_gate], so preservation needs no hypothesis about the transaction. *)
Theorem apply_tx_preserves_cvg_free :
  forall (s : ReplayState) (tx : TxAbs) (idx : nat) (s' : ReplayState),
    apply_tx s tx idx = Some s' ->
    utxo_cvg_free (rs_utxo s) = true ->
    utxo_cvg_free (rs_utxo s') = true.
Proof.
  intros s tx idx s' Happ Hu.
  pose proof (apply_tx_succeeds_implies_no_ticket_birth s tx idx s' Happ) as Hc.
  destruct (apply_tx_some_inv s tx idx s' Happ) as [u' [Hutxo [_ [_ Hs]]]].
  subst s'. simpl.
  apply (apply_tx_utxo_preserves_cvg_free (rs_utxo s) tx idx u' Hutxo Hu Hc).
Qed.

Theorem apply_txs_preserves_cvg_free :
  forall (txs : list TxAbs) (s : ReplayState) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    utxo_cvg_free (rs_utxo s) = true ->
    utxo_cvg_free (rs_utxo s') = true.
Proof.
  intros txs. induction txs as [|t rest IH]; intros s idx s' H Hu; simpl in H.
  - injection H as Heq. rewrite <- Heq. exact Hu.
  - destruct (apply_tx s t idx) as [s1|] eqn:Hat; [|discriminate H].
    apply (IH s1 (S idx) s' H).
    apply (apply_tx_preserves_cvg_free s t idx s1 Hat Hu).
Qed.

Lemma utxo_lookup_cvg_free :
  forall (u : UTXOSet) (op : OutPoint) (o : TxOut),
    utxo_cvg_free u = true ->
    utxo_lookup u op = Some o ->
    is_cvg_covenant_output o = false.
Proof.
  intros u op o. unfold utxo_cvg_free.
  induction u as [|[op' o'] rest IH]; intros Hu Hl; simpl in *; [discriminate Hl|].
  apply andb_true_iff in Hu as [He Hrest].
  destruct (outpoint_eqb op op').
  - injection Hl as Heq. rewrite <- Heq. apply negb_true_iff in He. exact He.
  - apply IH; assumption.
Qed.

Lemma gather_inputs_cvg_free :
  forall (ops : list OutPoint) (u : UTXOSet) (ins : list TxOut),
    utxo_cvg_free u = true ->
    gather_inputs u ops = Some ins ->
    count_cvg ins = 0%nat.
Proof.
  intros ops. induction ops as [|op rest IH]; intros u ins Hu Hg; simpl in Hg.
  - injection Hg as Heq. rewrite <- Heq. reflexivity.
  - destruct (utxo_lookup u op) as [o|] eqn:Hl; [|discriminate Hg].
    destruct (gather_inputs u rest) as [os|] eqn:Hr; [|discriminate Hg].
    injection Hg as Heq. rewrite <- Heq.
    unfold count_cvg. simpl.
    rewrite (utxo_lookup_cvg_free u op o Hu Hl).
    exact (IH u os Hu Hr).
Qed.

(** The payoff. [cvg_mint_shape] demands exactly one CVG input; a ticket-free set supplies none,
    so the shape cannot be formed at all. *)
Theorem cvg_mint_shape_unreachable_from_cvg_free :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut) (reward : Z),
    utxo_cvg_free u = true ->
    gather_inputs u (tx_spends tx) = Some ins ->
    cvg_mint_shape tx ins reward = false.
Proof.
  intros u tx ins reward Hu Hg.
  destruct (cvg_mint_shape tx ins reward) eqn:E; [|reflexivity].
  pose proof (cvg_mint_shape_count_cvg_1 tx ins reward E) as H1.
  rewrite (gather_inputs_cvg_free (tx_spends tx) u ins Hu Hg) in H1.
  discriminate H1.
Qed.

(** Closes the P0-2 modelling gap from the other side. On a ticket-free chain the only scheduled
    mint shape available is [res_mint_shape] — the ORU-funded one, which is what
    [DualPoolConservation.valid_body_mint_count] couples to a claimed win. The covenant disjunct,
    the one that carries no winner authorization, contributes nothing. *)
Theorem scheduled_mint_shape_is_oru_funded_from_cvg_free :
  forall (u : UTXOSet) (tx : TxAbs) (ins : list TxOut) (reward : Z),
    utxo_cvg_free u = true ->
    gather_inputs u (tx_spends tx) = Some ins ->
    is_scheduled_mint_shape tx ins reward = res_mint_shape tx ins reward.
Proof.
  intros u tx ins reward Hu Hg.
  unfold is_scheduled_mint_shape.
  rewrite (cvg_mint_shape_unreachable_from_cvg_free u tx ins reward Hu Hg).
  apply Bool.orb_false_r.
Qed.

(** Whole-replay statement: a transaction applied against any state reachable from a ticket-free
    start can never present a covenant mint. Together with [cvg_create_gate] this is the model's
    version of the C++ claim that [OP_CVG_CLAIM] is unreachable — not "rejected at ConnectBlock",
    but "no such transaction exists on any accepted chain that started ticket-free". *)
Theorem replay_from_cvg_free_admits_no_covenant_mint :
  forall (txs : list TxAbs) (s s' : ReplayState) (idx : nat)
         (tx : TxAbs) (ins : list TxOut),
    utxo_cvg_free (rs_utxo s) = true ->
    apply_txs s txs idx = Some s' ->
    gather_inputs (rs_utxo s') (tx_spends tx) = Some ins ->
    cvg_mint_shape tx ins (apply_tx_mint_reward s') = false.
Proof.
  intros txs s s' idx tx ins Hu Happ Hg.
  apply (cvg_mint_shape_unreachable_from_cvg_free (rs_utxo s') tx ins
           (apply_tx_mint_reward s')
           (apply_txs_preserves_cvg_free txs s idx s' Happ Hu) Hg).
Qed.

(** Every body tx of a successful [apply_txs] passed [valid_script_shape_tx]
    at the UTXO it was applied against (TASK-FV-MODEL-SMALL-LIFT-FOLLOWUPS-001).
    Twin of [DualPoolConservation.apply_txs_dual_pool]. Pooling is
    [output_asset] = [f(script)]; this conjunct is the byte-shape gate. *)
Fixpoint apply_txs_script_shape (s : ReplayState) (txs : list TxAbs) (idx : nat)
                                (s_end : ReplayState) : Prop :=
  match txs with
  | nil => s_end = s
  | t :: rest =>
      valid_script_shape_tx (rs_utxo s) t = true /\
      exists s1 : ReplayState,
        apply_tx s t idx = Some s1 /\
        apply_txs_script_shape s1 rest (S idx) s_end
  end.

Theorem apply_txs_succeeds_implies_script_shape :
  forall (s : ReplayState) (txs : list TxAbs) (idx : nat) (s' : ReplayState),
    apply_txs s txs idx = Some s' ->
    apply_txs_script_shape s txs idx s'.
Proof.
  intros s txs idx s'. revert s idx.
  induction txs as [|t rest IH]; intros s idx H; simpl in *.
  - injection H as ->. reflexivity.
  - apply apply_txs_cons_inv in H.
    destruct H as [s1 [Hat Hrest]].
    split.
    + eapply apply_tx_succeeds_implies_script_shape. exact Hat.
    + exists s1. split; [exact Hat | apply IH; exact Hrest].
Qed.

Lemma apply_txs_rejects_head_without_script_shape :
  forall (s : ReplayState) (t : TxAbs) (rest : list TxAbs) (idx : nat),
    valid_script_shape_tx (rs_utxo s) t = false ->
    apply_txs s (t :: rest) idx = None.
Proof.
  intros s t rest idx H.
  simpl. rewrite (apply_tx_rejects_without_script_shape s t idx H). reflexivity.
Qed.

(** [ApplyBlock]: total on valid inputs; [None] = rejection (ConnectBlock ⊥). *)
Definition apply_block_body (s : ReplayState) (b : BlockAbs) : option ReplayState :=
  match apply_txs s (blk_txs b) 0 with
  | None => None
  | Some s_tx =>
      let hdr := blk_header b in
      let cm' :=
        {| cm_height := hdr_height hdr;
           cm_chainwork := hdr_chainwork hdr;
           cm_tip_hash := hash32_placeholder |} in
      let s' :=
        apply_supply_delta (replay_set_chain s_tx cm') b in
      Some (push_history s s')
  end.

Definition apply_block (s : ReplayState) (b : BlockAbs) : option ReplayState :=
  if valid_block_connect s b then apply_block_body s b else None.

Lemma apply_block_rejects_unless_connect :
  forall (s : ReplayState) (b : BlockAbs),
    valid_block_connect s b = false ->
    apply_block s b = None.
Proof.
  intros s b H. unfold apply_block. rewrite H. reflexivity.
Qed.

(** Explicit reject path: overshoot is [None], not a zeroed mint delta. *)
Lemma apply_block_rejects_cap_overshoot :
  forall (s : ReplayState) (b : BlockAbs),
    valid_res_cap s b = false ->
    apply_block s b = None.
Proof.
  intros s b H.
  unfold apply_block, valid_block_connect.
  rewrite H. rewrite Bool.andb_false_r. reflexivity.
Qed.

Lemma apply_block_rejects_one_atom_overshoot :
  forall (s : ReplayState) (b : BlockAbs),
    (rs_res_supply s + cb_res_attempted_amount b = cb_MAX_RES_SUPPLY + 1)%Z ->
    apply_block s b = None.
Proof.
  intros s b Heq.
  apply apply_block_rejects_cap_overshoot.
  apply valid_res_cap_overshoot_one_atom. exact Heq.
Qed.

Lemma apply_block_rejects_overshoot :
  forall (s : ReplayState) (b : BlockAbs),
    (cb_MAX_RES_SUPPLY < rs_res_supply s + cb_res_attempted_amount b)%Z ->
    apply_block s b = None.
Proof.
  intros s b Hlt.
  apply apply_block_rejects_cap_overshoot.
  apply valid_res_cap_overshoot. exact Hlt.
Qed.

(** Fold a block sequence from [s0] (deterministic when defined). *)
Fixpoint fold_apply_blocks (s : ReplayState) (bs : list BlockAbs)
  : option ReplayState :=
  match bs with
  | nil => Some s
  | b :: rest =>
      match apply_block s b with
      | None => None
      | Some s' => fold_apply_blocks s' rest
      end
  end.

(** Structural validity: every block connects to the running tip. *)
Fixpoint valid_block_sequence (s : ReplayState) (bs : list BlockAbs) : bool :=
  match bs with
  | nil => true
  | b :: rest =>
      if valid_block_connect s b then
        match apply_block_body s b with
        | None => false
        | Some s' => valid_block_sequence s' rest
        end
      else
        false
  end.

Lemma apply_block_deterministic :
  forall (s : ReplayState) (b : BlockAbs) (r1 r2 : ReplayState),
    apply_block s b = Some r1 ->
    apply_block s b = Some r2 ->
    r1 = r2.
Proof.
  intros s b r1 r2 H1 H2.
  congruence.
Qed.

Lemma valid_block_connect_apply_block :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    valid_block_connect s b = true ->
    apply_block_body s b = Some s' ->
    apply_block s b = Some s'.
Proof.
  intros s b s' Hc Hbody.
  unfold apply_block. rewrite Hc. exact Hbody.
Qed.

Lemma apply_block_some_inv :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    valid_block_connect s b = true /\ apply_block_body s b = Some s'.
Proof.
  intros s b s' H.
  unfold apply_block in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  split; [reflexivity | exact H].
Qed.

Theorem apply_block_body_succeeds_implies_script_shape :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block_body s b = Some s' ->
    exists s_tx : ReplayState,
      apply_txs s (blk_txs b) 0 = Some s_tx /\
      apply_txs_script_shape s (blk_txs b) 0 s_tx.
Proof.
  intros s b s' H.
  unfold apply_block_body in H.
  destruct (apply_txs s (blk_txs b) 0) as [s_tx|] eqn:Ht; [|discriminate H].
  exists s_tx. split; [reflexivity|].
  apply apply_txs_succeeds_implies_script_shape. exact Ht.
Qed.

(** Accepted block ⇒ every body tx passed the finite shape conjunct
    (created [IsAllowedScript] / zero-value [OP_RETURN], no spend-CVG2,
    optional PCORB sizes). Empty scripts are [AssetInvalid] and fail.
    Not [IsResScript] vs tag. *)
Theorem apply_block_succeeds_implies_script_shape :
  forall (s : ReplayState) (b : BlockAbs) (s' : ReplayState),
    apply_block s b = Some s' ->
    exists s_tx : ReplayState,
      apply_txs s (blk_txs b) 0 = Some s_tx /\
      apply_txs_script_shape s (blk_txs b) 0 s_tx.
Proof.
  intros s b s' H.
  apply apply_block_some_inv in H.
  destruct H as [_ Hb].
  exact (apply_block_body_succeeds_implies_script_shape s b s' Hb).
Qed.

Lemma valid_block_sequence_head :
  forall (s : ReplayState) (b : BlockAbs) (rest : list BlockAbs),
    valid_block_sequence s (b :: rest) = true ->
    exists s',
      apply_block_body s b = Some s' /\
      valid_block_sequence s' rest = true.
Proof.
  intros s b rest H.
  simpl in H.
  destruct (valid_block_connect s b) eqn:Hc; [|discriminate H].
  destruct (apply_block_body s b) as [s'|] eqn:Hb; [|discriminate H].
  exists s'. split; eauto.
Qed.

Lemma fold_apply_blocks_cons :
  forall (s : ReplayState) (b : BlockAbs) (rest : list BlockAbs)
         (s' s'' : ReplayState),
    apply_block s b = Some s' ->
    fold_apply_blocks s' rest = Some s'' ->
    fold_apply_blocks s (b :: rest) = Some s''.
Proof.
  intros s b rest s' s'' Hab Hfold.
  simpl. rewrite Hab. exact Hfold.
Qed.

Theorem valid_block_sequence_fold_some :
  forall (s : ReplayState) (blocks : list BlockAbs),
    valid_block_sequence s blocks = true ->
    exists s', fold_apply_blocks s blocks = Some s'.
Proof.
  intros s blocks H.
  revert s H.
  induction blocks as [|b rest IH]; intros s H; simpl in H.
  - exists s. reflexivity.
  - destruct (valid_block_connect s b) eqn:Hc; [|discriminate].
    destruct (apply_block_body s b) as [s1|] eqn:Hb; [|discriminate].
    destruct (valid_block_sequence s1 rest) eqn:Hrest; [|discriminate].
    destruct (IH s1 Hrest) as [s'' Hfold].
    exists s''.
    eapply fold_apply_blocks_cons; eauto.
    eapply valid_block_connect_apply_block; eauto.
Qed.
