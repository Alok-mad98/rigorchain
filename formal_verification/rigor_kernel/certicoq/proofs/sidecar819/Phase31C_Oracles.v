(** TASK 3.1A Phase C/D: reduce Selection oracles in isolation. *)
From Coq Require Import List ZArith.
From compcert Require Import Integers Switch.
From compcert Require Selection.
Import ListNotations.

Definition tiny_table : Switch.table := (0%Z, 1%nat) :: nil.

Definition dummy_tree : Switch.comptree := Switch.CTaction 0%nat.

Definition validate_dummy : bool :=
  Switch.validate_switch Int.modulus 0%nat tiny_table dummy_tree.

Timeout 30 Eval vm_compute in validate_dummy.
Timeout 8 Eval vm_compute in
  Selection.compile_switch Int.modulus 0%nat tiny_table.
