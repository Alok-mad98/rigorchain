(** ReplayReorgCore/ReplayState.v
    PHASE-4B structural blockchain state (TASK-FORMAL-PHASE4B-REPLAY-REORG-CORE-001).

    Baseline: c05f1a94 / mainnet-freeze-rfc015-2026-05-26 (frozen kernel unchanged).

    Abstraction contract:
    - NO hash execution, NO SHA-256, NO convergence draw.
    - Block and UTXO shapes only; consensus bytes live in future refinement layers.
*)
From Coq Require Import ZArith.ZArith.
From Coq Require Import List.
From Coq Require Import Arith.
From Coq Require Import Bool.
Import ListNotations.
Require Import Rigor.ReplayReorgCore.Hash32Order.

(** 32-byte digest as byte list (RFC-009 wire order; no hash function here). *)
Definition Hash32 := Rigor.ReplayReorgCore.Hash32Order.Hash32.
Definition hash32_eq_dec := Rigor.ReplayReorgCore.Hash32Order.hash32_eq_dec.
Definition hash32_zero := Rigor.ReplayReorgCore.Hash32Order.hash32_zero.

(** OutPoint and output record (mirrors CTxOut storage: value + scriptPubKey only). *)
Inductive OutPoint : Type :=
| MkOutPoint (op_txid : Hash32) (op_index : nat).

Record TxOut := {
  out_value : Z;
  out_script : list nat  (** opaque script bytes as naturals 0..255 *)
}.

(** Finite UTXO set as association list (deterministic when keys unique). *)
Definition UTXOSet := list (OutPoint * TxOut).

(** Abstract block header (no serialization / encoding). *)
Record BlockHeaderAbs := {
  hdr_height : nat;
  hdr_prev_hash : Hash32;
  hdr_chainwork : Z;   (** cumulative work — RFC PoW fork choice input *)
  hdr_nbits : Z;
  hdr_time : Z
}.

(** Abstract transaction: explicit spends and creates (structural only). *)
Record TxAbs := {
  tx_spends : list OutPoint;
  tx_creates : list TxOut
}.

(** ConnectBlock mint authorization carried on abstract blocks (PHASE-4E). *)
Record ConnectMintSpec := {
  cm_draw_won : bool;      (** draw kernel returned a winner (RES mint gate) *)
  cm_oru_subsidy : Z       (** authorized ORU subsidy for this block (≥ 0) *)
}.

Definition default_connect_mint : ConnectMintSpec :=
  {| cm_draw_won := false; cm_oru_subsidy := 0%Z |}.

(** Abstract block body. *)
Record BlockAbs := {
  blk_header : BlockHeaderAbs;
  blk_txs : list TxAbs;
  blk_mint : ConnectMintSpec
}.

(** Active-chain metadata (authoritative projection subset). *)
Record ChainMeta := {
  cm_height : nat;
  cm_chainwork : Z;
  cm_tip_hash : Hash32
}.

(** Pre-extension snapshot (non-recursive; used for rollback). *)
Record ReplaySnapshot := {
  snap_utxo : UTXOSet;
  snap_chain : ChainMeta;
  snap_oru_supply : Z;
  snap_res_supply : Z
}.

(** Full replay-visible consensus slice at a tip.

    [rs_history] holds pre-extension snapshots (newest first) for rollback.
    No account balances: supplies are scalar projections only. *)
Record ReplayState := {
  rs_utxo : UTXOSet;
  rs_chain : ChainMeta;
  rs_oru_supply : Z;
  rs_res_supply : Z;
  rs_history : list ReplaySnapshot
}.

Definition snapshot_of_state (s : ReplayState) : ReplaySnapshot :=
  {| snap_utxo := rs_utxo s;
     snap_chain := rs_chain s;
     snap_oru_supply := rs_oru_supply s;
     snap_res_supply := rs_res_supply s |}.

Definition state_of_snapshot (snap : ReplaySnapshot) (hist : list ReplaySnapshot)
  : ReplayState :=
  {| rs_utxo := snap_utxo snap;
     rs_chain := snap_chain snap;
     rs_oru_supply := snap_oru_supply snap;
     rs_res_supply := snap_res_supply snap;
     rs_history := hist |}.

Definition replay_set_utxo (s : ReplayState) (u : UTXOSet) : ReplayState :=
  {| rs_utxo := u;
     rs_chain := rs_chain s;
     rs_oru_supply := rs_oru_supply s;
     rs_res_supply := rs_res_supply s;
     rs_history := rs_history s |}.

Definition replay_set_chain (s : ReplayState) (cm : ChainMeta) : ReplayState :=
  {| rs_utxo := rs_utxo s;
     rs_chain := cm;
     rs_oru_supply := rs_oru_supply s;
     rs_res_supply := rs_res_supply s;
     rs_history := rs_history s |}.

Definition empty_utxo : UTXOSet := [].

(** Genesis placeholder tip (32 zero bytes). *)
Definition hash32_placeholder : Hash32 := hash32_zero.

Definition genesis_chain_meta : ChainMeta :=
  {| cm_height := 0;
     cm_chainwork := 0%Z;
     cm_tip_hash := hash32_placeholder |}.

Definition init_replay_state : ReplayState :=
  {| rs_utxo := empty_utxo;
     rs_chain := genesis_chain_meta;
     rs_oru_supply := 0%Z;
     rs_res_supply := 0%Z;
     rs_history := [] |}.

(** State equality is component-wise (replay comparison target). *)
Definition replay_state_eq (s1 s2 : ReplayState) : Prop :=
  rs_utxo s1 = rs_utxo s2 /\
  rs_chain s1 = rs_chain s2 /\
  rs_oru_supply s1 = rs_oru_supply s2 /\
  rs_res_supply s1 = rs_res_supply s2.
