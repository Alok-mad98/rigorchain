(** CanonicalHashKernel.v — FIPS 180-4 lock for the executable SHA-256 kernel.

    RigorSha256 is the Coq implementation. This module pins NIST / FIPS test
    vectors and the SHA-256d composition lemma so a kernel regression fails the
    build. It does not identify Coq, C++, Python, and extracted OCaml as one
    binary, and it is not a refinement proof against CHash256 / CSHA256.

    C++: core/src/crypto/sha256.h (CSHA256) and core/src/hash.h (CHash256).
    A portable C++ CLI over sha256.cpp is compared to this kernel in
    formal_verification/tests/test_hash_kernel_tripartite.py. That check is
    differential, not a shared binary.
    Python: hashlib.sha256 / spec.sha256d
    Extracted OCaml: formal_verification/extracted/ (optional; see ExtractHashKernel.v)
*)

From Coq Require Import Arith.Arith.
From Coq Require Import List.
Import ListNotations.

From Rigor Require Import RigorSha256.
From Rigor Require Import RigorHash.

(** * Composition: Bitcoin Hash256 is two SHA-256 compressions *)

Theorem hash256_bytes_is_double_sha256 :
  forall m, hash256_bytes m = sha256_bytes (sha256_bytes m).
Proof. reflexivity. Qed.

Theorem sha256_bytes_digest_length :
  forall m, length (sha256_bytes m) = 32%nat.
Proof.
  intros m.
  unfold sha256_bytes.
  apply words_to_bytes32_len8.
  apply blocks_len.
  reflexivity.
Qed.

Theorem hash256_bytes_digest_length :
  forall m, length (hash256_bytes m) = 32%nat.
Proof. apply hash256_bytes_length. Qed.

(** * FIPS 180-4 / NIST CAVP short messages

    Empty and "abc" are the classic one-block vectors. 55 / 56 / 63 / 64 / 65-byte
    messages sit on the padding boundary (one vs two blocks). The 56-byte
    alphabetic string is the published two-block NIST example. *)

Definition fips_empty : list byte := [].
Definition fips_abc : list byte := [97; 98; 99]%nat.
Definition fips_55a : list byte := repeat 97%nat 55.
Definition fips_56a : list byte := repeat 97%nat 56.
Definition fips_64z : list byte := repeat 0%nat 64.
Definition fips_63a : list byte := repeat 97%nat 63.
Definition fips_65a : list byte := repeat 97%nat 65.
Definition fips_nist56 : list byte :=
  [97; 98; 99; 100; 98; 99; 100; 101; 99; 100; 101; 102; 100; 101; 102; 103;
   101; 102; 103; 104; 102; 103; 104; 105; 103; 104; 105; 106; 104; 105; 106; 107;
   105; 106; 107; 108; 106; 107; 108; 109; 107; 108; 109; 110; 108; 109; 110; 111;
   109; 110; 111; 112; 110; 111; 112; 113]%nat.

Definition fips_empty_sha256 : list byte :=
  [227; 176; 196; 66; 152; 252; 28; 20; 154; 251; 244; 200; 153; 111; 185; 36;
   39; 174; 65; 228; 100; 155; 147; 76; 164; 149; 153; 27; 120; 82; 184; 85]%nat.
Definition fips_abc_sha256 : list byte :=
  [186; 120; 22; 191; 143; 1; 207; 234; 65; 65; 64; 222; 93; 174; 34; 35;
   176; 3; 97; 163; 150; 23; 122; 156; 180; 16; 255; 97; 242; 0; 21; 173]%nat.
Definition fips_55a_sha256 : list byte :=
  [159; 67; 144; 248; 211; 12; 45; 217; 46; 201; 240; 149; 182; 94; 43; 154;
   233; 176; 169; 37; 165; 37; 142; 36; 28; 159; 30; 145; 15; 115; 67; 24]%nat.
Definition fips_56a_sha256 : list byte :=
  [179; 84; 57; 164; 172; 111; 9; 72; 182; 214; 249; 227; 198; 175; 15; 95;
   89; 12; 226; 15; 27; 222; 112; 144; 239; 121; 112; 104; 110; 198; 115; 138]%nat.
Definition fips_64z_sha256 : list byte :=
  [245; 165; 253; 66; 209; 106; 32; 48; 39; 152; 239; 110; 211; 9; 151; 155;
   67; 0; 61; 35; 32; 217; 240; 232; 234; 152; 49; 169; 39; 89; 251; 75]%nat.
Definition fips_63a_sha256 : list byte :=
  [125; 62; 116; 160; 93; 125; 177; 91; 206; 74; 217; 236; 6; 88; 234; 152;
   227; 240; 110; 238; 207; 22; 180; 198; 255; 242; 218; 69; 125; 220; 47; 52]%nat.
Definition fips_65a_sha256 : list byte :=
  [99; 83; 97; 196; 139; 185; 234; 177; 65; 152; 231; 110; 168; 171; 127; 26;
   65; 104; 93; 106; 214; 42; 169; 20; 109; 48; 29; 79; 23; 235; 10; 224]%nat.
Definition fips_nist56_sha256 : list byte :=
  [36; 141; 106; 97; 210; 6; 56; 184; 229; 192; 38; 147; 12; 62; 96; 57;
   163; 60; 228; 89; 100; 255; 33; 103; 246; 236; 237; 212; 25; 219; 6; 193]%nat.

Definition fips_empty_sha256d : list byte :=
  [93; 246; 224; 226; 118; 19; 89; 211; 10; 130; 117; 5; 142; 41; 159; 204;
   3; 129; 83; 69; 69; 245; 92; 244; 62; 65; 152; 63; 93; 76; 148; 86]%nat.
Definition fips_abc_sha256d : list byte :=
  [79; 139; 66; 194; 45; 211; 114; 155; 81; 155; 166; 246; 141; 45; 167; 204;
   91; 45; 96; 109; 5; 218; 237; 90; 213; 18; 140; 192; 62; 108; 99; 88]%nat.

Lemma sha256_fips_empty : sha256_bytes fips_empty = fips_empty_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_abc : sha256_bytes fips_abc = fips_abc_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_55a : sha256_bytes fips_55a = fips_55a_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_56a : sha256_bytes fips_56a = fips_56a_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_64z : sha256_bytes fips_64z = fips_64z_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_63a : sha256_bytes fips_63a = fips_63a_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_65a : sha256_bytes fips_65a = fips_65a_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma sha256_fips_nist56 : sha256_bytes fips_nist56 = fips_nist56_sha256.
Proof. vm_compute; reflexivity. Qed.

Lemma hash256_fips_empty : hash256_bytes fips_empty = fips_empty_sha256d.
Proof. vm_compute; reflexivity. Qed.

Lemma hash256_fips_abc : hash256_bytes fips_abc = fips_abc_sha256d.
Proof. vm_compute; reflexivity. Qed.

Lemma fips_empty_sha256_len : length fips_empty_sha256 = 32%nat.
Proof. reflexivity. Qed.

Lemma fips_abc_sha256_len : length fips_abc_sha256 = 32%nat.
Proof. reflexivity. Qed.
