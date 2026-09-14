(** Phase4FConsensus/RuntimeEvidenceCatalog.v
    Frozen runtime evidence catalog (PHASE-4F-001G).

    Imports ONLY frozen evidence as constants. No execution, no RPC, no node
    access. Values are transcribed from the permanent runtime certificates:

      TASK-003 : docs/runtime/res-live-mint-003/FIRST_RES_BLOCK_CERTIFICATE.md
      TASK-004 : docs/runtime/res-soak-ready-004/RES_MINT_LOOP.md (50/50 accepted)
      TASK-005 : docs/runtime/res-multinode-consensus-005/
                   {RES_CONSENSUS_READY_VERDICT, RES_REORG_VALIDATION}.md

    Hashes/txids are the 256-bit big-endian integer values of the hex strings. *)

From Coq Require Import List ZArith.
Import ListNotations.
Require Import Rigor.Phase4FConsensus.RuntimeEvidenceBridge.

(** TASK-003: first accepted closed-testnet RES body-mint block (height 1500). *)
Definition ev_003_mint : RuntimeEvidenceRecord :=
  {| rer_task_id := 3;
     rer_kind := REK_ResMintAccepted;
     rer_block_hash := 38077415480994337617352834439253557395691338191600224285949151229929911611599;
     rer_height := 1500;
     rer_winner_txid := 97362155569800707569861957093549671454800671317442962058375368396301870369188;
     rer_status := RST_Accepted |}.

Definition ev_003_winner : RuntimeEvidenceRecord :=
  {| rer_task_id := 3;
     rer_kind := REK_DrawWinner;
     rer_block_hash := 38077415480994337617352834439253557395691338191600224285949151229929911611599;
     rer_height := 1500;
     rer_winner_txid := 97362155569800707569861957093549671454800671317442962058375368396301870369188;
     rer_status := RST_Accepted |}.

(** TASK-004: soak loop, 50/50 accepted RES mints (aggregate; first slot 1501).
    No single block hash is certified for the aggregate, so block_hash = 0. *)
Definition ev_004_soak : RuntimeEvidenceRecord :=
  {| rer_task_id := 4;
     rer_kind := REK_ResMintAccepted;
     rer_block_hash := 0;
     rer_height := 1501;
     rer_winner_txid := 0;
     rer_status := RST_Accepted |}.

(** TASK-005: multinode connect-block acceptance (sample block, height 1641). *)
Definition ev_005_connect : RuntimeEvidenceRecord :=
  {| rer_task_id := 5;
     rer_kind := REK_ConnectBlockAccepted;
     rer_block_hash := 17737404885623055494212933369350255889795214441078337394485326227620790817766;
     rer_height := 1641;
     rer_winner_txid := 0;
     rer_status := RST_Accepted |}.

(** TASK-005: reorg / replay validation PASS (fork height 1638). *)
Definition ev_005_reorg : RuntimeEvidenceRecord :=
  {| rer_task_id := 5;
     rer_kind := REK_ReorgReplay;
     rer_block_hash := 94028617866244248987177472443274938029931759632474561231884493563722918121864;
     rer_height := 1638;
     rer_winner_txid := 0;
     rer_status := RST_Pass |}.

(** TASK-005: multinode consensus closure PASS (tip height 1643). *)
Definition ev_005_multinode : RuntimeEvidenceRecord :=
  {| rer_task_id := 5;
     rer_kind := REK_MultinodeConsensus;
     rer_block_hash := 73330333463328158884727197169637247595780103269890288719550616950941324866293;
     rer_height := 1643;
     rer_winner_txid := 0;
     rer_status := RST_Pass |}.

(** REQUIRED-2: typed frozen evidence catalog (TASK-003/004/005). *)
Definition runtime_evidence_catalog : list RuntimeEvidenceRecord :=
  [ ev_003_mint; ev_003_winner; ev_004_soak;
    ev_005_connect; ev_005_reorg; ev_005_multinode ].

Definition runtime_evidence_count : nat := length runtime_evidence_catalog.

Lemma runtime_evidence_count_value : runtime_evidence_count = 6%nat.
Proof. reflexivity. Qed.

(** REQUIRED-2: catalog is non-empty. *)
Lemma runtime_catalog_nonempty : runtime_evidence_count <> 0%nat.
Proof. unfold runtime_evidence_count. simpl. discriminate. Qed.
