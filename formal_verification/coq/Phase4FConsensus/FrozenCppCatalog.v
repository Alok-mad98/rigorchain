(** Phase4FConsensus/FrozenCppCatalog.v
    Frozen C++ observation catalog (PHASE-4F-001J).

    Immutable constants imported ONLY from already-frozen evidence
    (TASK-003/004/005, PHASE-4A/4B/4D/4E). No new measurements, no runtime calls,
    no JSON parsing, no external scripts. Payloads are the 256-bit integer values
    of frozen hashes/txids produced by the frozen C++ node. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.FrozenCppOracle.

(** FCC_Draw: winner txid of the first accepted RES block (TASK-003). *)
Definition fco_draw : FrozenCppObservation :=
  {| fco_component := FCC_Draw;
     fco_present := true;
     fco_payload := 97362155569800707569861957093549671454800671317442962058375368396301870369188 |}.

(** FCC_ConnectBlock: accepted multinode sample block hash (TASK-005 / PHASE-4E). *)
Definition fco_connectblock : FrozenCppObservation :=
  {| fco_component := FCC_ConnectBlock;
     fco_present := true;
     fco_payload := 17737404885623055494212933369350255889795214441078337394485326227620790817766 |}.

(** FCC_Reorg: fork block hash from reorg validation (TASK-005 / PHASE-4B). *)
Definition fco_reorg : FrozenCppObservation :=
  {| fco_component := FCC_Reorg;
     fco_present := true;
     fco_payload := 94028617866244248987177472443274938029931759632474561231884493563722918121864 |}.

(** FCC_Replay: multinode consensus tip hash (TASK-005 / PHASE-4B). *)
Definition fco_replay : FrozenCppObservation :=
  {| fco_component := FCC_Replay;
     fco_present := true;
     fco_payload := 73330333463328158884727197169637247595780103269890288719550616950941324866293 |}.

(** FCC_Supply: no frozen C++ supply OUTPUT is imported (PHASE-4D supply totals
    were not captured as a single frozen C++ payload). Marked absent — honest. *)
Definition fco_supply : FrozenCppObservation :=
  {| fco_component := FCC_Supply;
     fco_present := false;
     fco_payload := 0 |}.

(** Immutable frozen C++ catalog. *)
Definition frozen_cpp_catalog : list FrozenCppObservation :=
  [ fco_draw; fco_connectblock; fco_reorg; fco_replay; fco_supply ].
