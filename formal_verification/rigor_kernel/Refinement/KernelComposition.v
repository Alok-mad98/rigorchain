(** Kernel-step refinement composition.

    Abstract consensus transition and a model-level refined step
    agree on UTXO / ORU / RES / Script / PoW flags and pools.

    NOT CLAIMED: node transition, C++ ConnectBlock, CompCert.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import Lia.

Local Open Scope Z_scope.

Record KernelAbs := {
  ka_utxo_ok : bool;
  ka_oru : Z;
  ka_res : Z;
  ka_script_ok : bool;
  ka_pow_ok : bool
}.

Record KernelConc := {
  kc_utxo_ok : bool;
  kc_oru : Z;
  kc_res : Z;
  kc_script_ok : bool;
  kc_pow_ok : bool
}.

Record KernelStep := {
  ks_utxo : bool;
  ks_oru_delta : Z;
  ks_res_delta : Z;
  ks_script : bool;
  ks_pow : bool
}.

Definition kernel_refines (c : KernelConc) (a : KernelAbs) : Prop :=
  kc_utxo_ok c = ka_utxo_ok a /\
  kc_oru c = ka_oru a /\
  kc_res c = ka_res a /\
  kc_script_ok c = ka_script_ok a /\
  kc_pow_ok c = ka_pow_ok a.

Definition apply_kernel_abs (a : KernelAbs) (s : KernelStep) : KernelAbs :=
  {| ka_utxo_ok := andb (ka_utxo_ok a) (ks_utxo s);
     ka_oru := ka_oru a + ks_oru_delta s;
     ka_res := ka_res a + ks_res_delta s;
     ka_script_ok := andb (ka_script_ok a) (ks_script s);
     ka_pow_ok := andb (ka_pow_ok a) (ks_pow s) |}.

Definition apply_kernel_conc (c : KernelConc) (s : KernelStep) : KernelConc :=
  {| kc_utxo_ok := andb (kc_utxo_ok c) (ks_utxo s);
     kc_oru := kc_oru c + ks_oru_delta s;
     kc_res := kc_res c + ks_res_delta s;
     kc_script_ok := andb (kc_script_ok c) (ks_script s);
     kc_pow_ok := andb (kc_pow_ok c) (ks_pow s) |}.

Theorem kernel_step_refinement_composes :
  forall (c : KernelConc) (a : KernelAbs) (s : KernelStep),
    kernel_refines c a ->
    kernel_refines (apply_kernel_conc c s) (apply_kernel_abs a s).
Proof.
  intros [cu co cr cs cp] [au ao ar as_ ap] [su so sr ss sp].
  unfold kernel_refines, apply_kernel_conc, apply_kernel_abs. simpl.
  intros [Hu [Ho [Hr [Hs Hp]]]].
  subst. repeat split; reflexivity.
Qed.
