# Null Content Inventory

Theorems whose statement holds of **any** function, and therefore
says nothing about the rule named in it. Detected by
`scripts/check_null_content.py`; see that file for the four shapes.

Frontier 1.4 removed this class from the classifier. These are the
instances elsewhere in the tree. None of them is in Category A, so
the public claim does not rest on any of them, but a statement of
this shape is not a supporting lemma either and should be replaced by
one with content or moved to Category C as a boundary marker.

Rows marked `abstract-section` sit inside a `Section` that declares a
`Variable`, almost always the uninterpreted PoW digest `H`. Over an
abstract function the only available facts *are* the free ones, so
these were an attempt to state something where nothing can be stated.
Their honest form is a Category C boundary marker saying `H` is
uninterpreted, which is what `Audit/KernelNonClaims.v` already does
with `kernel_does_not_assume_sha3_security`.

Generated. Do not edit by hand: CI regenerates and compares the set.
A name leaves this file by being restated with content or deleted. A
new name appearing here fails the gate.


abstract_digest_is_a_function  Audit/KernelAssumptions.v  self-equality (via digest_is_a_function)  category=unclassified  concrete
asert_deterministic  Pow/ASERTModel.v  self-equality  category=unclassified  concrete
check_asert_deterministic  Kernel/ASERTValidator.v  congruence (via deterministic_asert)  category=unclassified  concrete
check_pow_deterministic  Kernel/PowValidator.v  self-equality  category=C  concrete
classify_extensional  Script/ScriptClassifier.v  congruence  category=unclassified  concrete
classify_no_interpreter  proofs/Script_Correctness/ScriptCorrectness.v  self-equality  category=unclassified  concrete
encode_pow_target_deterministic  Kernel/SerializationBoundary.v  self-equality  category=unclassified  concrete
encode_res_schedule_deterministic  Kernel/SerializationBoundary.v  self-equality  category=unclassified  concrete
encode_utxo_deterministic  Kernel/SerializationBoundary.v  self-equality  category=unclassified  concrete
encoding_deterministic  Kernel/SerializationBoundary.v  self-equality  category=C  concrete
hash_deterministic  Pow/AbstractHash.v  self-equality  category=unclassified  abstract-section
hash_extensional  Pow/AbstractHash.v  congruence  category=unclassified  abstract-section
machine_step_deterministic_real  Runtime/MachineState.v  function-ness  category=C  concrete
pow_asert_correctness  proofs/Pow_Correctness/ASERTCorrectness.v  congruence (via deterministic_asert)  category=C  concrete
pow_hash_deterministic  proofs/Pow_Correctness/PowCorrectness.v  congruence  category=C  abstract-section
pow_header_deterministic  Pow/PowModel.v  congruence  category=unclassified  abstract-section
pow_same_header_same_digest_same_target_same_result  proofs/Pow_Correctness/PowCorrectness.v  self-equality  category=unclassified  abstract-section
pow_valid_deterministic  proofs/Pow_Correctness/PowCorrectness.v  self-equality  category=C  abstract-section
same_pow_target_same_encoding  Kernel/SerializationBoundary.v  congruence  category=unclassified  concrete
same_res_schedule_same_encoding  Kernel/SerializationBoundary.v  congruence  category=unclassified  concrete
same_state_same_encoding  Kernel/SerializationBoundary.v  congruence  category=C  concrete
same_utxo_same_encoding  Kernel/SerializationBoundary.v  congruence  category=unclassified  concrete
