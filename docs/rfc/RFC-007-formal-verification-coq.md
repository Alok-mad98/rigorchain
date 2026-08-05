# RFC-007: Formal Verification Spec (Coq)

**Version:** 1.0 (Canonical)
**Goal:** Machine-Proven Consensus Correctness

**This document MUST be interpreted under RFC-000 Constitutional Nomenclature**

---

## I. Directory Structure (must be organized exactly like this)

```
coq/
 ├── Types.v
 ├── Serialization.v
 ├── Hash.v
 ├── State.v
 ├── ConvergeRoot.v
 ├── Sigma.v
 ├── Selection.v
 ├── Transition.v
 ├── Execution.v
 ├── Properties.v
 ├── ArdaConsensus.v
 ├── Proofs/
 │    ├── Determinism.v
 │    ├── Correctness.v
 │    ├── SplitInvariance.v
 │    ├── Fairness.v
 │    └── Safety.v
 └── Extraction.v
```

**Note**: Current implementation includes `ArdaConsensus.v` which is not in RFC-007 specification. This appears to be the main consensus file that aggregates other modules.

---

## II. Core Type Definitions (Types.v)

```coq
Require Import Coq.ZArith.ZArith.
Require Import Coq.Lists.List.
Import ListNotations.

(* 256-bit modeled as Z with bound *)
Definition uint256 := Z.
Definition bytes32 := Z.
Definition txid := bytes32.

Inductive Asset :=
| ORU
| RES.

Record Tx := {
  tx_id : txid;
  asset_in : Asset;
  fee : Z;
  mint_cost : Z;
}.

Record Block := {
  txs : list Tx;
  height : Z;
  prev_hash : bytes32;
}.
```

---

## III. ConvergeRoot (ConvergeRoot.v)

```coq
Definition sort_txids (l : list txid) : list txid :=
  List.sort Z.leb l.

Parameter hash256 : Z -> Z.

Definition concat_txids (l : list txid) : Z :=
  fold_left Z.add l 0.

Definition ConvergeRoot (F : list Tx) : bytes32 :=
  let ids := map tx_id F in
  let sorted := sort_txids ids in
  match sorted with
  | [] => 0
  | _ => hash256 (concat_txids sorted)
  end.
```

---

## IV. SIGMA (Sigma.v)

```coq
Definition Sigma_input (t : Tx) (C B : bytes32) (h : Z) : Z :=
  tx_id t + C + B + h.

Definition Sigma (t : Tx) (C B : bytes32) (h : Z) : Z :=
  hash256 (Sigma_input t C B h).
```

---

## V. Weight f_i (Selection.v)

```coq
Definition weight (t : Tx) : Z :=
  match asset_in t with
  | ORU => mint_cost t     (* Mint path *)
  | RES => fee t      (* Spend path *)
  end.
```

---

## VI. Core Inequality

```coq
(* RFC-014 Conv-Decouple-01 amendment (2026-05-25):
   `T` is the immutable constitutional constant. Only the issuance-rate
   lemma is affected; Sigma-CR-01 and replay-equivalence proofs are unchanged. *)
(* RFC-014 v1.1 §2.1 — value corrected under SECURITY-001-A / A-6.
   Was: Z.shiftl 1 240, which is vacuous because weight is in satoshis. *)
Definition MIN_CONVERGENCE_AMOUNT : Z := 100000000.
Definition RES_PROBABILITY_TARGET : Z := Z.shiftl 1 240 / MIN_CONVERGENCE_AMOUNT.
Definition eligible (t : Tx) (C B : bytes32) (h : Z) : Prop :=
  Sigma t C B h < RES_PROBABILITY_TARGET * weight t.

(* Legacy parameterised form, retained for transitional lemma compatibility: *)
Definition eligible_with_T (t : Tx) (C B : bytes32) (h T : Z) : Prop :=
  Sigma t C B h < T * weight t.
```

---

## VII. Winner Selection (No Division)

```coq
Definition better (a b : Tx) (C B : bytes32) (h : Z) : Prop :=
  Sigma a C B h * weight b < Sigma b C B h * weight a.

Fixpoint select_winner (l : list Tx) (C B : bytes32) (h : Z) : option Tx :=
  match l with
  | [] => None
  | x :: xs =>
      match select_winner xs C B h with
      | None => Some x
      | Some y =>
          if Z_lt_dec
               (Sigma x C B h * weight y)
               (Sigma y C B h * weight x)
          then Some x else Some y
      end
  end.
```

---

## VIII. Execution Semantics (Execution.v)

```coq
Definition is_mint (t : Tx) : bool :=
  match asset_in t with
  | ORU => true
  | _ => false
  end.

Definition build_F (txs : list Tx) : list Tx :=
  filter is_mint txs.

Definition Execute (b : Block) (T B : Z) : option Tx :=
  let F := build_F b.(txs) in
  let C := ConvergeRoot F in
  let candidates :=
      filter (fun t =>
        if Z_lt_dec (Sigma t C B b.(height)) (T * weight t)
        then true else false) F
  in
  select_winner candidates C B b.(height).
```

---

## IX. Most Critical Theorems (Must be proven)

### 1. Determinism

```coq
Theorem selection_deterministic :
  forall b T B w1 w2,
    Execute b T B = Some w1 ->
    Execute b T B = Some w2 ->
    w1 = w2.
```

### 2. Split Invariance

```coq
Theorem split_invariance :
  forall f_total parts,
    sum parts = f_total ->
    probability(parts) = probability([f_total]).
```

**(Abstract probability model required here)**

### 3. Monotonicity

```coq
Theorem weight_monotonic :
  forall t1 t2,
    weight t1 > weight t2 ->
    Pr(t1 wins) >= Pr(t2 wins).
```

### 4. Safety (No Double Mint)

```coq
Theorem unique_mint :
  forall b T B w1 w2,
    Execute b T B = Some w1 ->
    Execute b T B = Some w2 ->
    w1 = w2.
```

### 5. Supply Bound (RES Cap)

```coq
Theorem supply_bound :
  forall state,
    state.RES_supply <= RES_CAP.
```

---

## X. Implementation Consistency Proof (Most Critical)

```coq
Definition Verify (b : Block) : bool :=
  (* RFC-005 *)

Theorem verify_correct :
  forall b,
    Verify b = true <-> exists w, Execute b = Some w.
```

**This is the ultimate lock**

---

## XI. Prohibitions

**Prohibited:**
- Use float
- Use machine int (must use Z)
- Use undefined hash
- Modify weight definition
- Change sorting rules
- Introduce any non-determinism
- Change execution order
- Use witness decision semantics

---

## XII. Extraction (Landing to C++)

Extraction Language: OCaml.

```coq
Extraction "rigor.ml" Execute ConvergeRoot Sigma.
```

Then:
```
OCaml → C++ (Binding)
```

---

**END OF RFC-007**
