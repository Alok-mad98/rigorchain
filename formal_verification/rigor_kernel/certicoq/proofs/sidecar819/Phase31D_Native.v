From Coq Require Import List ZArith.
From compcert Require Import Integers Switch.
From compcert Require Selection.
Import ListNotations.

Definition tiny_table : Switch.table := (0%Z, 1%nat) :: nil.

Timeout 15 Eval native_compute in (1 + 1)%nat.
Timeout 15 Eval native_compute in
  Selection.compile_switch Int.modulus 0%nat tiny_table.
