(** Phase 6: CertiCoq compile of the Observe-literal verdicts.

    Sidecar Coq 8.19.2 + CertiCoq 0.9+8.19. Not in the 8.15
    surface [ _CoqProject ].

    The records are the same Gallina literals as [Observe.v]
    (INVALID-001…005, [invalid_script], six smokes). The C
    driver is trusted print/exit glue; it does not re-encode
    boxed [Z].

    NOT CLAIMED:
      compiler correctness
      binary semantic equivalence
      C++ / ConnectBlock / rigord
*)
From CertiCoq.Plugin Require Import CertiCoq.
From RigorSurface Require Import Observe.
From RigorSurface Require Import Kernel.UTXO.
From RigorSurface Require Import Kernel.ORU.
From RigorSurface Require Import Kernel.RES.
From RigorSurface Require Import Kernel.Script.
From RigorSurface Require Import Kernel.Pow.
From RigorSurface Require Import Kernel.ASERT.

Record phase6_result := {
  r_inv001 : bool;
  r_inv002 : bool;
  r_inv003 : bool;
  r_inv004 : bool;
  r_inv005 : bool;
  r_inv_script : bool;
  r_smoke_utxo : bool;
  r_smoke_oru : bool;
  r_smoke_res : bool;
  r_smoke_script : bool;
  r_smoke_pow : bool;
  r_smoke_asert : bool
}.

Definition phase6_run : phase6_result :=
  {| r_inv001 := check_utxo invalid001_tx;
     r_inv002 := check_oru invalid002_tx;
     r_inv003 := check_res invalid003_tx;
     r_inv004 := check_asert invalid004_in;
     r_inv005 := check_pow invalid005_in;
     r_inv_script := check_script invalid_script;
     r_smoke_utxo := check_utxo smoke_utxo;
     r_smoke_oru := check_oru smoke_oru;
     r_smoke_res := check_res smoke_res;
     r_smoke_script := check_script smoke_script;
     r_smoke_pow := check_pow smoke_pow;
     r_smoke_asert := check_asert smoke_asert |}.

CertiCoq Compile -O 0 phase6_run.
