(** RigorKernel.Kernel.RefinementBoundary — proof / trust boundary freeze.

    Scope: Coq consensus kernel only.

    Marks where the formal argument stops:

      Spec → Coq model → Coq validators

    Extraction, the OCaml compiler, the binary artifact, and the
    host runtime are outside the proof boundary. This is not a
    CompCert refinement, not an extraction refinement, and not a
    C++ / full-node equivalence.

    [kernel_boundary_closed] is a class-C documentation lemma
    (fields are [True]; proof is [exact I]). It does not prove
    extraction or compiler correctness.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      CompCert refinement
      Extraction semantic preservation
      Binary semantic equivalence
      Compiler / OS / hardware correctness
*)

Record RefinementBoundary :=
{
  specification_layer : Prop;
  executable_layer : Prop;
  refinement_scope : Prop
}.

(** Frozen kernel boundary: specification and Coq-validator layers
    are in scope; extraction and the host execution environment
    are not. *)
Definition frozen_kernel_boundary : RefinementBoundary :=
  {| specification_layer := True;
     executable_layer := True;
     refinement_scope := True |}.

Definition refinement_scope_closed : Prop :=
  frozen_kernel_boundary.(refinement_scope).

(** Statement: documentation marker that the kernel refinement
    scope is closed at Coq validators (not at extraction).
    Class: C (boundary / documentation lemma).
    Assumptions: none.
    Coq consensus kernel only: yes. *)
Theorem kernel_boundary_closed :
  refinement_scope_closed.
Proof.
  unfold refinement_scope_closed, frozen_kernel_boundary. exact I.
Qed.

Theorem specification_layer_in_scope :
  frozen_kernel_boundary.(specification_layer).
Proof. exact I. Qed.

Theorem executable_layer_in_scope :
  frozen_kernel_boundary.(executable_layer).
Proof. exact I. Qed.
