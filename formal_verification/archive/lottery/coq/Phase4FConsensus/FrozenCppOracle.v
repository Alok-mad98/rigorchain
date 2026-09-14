(** Phase4FConsensus/FrozenCppOracle.v
    Frozen C++ Oracle specification layer (PHASE-4F-001J).

    SPECIFICATION ONLY. Types the "frozen C++ consensus output" side of the
    Oracle Bridge so frozen observations can later be compared. NO runtime
    execution, NO C++ compilation, NO oracle replay, NO equivalence claim. No
    consensus / src / validation.cpp / ConnectBlock / mining / RPC / wallet /
    SHA256 / ConvergenceRoot / RFC / runtime-harness code is modified. *)

From Coq Require Import ZArith.

(** Components a frozen C++ output can speak to. *)
Inductive FrozenCppComponent :=
| FCC_Draw
| FCC_Replay
| FCC_Reorg
| FCC_Supply
| FCC_ConnectBlock.

(** A single frozen C++ observation. [fco_payload] is the 256-bit big-endian
    integer value of the frozen output (hash / txid), or 0 when absent. *)
Record FrozenCppObservation := {
  fco_component : FrozenCppComponent;
  fco_present : bool;
  fco_payload : Z;
}.

(** Presence status of a frozen C++ observation. *)
Inductive FrozenCppStatus :=
| FCS_Present
| FCS_Missing.

Definition frozen_cpp_status (o : FrozenCppObservation) : FrozenCppStatus :=
  if o.(fco_present) then FCS_Present else FCS_Missing.

Lemma present_status_not_missing : FCS_Present <> FCS_Missing.
Proof. discriminate. Qed.
