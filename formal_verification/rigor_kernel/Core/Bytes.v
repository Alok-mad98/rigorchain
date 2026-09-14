(** RigorKernel.Core.Bytes — Phase A foundation.

    Scope: consensus kernel only.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
From Coq Require Import Lia.
From Coq Require Import ZArith.ZArith.
Import ListNotations.

Definition byte := nat.

Definition byte_ok (b : byte) : Prop := (b <=? 255)%nat = true.

Fixpoint all_byte_ok (bs : list byte) : Prop :=
  match bs with
  | nil => True
  | b :: rest => byte_ok b /\ all_byte_ok rest
  end.

Fixpoint repeat_byte (n : nat) (v : byte) : list byte :=
  match n with
  | O => nil
  | S n' => v :: repeat_byte n' v
  end.

Fixpoint split_at (n : nat) (xs : list byte) : option (list byte * list byte) :=
  match n, xs with
  | O, _ => Some (nil, xs)
  | S n', x :: xs' =>
      match split_at n' xs' with
      | Some (pre, post) => Some (x :: pre, post)
      | None => None
      end
  | S _, nil => None
  end.

Fixpoint app_bytes (xs ys : list byte) : list byte :=
  match xs with
  | nil => ys
  | x :: xrest => x :: app_bytes xrest ys
  end.

(** Little-endian 32-bit encoding of a [Z]. Used only as an abstract
    header serializer; not a compact-nBits codec (ASERT is out of scope). *)
Definition le32_bytes (z : Z) : list byte :=
  let b0 := Z.to_nat (Z.modulo z 256) in
  let z1 := Z.div z 256 in
  let b1 := Z.to_nat (Z.modulo z1 256) in
  let z2 := Z.div z1 256 in
  let b2 := Z.to_nat (Z.modulo z2 256) in
  let z3 := Z.div z2 256 in
  let b3 := Z.to_nat (Z.modulo z3 256) in
  [b0; b1; b2; b3].

Theorem le32_bytes_length :
  forall z, length (le32_bytes z) = 4%nat.
Proof. intros z. reflexivity. Qed.

(** Abstract big-endian integer view of a byte string.
    Endianness is not claimed to match C++ [uint256]. *)
Definition digest_as_Z (bs : list byte) : Z :=
  fold_left (fun (acc : Z) (b : byte) => Z.add (Z.mul acc 256) (Z.of_nat b)) bs 0%Z.
