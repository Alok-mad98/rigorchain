(** RigorKernel.Kernel.KernelSoundness — executable → invariant bridge.

    Scope: consensus kernel only.

    validate_kernel input = true  →  consensus_invariant input

    Each branch cites an existing P0-108 theorem. No new rule.

    NOT CLAIMED:
      C++ implementation equivalence
      Full chain validity
      CompCert / implementation refinement
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lists.List.
From RigorKernel.proofs.UTXO_Soundness Require Import UTXOSoundness.
From RigorKernel.proofs.ORU_Conservation Require Import ORUConservation.
From RigorKernel.proofs.RES_Issuance Require Import RESIssuance.
From RigorKernel.proofs.Script_Correctness Require Import ScriptCorrectness.
From RigorKernel Require Import Supply.SupplySchedule.
From RigorKernel Require Import Script.ScriptClassifier.
From RigorKernel Require Import Pow.CompactBits.
From RigorKernel Require Import Pow.ASERTModel.
From RigorKernel Require Import Kernel.UTXOValidator.
From RigorKernel Require Import Kernel.ORUValidator.
From RigorKernel Require Import Kernel.RESValidator.
From RigorKernel Require Import Kernel.ScriptValidator.
From RigorKernel Require Import Kernel.PowValidator.
From RigorKernel Require Import Kernel.ASERTValidator.
From RigorKernel Require Import Kernel.KernelInterface.

Local Open Scope Z_scope.

Inductive KernelValidationRequest :=
| KI_UTXO : UTXOState -> KernelValidationRequest
| KI_ORU : ORUState -> KernelValidationRequest
| KI_RES : RESState -> KernelValidationRequest
| KI_Script : ScriptState -> KernelValidationRequest
| KI_Pow : PowHeader -> KernelValidationRequest
| KI_ASERT : ASERTInput -> KernelValidationRequest.

Definition validate_kernel (input : KernelValidationRequest) : bool :=
  match input with
  | KI_UTXO s => UTXOValidator.check_utxo s
  | KI_ORU s => ORUValidator.check_oru s
  | KI_RES s => RESValidator.check_res s
  | KI_Script s => ScriptValidator.check_script s
  | KI_Pow h => PowValidator.check_pow h
  | KI_ASERT i => ASERTValidator.check_asert i
  end.

Definition consensus_invariant (input : KernelValidationRequest) : Prop :=
  match input with
  | KI_UTXO s =>
      UTXOSoundness.valid_tx s
  | KI_ORU s =>
      ORUConservation.valid_tx s
      /\ oru_input s = oru_output s + fee s
      /\ new_supply s = old_supply s + subsidy s + fees s - burn s
  | KI_RES s =>
      res_created s
      /\ mint_amount s * res_destroy_pole_sq (rt_height s)
           <= Z.shiftl (burn_oru s) RES_DESTROY_SHIFT
  | KI_Script s =>
      classify_script (ss_script s) = ss_expected s
      /\ In (classify (ss_script s)) reachable_assets
      /\ (shape_hits (ss_script s) <= 1)%nat
  | KI_Pow h =>
      0 < pow_target h
      /\ pow_digest_z h <= pow_target h
      /\ compact_decode (compact_encode (pow_compact_t h))
           <= pow_compact_t h
  | KI_ASERT i =>
      asert_te i < 0
      /\ 0 < asert_tau i
      /\ exp_q_raw (asert_te i) (asert_tau i) = asert_expected_q i
      /\ exp_q_raw (asert_te i) (asert_tau i)
           = Z.quot (asert_te i * ASERT_RADIX) (asert_tau i)
  end.

(** Statement: a passing executable check implies the kernel
    invariant of that domain.
    Assumptions: none (Closed).
    Consensus kernel only: yes. *)
Theorem kernel_sound :
  forall input,
    validate_kernel input = true ->
    consensus_invariant input.
Proof.
  intros input H. destruct input as [s|s|s|s|h|i]; simpl in *.
  - apply check_utxo_valid. exact H.
  - pose proof (check_oru_valid s H) as Hv.
    pose proof (check_oru_sound s H) as Hs.
    tauto.
  - pose proof H as Hchk.
    apply check_res_reflect in H.
    apply check_res_sound in Hchk.
    destruct Hchk as [_ [_ Hrate]].
    split; [exact H | exact Hrate].
  - apply check_script_sound. exact H.
  - apply check_pow_sound in H. tauto.
  - apply check_asert_sound in H. tauto.
Qed.
