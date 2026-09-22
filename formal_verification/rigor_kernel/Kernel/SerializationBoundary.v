(** RigorKernel.Kernel.SerializationBoundary — kernel object encoding.

    Scope: Coq consensus kernel only.

    Deterministic encoding of kernel objects used by the Coq model.
    No cryptographic hash. No network serialization. No transaction
    wire format.

    Proved here: the encoding contract (deterministic / same-state /
    ORU decode-encode identity). Extraction source-identity is proved
    in ExtractionBoundary, not here.

    Not proved: cryptographic primitives (see Kernel/CryptoAssumptions.v)
    and a full extraction refinement.

    Cover:
      UTXO state
      ORU state
      RES schedule
      PoW target

    NOT CLAIMED:
      Crypto-layer correctness
      P2P / block / tx wire format
      C++ serialization equivalence
      Full extraction refinement
*)
From Coq Require Import List.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From RigorKernel Require Import Supply.ORUModel.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Pow.CompactBits.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.

Local Open Scope Z_scope.

Definition encode_bool (b : bool) : Z := if b then 1 else 0.

Definition encode_dom (d : input_domain) : Z :=
  match d with
  | DomORU => 0
  | DomRES => 1
  end.

Definition decode_dom (z : Z) : option input_domain :=
  if z =? 0 then Some DomORU
  else if z =? 1 then Some DomRES
  else None.

(** * UTXO state *)

Definition encode_utxo (s : tx) : list Z :=
  encode_bool (tx_coinbase s)
    :: Z.of_nat (length (tx_in s))
    :: Z.of_nat (length (tx_out s))
    :: map txo_value (tx_out s).

(** * ORU state *)

Definition encode_oru (s : kernel_tx) : list Z :=
  [encode_dom (kt_dom s);
   kt_oru_in s;
   kt_oru_out s;
   kt_res_in s;
   kt_res_out s;
   kt_old_supply s;
   kt_subsidy s].

Definition decode_oru (xs : list Z) : option kernel_tx :=
  match xs with
  | d :: a :: b :: c :: e :: f :: g :: nil =>
      match decode_dom d with
      | Some dom =>
          Some {| kt_dom := dom;
                  kt_oru_in := a;
                  kt_oru_out := b;
                  kt_res_in := c;
                  kt_res_out := e;
                  kt_old_supply := f;
                  kt_subsidy := g |}
      | None => None
      end
  | _ => None
  end.

(** * RES schedule *)

Definition encode_res_schedule (h : Z) : list Z :=
  [h; res_destroy_cost_at_height h].

(** * PoW target *)

Definition encode_pow_target (t : Z) : list Z :=
  [t; compact_encode t].

(** Statement: encoding is a function (same term, same bytes).
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem encoding_deterministic :
  forall s : kernel_tx, encode_oru s = encode_oru s.
Proof. intros s. reflexivity. Qed.

Theorem encode_utxo_deterministic :
  forall s : tx, encode_utxo s = encode_utxo s.
Proof. intros s. reflexivity. Qed.

Theorem encode_res_schedule_deterministic :
  forall h : Z, encode_res_schedule h = encode_res_schedule h.
Proof. intros h. reflexivity. Qed.

Theorem encode_pow_target_deterministic :
  forall t : Z, encode_pow_target t = encode_pow_target t.
Proof. intros t. reflexivity. Qed.

(** Statement: equal states have equal encodings.
    Assumptions: [s1 = s2].
    Coq consensus kernel only: yes. *)
Theorem same_state_same_encoding :
  forall s1 s2 : kernel_tx,
    s1 = s2 -> encode_oru s1 = encode_oru s2.
Proof. intros s1 s2 Heq. rewrite Heq. reflexivity. Qed.

Theorem same_utxo_same_encoding :
  forall s1 s2 : tx,
    s1 = s2 -> encode_utxo s1 = encode_utxo s2.
Proof. intros s1 s2 Heq. rewrite Heq. reflexivity. Qed.

Theorem same_res_schedule_same_encoding :
  forall h1 h2 : Z,
    h1 = h2 -> encode_res_schedule h1 = encode_res_schedule h2.
Proof. intros h1 h2 Heq. rewrite Heq. reflexivity. Qed.

Theorem same_pow_target_same_encoding :
  forall t1 t2 : Z,
    t1 = t2 -> encode_pow_target t1 = encode_pow_target t2.
Proof. intros t1 t2 Heq. rewrite Heq. reflexivity. Qed.

(** Statement: ORU decode inverts encode.
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem decode_encode_identity :
  forall s : kernel_tx,
    decode_oru (encode_oru s) = Some s.
Proof.
  intros [dom a b c e f g].
  unfold encode_oru, decode_oru, encode_dom, decode_dom.
  destruct dom; simpl; reflexivity.
Qed.

(** Statement: no two distinct ORU states share a wire encoding.
    Assumptions: none beyond [decode_encode_identity].
    Coq consensus kernel only: yes.

    The determinism statements in this file -- [encoding_deterministic]
    and [same_state_same_encoding] -- are free: every Coq function is
    deterministic and respects equality, so they hold whatever
    [encode_oru] does. The direction that is not free is this one. Two
    states that disagree must encode differently, or a node could be
    shown one state and commit to another, and the encoding would not
    determine what was signed.

    Proved from the encoded list rather than through
    [decode_encode_identity]. Routing it through the round trip would
    make it forward to that lemma, and a mutation that collapses two
    fields of [encode_oru] would be caught there and never reach this
    statement -- the injectivity claim would ride on a proof nothing
    tests. *)
Theorem encode_oru_injective :
  forall s1 s2 : kernel_tx,
    encode_oru s1 = encode_oru s2 -> s1 = s2.
Proof.
  intros [d1 a1 b1 c1 e1 f1 g1] [d2 a2 b2 c2 e2 f2 g2] Heq.
  unfold encode_oru, encode_dom in Heq. simpl in Heq.
  destruct d1, d2; inversion Heq; subst; reflexivity.
Qed.

(** Encoding contract is proved. Crypto primitives and full
    extraction refinement are not. *)
Definition serialization_proves_crypto : bool := false.
Definition serialization_proves_full_extraction_refinement : bool := false.

Theorem encoding_contract_proved_crypto_not_proved :
  serialization_proves_crypto = false
  /\ serialization_proves_full_extraction_refinement = false.
Proof. split; reflexivity. Qed.
