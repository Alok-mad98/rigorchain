(** RIGOR v0.1 — SignatureHash preimage layout (formalization level 1).

    Byte concatenation matching C++ HashWriter SER_GETHASH used by
    [SignatureHash] / [ReconstructSighashPreimage]:

      tag(8) || chain_id(32) || nVersion(LE32) || hashPrevouts(32) ||
      hashSequence(32) || hashOutputs(32) || prevout.hash(32) ||
      prevout.n(LE32) || compact_size(script) || script ||
      amount(LE64) || nSequence(LE32) || nLockTime(LE32)

    Digest is [hash256_bytes] of that concatenation. Not ∀-tx VST of
    SignatureHash. No [Axiom] / [Admitted]. *)
From Coq Require Import List.
From Coq Require Import Arith.PeanoNat.
From Coq Require Import ZArith.ZArith.
Import ListNotations.
From Rigor Require Import RigorSha256.
From Rigor Require Import RigorHash.

Local Open Scope Z_scope.

Definition le8 (n : Z) : byte := Z.to_nat (n mod 256).

Fixpoint le_bytes (n : Z) (count : nat) : list byte :=
  match count with
  | O => nil
  | S c => le8 n :: le_bytes (n / 256) c
  end.

Definition le32 (n : Z) : list byte := le_bytes n 4%nat.
Definition le64 (n : Z) : list byte := le_bytes n 8%nat.

(** Bitcoin compact size used to serialize CScript as a byte vector. *)
Definition compact_size (n : nat) : list byte :=
  if Nat.ltb n 253%nat then [n]
  else if Nat.leb n 65535%nat then
    253%nat :: le_bytes (Z.of_nat n) 2%nat
  else if Nat.leb n 4294967295%nat then
    254%nat :: le_bytes (Z.of_nat n) 4%nat
  else
    255%nat :: le_bytes (Z.of_nat n) 8%nat.

Definition serialize_script (spk : list byte) : list byte :=
  compact_size (length spk) ++ spk.

Record SighashFields := {
  sh_tag : list byte;
  sh_chain_id : list byte;
  sh_version : Z;
  sh_hash_prevouts : list byte;
  sh_hash_sequence : list byte;
  sh_hash_outputs : list byte;
  sh_prevout_hash : list byte;
  sh_prevout_n : Z;
  sh_script : list byte;
  sh_amount : Z;
  sh_sequence : Z;
  sh_locktime : Z
}.

Definition sighash_preimage (f : SighashFields) : list byte :=
  sh_tag f ++
  sh_chain_id f ++
  le32 (sh_version f) ++
  sh_hash_prevouts f ++
  sh_hash_sequence f ++
  sh_hash_outputs f ++
  sh_prevout_hash f ++
  le32 (sh_prevout_n f) ++
  serialize_script (sh_script f) ++
  le64 (sh_amount f) ++
  le32 (sh_sequence f) ++
  le32 (sh_locktime f).

Definition sighash_digest (f : SighashFields) : list byte :=
  hash256_bytes (sighash_preimage f).

Theorem sighash_digest_is_sha256d :
  forall f, sighash_digest f = sha256_bytes (sha256_bytes (sighash_preimage f)).
Proof. intros. reflexivity. Qed.

Theorem compact_size_small :
  forall n, Nat.ltb n 253%nat = true -> compact_size n = [n].
Proof. intros n H. unfold compact_size. rewrite H. reflexivity. Qed.

Theorem le32_length : forall n, length (le32 n) = 4%nat.
Proof. intros. reflexivity. Qed.

Theorem le64_length : forall n, length (le64 n) = 8%nat.
Proof. intros. reflexivity. Qed.

Definition tag_oru : list byte := [82; 71; 82; 79; 82; 85; 48; 49]%nat. (* RGRORU01 *)
Definition tag_res : list byte := [82; 71; 82; 82; 69; 83; 48; 49]%nat. (* RGRRES01 *)
Definition tag_mint : list byte := [82; 71; 82; 77; 78; 84; 48; 49]%nat. (* RGRMNT01 *)

Theorem tag_oru_is_rgroru01 :
  tag_oru = [82; 71; 82; 79; 82; 85; 48; 49]%nat.
Proof. reflexivity. Qed.
