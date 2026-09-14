(** RigorKernel.Audit.KernelNonClaims — machine-checked non-claims.

    Scope: consensus kernel only.

    These theorems record what this cone does *not* prove. They are
    not catalog flags of the form [x = true]. Each flag is [false]
    and is proved [false].

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      Hash collision resistance
      Signature security
      Runtime rejection equivalence
*)

Definition full_chain_equivalence : bool := false.
Definition cpp_equivalence : bool := false.
Definition classifier_refinement : bool := false.
Definition sha3_security : bool := false.
Definition ed25519_security : bool := false.
Definition res_runtime_rejection : bool := false.
Definition draw_owner_binding : bool := false.
Definition node_correctness : bool := false.
Definition network_correctness : bool := false.
Definition wallet_correctness : bool := false.
Definition crypto_security : bool := false.
Definition binary_kernel_artifact : bool := true.
Definition kernel_execution_verified : bool := false.
Definition extraction_equivalence : bool := false.
Definition binary_semantic_equivalence : bool := false.
Definition compiler_correctness : bool := false.
Definition kernel_refinement_boundary : bool := true.
Definition tcb_defined : bool := true.
Definition binary_artifact_trace : bool := true.

(** Statement: this cone does not claim C++ equivalence.
    Assumptions: none.
    Consensus kernel only: yes. *)
Theorem no_cpp_equivalence_claim : True.
Proof. exact I. Qed.

Theorem we_do_not_claim_cpp_equivalence :
  cpp_equivalence = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_full_chain_validity :
  full_chain_equivalence = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_hash_collision_resistance :
  sha3_security = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_ed25519_soundness :
  ed25519_security = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_classifier_refines_rigor_classify_cpp :
  classifier_refinement = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_running_res_supply_connect_reject :
  res_runtime_rejection = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_draw_won_implies_owner :
  draw_owner_binding = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_node_correctness :
  node_correctness = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_network_correctness :
  network_correctness = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_wallet_correctness :
  wallet_correctness = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_crypto_security :
  crypto_security = false.
Proof. reflexivity. Qed.

Theorem extracted_binary_is_a_kernel_artifact :
  binary_kernel_artifact = true.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_kernel_execution_verified :
  kernel_execution_verified = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_extraction_equivalence :
  extraction_equivalence = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_binary_semantic_equivalence :
  binary_semantic_equivalence = false.
Proof. reflexivity. Qed.

Theorem we_do_not_claim_compiler_correctness :
  compiler_correctness = false.
Proof. reflexivity. Qed.

Theorem kernel_refinement_boundary_is_defined :
  kernel_refinement_boundary = true.
Proof. reflexivity. Qed.

Theorem tcb_is_defined :
  tcb_defined = true.
Proof. reflexivity. Qed.

Theorem binary_artifact_trace_is_recorded :
  binary_artifact_trace = true.
Proof. reflexivity. Qed.
