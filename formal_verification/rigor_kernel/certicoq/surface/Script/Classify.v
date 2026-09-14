(** RigorSurface.Script.Classify — [classify_script] definitional slice.

    Copied from [Script/ScriptClassifier.v] up to the classifier.
    Theorems stay in the cone.
*)
From Coq Require Import Arith.Arith.
From Coq Require Import Bool.Bool.
From Coq Require Import List.
Import ListNotations.
From RigorSurface Require Import Core.Bytes.
From RigorSurface Require Import Script.ScriptTypes.

Definition RIGOR_PUSH_0 : byte := 0%nat.
Definition RIGOR_PUSH_1 : byte := 81%nat.
Definition RIGOR_PUSH_16 : byte := 96%nat.
Definition RIGOR_MARKER_RETURN : byte := 106%nat.
Definition RIGOR_PUSHBYTES_20 : byte := 20%nat.
Definition RIGOR_PUSHDATA1 : byte := 76%nat.
Definition RIGOR_PUSHDATA2 : byte := 77%nat.
Definition RIGOR_PUSHDATA4 : byte := 78%nat.
Definition RIGOR_MARKER_RES_MINT : byte := 187%nat.
Definition RIGOR_MARKER_CONVERGE : byte := 189%nat.

Definition cvg2_tag : list byte := [67; 86; 71; 50]%nat.
Definition res2_tag : list byte := [82; 69; 83; 50]%nat.

Definition witness_version_of (b : byte) : option nat :=
  if Nat.eqb b RIGOR_PUSH_0 then Some 0%nat
  else if andb (Nat.leb RIGOR_PUSH_1 b) (Nat.leb b RIGOR_PUSH_16)
       then Some (b - 80)%nat
       else None.

Definition parse_witness_program (s : Script) : option (nat * list byte) :=
  let n := length s in
  if orb (Nat.ltb n 4%nat) (Nat.ltb 42%nat n) then None
  else
    match s with
    | ver :: plen :: rest =>
        match witness_version_of ver with
        | None => None
        | Some v =>
            if andb (Nat.ltb 0%nat plen) (Nat.eqb (length rest) plen)
            then Some (v, rest)
            else None
        end
    | _ => None
    end.

Definition prefix4_eq (prog tag : list byte) : bool :=
  match prog, tag with
  | a :: b :: c :: d :: _, x :: y :: z :: w :: nil =>
      andb (Nat.eqb a x) (andb (Nat.eqb b y) (andb (Nat.eqb c z) (Nat.eqb d w)))
  | _, _ => false
  end.

Definition is_native_res_program (prog : list byte) : bool :=
  andb (Nat.eqb (length prog) 32%nat)
       (andb (negb (prefix4_eq prog cvg2_tag))
             (negb (prefix4_eq prog res2_tag))).

Definition is_mint_script (s : Script) : bool :=
  match s with
  | sn_op :: rest =>
      let after_sn :=
        if Nat.eqb sn_op RIGOR_PUSH_0 then Some rest
        else if andb (Nat.leb 1%nat sn_op) (Nat.leb sn_op 4%nat) then
          match split_at sn_op rest with
          | Some (_, tail) => Some tail
          | None => None
          end
        else None
      in
      match after_sn with
      | Some (plen :: body) =>
          if Nat.eqb plen RIGOR_PUSHBYTES_20 then
            match split_at 20%nat body with
            | Some (_, marker_tl) =>
                match marker_tl with
                | op :: nil => Nat.eqb op RIGOR_MARKER_RES_MINT
                | _ => false
                end
            | None => false
            end
          else false
      | _ => false
      end
  | nil => false
  end.

Definition is_converge_script (s : Script) : bool :=
  match s with
  | plen :: rest =>
      andb (Nat.eqb plen RIGOR_PUSHBYTES_20)
           (andb (Nat.eqb (length rest) 21%nat)
                 (match rev rest with
                  | op :: hash_rev =>
                      andb (Nat.eqb op RIGOR_MARKER_CONVERGE)
                           (Nat.eqb (length hash_rev) 20%nat)
                  | nil => false
                  end))
  | nil => false
  end.

Fixpoint is_push_only_n (fuel : nat) (rest : list byte) : bool :=
  match fuel with
  | O => match rest with nil => true | _ => false end
  | S fuel' =>
      match rest with
      | nil => true
      | op :: tail =>
          if Nat.leb op 75%nat then
            match split_at op tail with
            | Some (_, rest') => is_push_only_n fuel' rest'
            | None => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA1 then
            match tail with
            | ln :: data =>
                match split_at ln data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | nil => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA2 then
            match tail with
            | lo :: hi :: data =>
                match split_at (lo + hi * 256)%nat data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.eqb op RIGOR_PUSHDATA4 then
            match tail with
            | a :: b :: c :: d :: data =>
                match split_at
                        (a + b * 256 + c * 256 * 256 + d * 256 * 256 * 256)%nat
                        data with
                | Some (_, rest') => is_push_only_n fuel' rest'
                | None => false
                end
            | _ => false
            end
          else if Nat.leb op RIGOR_PUSH_16 then
            is_push_only_n fuel' tail
          else false
      end
  end.

Definition is_push_only (rest : list byte) : bool :=
  is_push_only_n (length rest) rest.

Definition is_null_data (s : Script) : bool :=
  match s with
  | x :: rest => andb (Nat.eqb x RIGOR_MARKER_RETURN) (is_push_only rest)
  | nil => false
  end.

Definition classify_script (s : Script) : ScriptType :=
  match parse_witness_program s with
  | Some (0%nat, prog) =>
      if Nat.eqb (length prog) 20%nat then T_WITNESS_V0 else T_NONSTANDARD
  | Some (1%nat, prog) =>
      if is_native_res_program prog then T_WITNESS_V1 else T_NONSTANDARD
  | Some _ => T_NONSTANDARD
  | None =>
      if is_mint_script s then T_MINT
      else if is_converge_script s then T_CONVERGE
      else if is_null_data s then T_NULL_DATA
      else T_NONSTANDARD
  end.
