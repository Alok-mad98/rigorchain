(** Every executable validator has a distinct extraction entry.

    The previous theorem here was [extraction_interface_total]:

      forall f, exists e, kernel_call_boundary f = e /\ boundary_aligned f

    The existential is witnessed by [kernel_call_boundary f] itself and
    closed by [reflexivity]. Every total function satisfies it, and all
    Coq functions are total, so it said nothing about this interface.

    The property that actually matters at an extraction boundary is
    that the mapping is a BIJECTION:

      - injective: two different Coq validators cannot share one
        extracted entry, so no validator can be silently substituted
        for another;
      - surjective: no declared extracted entry is unreachable from
        the Coq side, so there is no extracted name without a proof
        obligation behind it.

    Neither direction is automatic — [collapsing_boundary] below is a
    well-typed mapping that fails both.

    NOT CLAIMED: OCaml extraction correctness, native binary, CompCert.
*)
From RigorKernel Require Import ExtractionBoundary.ExtractionInterface.

(** * The interface mapping is injective *)

Theorem extraction_interface_injective :
  forall f g : CoqKernelFunction,
    kernel_call_boundary f = kernel_call_boundary g -> f = g.
Proof.
  intros f g H.
  destruct f; destruct g; simpl in H;
    try reflexivity; try discriminate.
Qed.

(** * Every extracted entry is reachable *)

Theorem extraction_interface_surjective :
  forall e : ExtractedKernelInterface,
    exists f : CoqKernelFunction, kernel_call_boundary f = e.
Proof.
  intros e. destruct e.
  - exists CK_UTXO. reflexivity.
  - exists CK_ORU. reflexivity.
  - exists CK_RES. reflexivity.
  - exists CK_Script. reflexivity.
  - exists CK_PoW. reflexivity.
  - exists CK_ASERT. reflexivity.
Qed.

(** * Every entry sits where the interface declares it *)

Theorem extraction_boundary_aligned :
  forall f : CoqKernelFunction, boundary_aligned f.
Proof.
  intros f. destruct f; unfold boundary_aligned, kernel_call_boundary;
    reflexivity.
Qed.

(** * Bundle

    Replaces [extraction_interface_total]. Same file, same mutation
    target (M37 retargets [CK_UTXO] to [EK_ORU], which breaks
    injectivity and surjectivity at once), but the statement now has
    content. *)

Theorem extraction_interface_bijective :
  (forall f g : CoqKernelFunction,
      kernel_call_boundary f = kernel_call_boundary g -> f = g) /\
  (forall e : ExtractedKernelInterface,
      exists f : CoqKernelFunction, kernel_call_boundary f = e) /\
  (forall f : CoqKernelFunction, boundary_aligned f).
Proof.
  split; [exact extraction_interface_injective |].
  split; [exact extraction_interface_surjective |].
  exact extraction_boundary_aligned.
Qed.

(** * Injectivity is a constraint, not a tautology

    A well-typed boundary mapping that collapses every validator onto
    one extracted entry. It type-checks; it is not injective; and it is
    not surjective. This is the witness that the theorems above are
    about [kernel_call_boundary] and not about Coq. *)

Definition collapsing_boundary (f : CoqKernelFunction)
  : ExtractedKernelInterface := EK_UTXO.

Theorem collapsing_boundary_is_not_injective :
  exists f g : CoqKernelFunction,
    f <> g /\ collapsing_boundary f = collapsing_boundary g.
Proof.
  exists CK_UTXO, CK_ORU.
  split; [discriminate | reflexivity].
Qed.

Theorem collapsing_boundary_is_not_surjective :
  ~ (forall e : ExtractedKernelInterface,
        exists f : CoqKernelFunction, collapsing_boundary f = e).
Proof.
  intros Hsurj.
  destruct (Hsurj EK_ORU) as [f Hf].
  unfold collapsing_boundary in Hf. discriminate.
Qed.
