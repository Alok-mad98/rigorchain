
(** val negb : bool -> bool **)

let negb = function
| true -> false
| false -> true

type nat =
| O
| S of nat

(** val fst : ('a1 * 'a2) -> 'a1 **)

let fst = function
| (x, _) -> x

(** val length : 'a1 list -> nat **)

let rec length = function
| [] -> O
| _ :: l' -> S (length l')

(** val app : 'a1 list -> 'a1 list -> 'a1 list **)

let rec app l m =
  match l with
  | [] -> m
  | a :: l1 -> a :: (app l1 m)

type comparison =
| Eq
| Lt
| Gt

(** val compOpp : comparison -> comparison **)

let compOpp = function
| Eq -> Eq
| Lt -> Gt
| Gt -> Lt

(** val add : nat -> nat -> nat **)

let rec add n0 m =
  match n0 with
  | O -> m
  | S p -> S (add p m)

(** val mul : nat -> nat -> nat **)

let rec mul n0 m =
  match n0 with
  | O -> O
  | S p -> add m (mul p m)

(** val sub : nat -> nat -> nat **)

let rec sub n0 m =
  match n0 with
  | O -> n0
  | S k -> (match m with
            | O -> n0
            | S l -> sub k l)

module Nat =
 struct
  (** val eqb : nat -> nat -> bool **)

  let rec eqb n0 m =
    match n0 with
    | O -> (match m with
            | O -> true
            | S _ -> false)
    | S n' -> (match m with
               | O -> false
               | S m' -> eqb n' m')

  (** val leb : nat -> nat -> bool **)

  let rec leb n0 m =
    match n0 with
    | O -> true
    | S n' -> (match m with
               | O -> false
               | S m' -> leb n' m')

  (** val ltb : nat -> nat -> bool **)

  let ltb n0 m =
    leb (S n0) m
 end

type positive =
| XI of positive
| XO of positive
| XH

type n =
| N0
| Npos of positive

type z =
| Z0
| Zpos of positive
| Zneg of positive

module Pos =
 struct
  type mask =
  | IsNul
  | IsPos of positive
  | IsNeg
 end

module Coq_Pos =
 struct
  (** val succ : positive -> positive **)

  let rec succ = function
  | XI p -> XO (succ p)
  | XO p -> XI p
  | XH -> XO XH

  (** val add : positive -> positive -> positive **)

  let rec add x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> XO (add_carry p q)
       | XO q -> XI (add p q)
       | XH -> XO (succ p))
    | XO p ->
      (match y with
       | XI q -> XI (add p q)
       | XO q -> XO (add p q)
       | XH -> XI p)
    | XH -> (match y with
             | XI q -> XO (succ q)
             | XO q -> XI q
             | XH -> XO XH)

  (** val add_carry : positive -> positive -> positive **)

  and add_carry x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> XI (add_carry p q)
       | XO q -> XO (add_carry p q)
       | XH -> XI (succ p))
    | XO p ->
      (match y with
       | XI q -> XO (add_carry p q)
       | XO q -> XI (add p q)
       | XH -> XO (succ p))
    | XH ->
      (match y with
       | XI q -> XI (succ q)
       | XO q -> XO (succ q)
       | XH -> XI XH)

  (** val pred_double : positive -> positive **)

  let rec pred_double = function
  | XI p -> XI (XO p)
  | XO p -> XI (pred_double p)
  | XH -> XH

  (** val pred_N : positive -> n **)

  let pred_N = function
  | XI p -> Npos (XO p)
  | XO p -> Npos (pred_double p)
  | XH -> N0

  type mask = Pos.mask =
  | IsNul
  | IsPos of positive
  | IsNeg

  (** val succ_double_mask : mask -> mask **)

  let succ_double_mask = function
  | IsNul -> IsPos XH
  | IsPos p -> IsPos (XI p)
  | IsNeg -> IsNeg

  (** val double_mask : mask -> mask **)

  let double_mask = function
  | IsPos p -> IsPos (XO p)
  | x0 -> x0

  (** val double_pred_mask : positive -> mask **)

  let double_pred_mask = function
  | XI p -> IsPos (XO (XO p))
  | XO p -> IsPos (XO (pred_double p))
  | XH -> IsNul

  (** val sub_mask : positive -> positive -> mask **)

  let rec sub_mask x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> double_mask (sub_mask p q)
       | XO q -> succ_double_mask (sub_mask p q)
       | XH -> IsPos (XO p))
    | XO p ->
      (match y with
       | XI q -> succ_double_mask (sub_mask_carry p q)
       | XO q -> double_mask (sub_mask p q)
       | XH -> IsPos (pred_double p))
    | XH -> (match y with
             | XH -> IsNul
             | _ -> IsNeg)

  (** val sub_mask_carry : positive -> positive -> mask **)

  and sub_mask_carry x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> succ_double_mask (sub_mask_carry p q)
       | XO q -> double_mask (sub_mask p q)
       | XH -> IsPos (pred_double p))
    | XO p ->
      (match y with
       | XI q -> double_mask (sub_mask_carry p q)
       | XO q -> succ_double_mask (sub_mask_carry p q)
       | XH -> double_pred_mask p)
    | XH -> IsNeg

  (** val mul : positive -> positive -> positive **)

  let rec mul x y =
    match x with
    | XI p -> add y (XO (mul p y))
    | XO p -> XO (mul p y)
    | XH -> y

  (** val iter : ('a1 -> 'a1) -> 'a1 -> positive -> 'a1 **)

  let rec iter f x = function
  | XI n' -> f (iter f (iter f x n') n')
  | XO n' -> iter f (iter f x n') n'
  | XH -> f x

  (** val div2 : positive -> positive **)

  let div2 = function
  | XI p0 -> p0
  | XO p0 -> p0
  | XH -> XH

  (** val div2_up : positive -> positive **)

  let div2_up = function
  | XI p0 -> succ p0
  | XO p0 -> p0
  | XH -> XH

  (** val size : positive -> positive **)

  let rec size = function
  | XI p0 -> succ (size p0)
  | XO p0 -> succ (size p0)
  | XH -> XH

  (** val compare_cont : comparison -> positive -> positive -> comparison **)

  let rec compare_cont r x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> compare_cont r p q
       | XO q -> compare_cont Gt p q
       | XH -> Gt)
    | XO p ->
      (match y with
       | XI q -> compare_cont Lt p q
       | XO q -> compare_cont r p q
       | XH -> Gt)
    | XH -> (match y with
             | XH -> r
             | _ -> Lt)

  (** val compare : positive -> positive -> comparison **)

  let compare =
    compare_cont Eq

  (** val eqb : positive -> positive -> bool **)

  let rec eqb p q =
    match p with
    | XI p0 -> (match q with
                | XI q0 -> eqb p0 q0
                | _ -> false)
    | XO p0 -> (match q with
                | XO q0 -> eqb p0 q0
                | _ -> false)
    | XH -> (match q with
             | XH -> true
             | _ -> false)

  (** val testbit : positive -> n -> bool **)

  let rec testbit p n0 =
    match p with
    | XI p0 -> (match n0 with
                | N0 -> true
                | Npos n1 -> testbit p0 (pred_N n1))
    | XO p0 -> (match n0 with
                | N0 -> false
                | Npos n1 -> testbit p0 (pred_N n1))
    | XH -> (match n0 with
             | N0 -> true
             | Npos _ -> false)
 end

module N =
 struct
  (** val succ_double : n -> n **)

  let succ_double = function
  | N0 -> Npos XH
  | Npos p -> Npos (XI p)

  (** val double : n -> n **)

  let double = function
  | N0 -> N0
  | Npos p -> Npos (XO p)

  (** val sub : n -> n -> n **)

  let sub n0 m =
    match n0 with
    | N0 -> N0
    | Npos n' ->
      (match m with
       | N0 -> n0
       | Npos m' ->
         (match Coq_Pos.sub_mask n' m' with
          | Coq_Pos.IsPos p -> Npos p
          | _ -> N0))

  (** val compare : n -> n -> comparison **)

  let compare n0 m =
    match n0 with
    | N0 -> (match m with
             | N0 -> Eq
             | Npos _ -> Lt)
    | Npos n' -> (match m with
                  | N0 -> Gt
                  | Npos m' -> Coq_Pos.compare n' m')

  (** val leb : n -> n -> bool **)

  let leb x y =
    match compare x y with
    | Gt -> false
    | _ -> true

  (** val pos_div_eucl : positive -> n -> n * n **)

  let rec pos_div_eucl a b =
    match a with
    | XI a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = succ_double r in
      if leb b r' then ((succ_double q), (sub r' b)) else ((double q), r')
    | XO a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = double r in
      if leb b r' then ((succ_double q), (sub r' b)) else ((double q), r')
    | XH ->
      (match b with
       | N0 -> (N0, (Npos XH))
       | Npos p -> (match p with
                    | XH -> ((Npos XH), N0)
                    | _ -> (N0, (Npos XH))))

  (** val testbit : n -> n -> bool **)

  let testbit a n0 =
    match a with
    | N0 -> false
    | Npos p -> Coq_Pos.testbit p n0
 end

(** val rev : 'a1 list -> 'a1 list **)

let rec rev = function
| [] -> []
| x :: l' -> app (rev l') (x :: [])

(** val existsb : ('a1 -> bool) -> 'a1 list -> bool **)

let rec existsb f = function
| [] -> false
| a :: l0 -> (||) (f a) (existsb f l0)

(** val forallb : ('a1 -> bool) -> 'a1 list -> bool **)

let rec forallb f = function
| [] -> true
| a :: l0 -> (&&) (f a) (forallb f l0)

module Z =
 struct
  (** val double : z -> z **)

  let double = function
  | Z0 -> Z0
  | Zpos p -> Zpos (XO p)
  | Zneg p -> Zneg (XO p)

  (** val succ_double : z -> z **)

  let succ_double = function
  | Z0 -> Zpos XH
  | Zpos p -> Zpos (XI p)
  | Zneg p -> Zneg (Coq_Pos.pred_double p)

  (** val pred_double : z -> z **)

  let pred_double = function
  | Z0 -> Zneg XH
  | Zpos p -> Zpos (Coq_Pos.pred_double p)
  | Zneg p -> Zneg (XI p)

  (** val pos_sub : positive -> positive -> z **)

  let rec pos_sub x y =
    match x with
    | XI p ->
      (match y with
       | XI q -> double (pos_sub p q)
       | XO q -> succ_double (pos_sub p q)
       | XH -> Zpos (XO p))
    | XO p ->
      (match y with
       | XI q -> pred_double (pos_sub p q)
       | XO q -> double (pos_sub p q)
       | XH -> Zpos (Coq_Pos.pred_double p))
    | XH ->
      (match y with
       | XI q -> Zneg (XO q)
       | XO q -> Zneg (Coq_Pos.pred_double q)
       | XH -> Z0)

  (** val add : z -> z -> z **)

  let add x y =
    match x with
    | Z0 -> y
    | Zpos x' ->
      (match y with
       | Z0 -> x
       | Zpos y' -> Zpos (Coq_Pos.add x' y')
       | Zneg y' -> pos_sub x' y')
    | Zneg x' ->
      (match y with
       | Z0 -> x
       | Zpos y' -> pos_sub y' x'
       | Zneg y' -> Zneg (Coq_Pos.add x' y'))

  (** val opp : z -> z **)

  let opp = function
  | Z0 -> Z0
  | Zpos x0 -> Zneg x0
  | Zneg x0 -> Zpos x0

  (** val sub : z -> z -> z **)

  let sub m n0 =
    add m (opp n0)

  (** val mul : z -> z -> z **)

  let mul x y =
    match x with
    | Z0 -> Z0
    | Zpos x' ->
      (match y with
       | Z0 -> Z0
       | Zpos y' -> Zpos (Coq_Pos.mul x' y')
       | Zneg y' -> Zneg (Coq_Pos.mul x' y'))
    | Zneg x' ->
      (match y with
       | Z0 -> Z0
       | Zpos y' -> Zneg (Coq_Pos.mul x' y')
       | Zneg y' -> Zpos (Coq_Pos.mul x' y'))

  (** val compare : z -> z -> comparison **)

  let compare x y =
    match x with
    | Z0 -> (match y with
             | Z0 -> Eq
             | Zpos _ -> Lt
             | Zneg _ -> Gt)
    | Zpos x' -> (match y with
                  | Zpos y' -> Coq_Pos.compare x' y'
                  | _ -> Gt)
    | Zneg x' ->
      (match y with
       | Zneg y' -> compOpp (Coq_Pos.compare x' y')
       | _ -> Lt)

  (** val leb : z -> z -> bool **)

  let leb x y =
    match compare x y with
    | Gt -> false
    | _ -> true

  (** val ltb : z -> z -> bool **)

  let ltb x y =
    match compare x y with
    | Lt -> true
    | _ -> false

  (** val eqb : z -> z -> bool **)

  let eqb x y =
    match x with
    | Z0 -> (match y with
             | Z0 -> true
             | _ -> false)
    | Zpos p -> (match y with
                 | Zpos q -> Coq_Pos.eqb p q
                 | _ -> false)
    | Zneg p -> (match y with
                 | Zneg q -> Coq_Pos.eqb p q
                 | _ -> false)

  (** val of_N : n -> z **)

  let of_N = function
  | N0 -> Z0
  | Npos p -> Zpos p

  (** val pos_div_eucl : positive -> z -> z * z **)

  let rec pos_div_eucl a b =
    match a with
    | XI a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = add (mul (Zpos (XO XH)) r) (Zpos XH) in
      if ltb r' b
      then ((mul (Zpos (XO XH)) q), r')
      else ((add (mul (Zpos (XO XH)) q) (Zpos XH)), (sub r' b))
    | XO a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = mul (Zpos (XO XH)) r in
      if ltb r' b
      then ((mul (Zpos (XO XH)) q), r')
      else ((add (mul (Zpos (XO XH)) q) (Zpos XH)), (sub r' b))
    | XH -> if leb (Zpos (XO XH)) b then (Z0, (Zpos XH)) else ((Zpos XH), Z0)

  (** val div_eucl : z -> z -> z * z **)

  let div_eucl a b =
    match a with
    | Z0 -> (Z0, Z0)
    | Zpos a' ->
      (match b with
       | Z0 -> (Z0, a)
       | Zpos _ -> pos_div_eucl a' b
       | Zneg b' ->
         let (q, r) = pos_div_eucl a' (Zpos b') in
         (match r with
          | Z0 -> ((opp q), Z0)
          | _ -> ((opp (add q (Zpos XH))), (add b r))))
    | Zneg a' ->
      (match b with
       | Z0 -> (Z0, a)
       | Zpos _ ->
         let (q, r) = pos_div_eucl a' b in
         (match r with
          | Z0 -> ((opp q), Z0)
          | _ -> ((opp (add q (Zpos XH))), (sub b r)))
       | Zneg b' -> let (q, r) = pos_div_eucl a' (Zpos b') in (q, (opp r)))

  (** val div : z -> z -> z **)

  let div a b =
    let (q, _) = div_eucl a b in q

  (** val modulo : z -> z -> z **)

  let modulo a b =
    let (_, r) = div_eucl a b in r

  (** val quotrem : z -> z -> z * z **)

  let quotrem a b =
    match a with
    | Z0 -> (Z0, Z0)
    | Zpos a0 ->
      (match b with
       | Z0 -> (Z0, a)
       | Zpos b0 ->
         let (q, r) = N.pos_div_eucl a0 (Npos b0) in ((of_N q), (of_N r))
       | Zneg b0 ->
         let (q, r) = N.pos_div_eucl a0 (Npos b0) in
         ((opp (of_N q)), (of_N r)))
    | Zneg a0 ->
      (match b with
       | Z0 -> (Z0, a)
       | Zpos b0 ->
         let (q, r) = N.pos_div_eucl a0 (Npos b0) in
         ((opp (of_N q)), (opp (of_N r)))
       | Zneg b0 ->
         let (q, r) = N.pos_div_eucl a0 (Npos b0) in
         ((of_N q), (opp (of_N r))))

  (** val quot : z -> z -> z **)

  let quot a b =
    fst (quotrem a b)

  (** val odd : z -> bool **)

  let odd = function
  | Z0 -> false
  | Zpos p -> (match p with
               | XO _ -> false
               | _ -> true)
  | Zneg p -> (match p with
               | XO _ -> false
               | _ -> true)

  (** val div2 : z -> z **)

  let div2 = function
  | Z0 -> Z0
  | Zpos p -> (match p with
               | XH -> Z0
               | _ -> Zpos (Coq_Pos.div2 p))
  | Zneg p -> Zneg (Coq_Pos.div2_up p)

  (** val log2 : z -> z **)

  let log2 = function
  | Zpos p0 ->
    (match p0 with
     | XI p -> Zpos (Coq_Pos.size p)
     | XO p -> Zpos (Coq_Pos.size p)
     | XH -> Z0)
  | _ -> Z0

  (** val testbit : z -> z -> bool **)

  let testbit a = function
  | Z0 -> odd a
  | Zpos p ->
    (match a with
     | Z0 -> false
     | Zpos a0 -> Coq_Pos.testbit a0 (Npos p)
     | Zneg a0 -> negb (N.testbit (Coq_Pos.pred_N a0) (Npos p)))
  | Zneg _ -> false

  (** val shiftl : z -> z -> z **)

  let shiftl a = function
  | Z0 -> a
  | Zpos p -> Coq_Pos.iter (mul (Zpos (XO XH))) a p
  | Zneg p -> Coq_Pos.iter div2 a p

  (** val shiftr : z -> z -> z **)

  let shiftr a n0 =
    shiftl a (opp n0)
 end

type byte = nat

(** val split_at : nat -> byte list -> (byte list * byte list) option **)

let rec split_at n0 xs =
  match n0 with
  | O -> Some ([], xs)
  | S n' ->
    (match xs with
     | [] -> None
     | x :: xs' ->
       (match split_at n' xs' with
        | Some p -> let (pre, post) = p in Some ((x :: pre), post)
        | None -> None))

type amount = z

(** val cOIN : amount **)

let cOIN =
  Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XI (XO (XO (XO (XO (XI (XI (XI (XI
    (XO (XI (XO (XI (XI (XI (XI (XI (XO XH))))))))))))))))))))))))))

(** val mAX_MONEY : amount **)

let mAX_MONEY =
  Z.mul (Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XI (XO (XO (XI (XI (XI
    (XI (XI (XO (XI (XO (XO (XO (XO (XO (XO (XI (XO (XI (XO (XI (XO (XO
    XH)))))))))))))))))))))))))))))))))) cOIN

(** val money_range_b : amount -> bool **)

let money_range_b v =
  (&&) (Z.leb Z0 v) (Z.leb v mAX_MONEY)

type hash32 = byte list

type outpoint = { op_txid : hash32; op_index : nat }

type txout = { txo_value : z; txo_spk : byte list }

type tx = { tx_id : hash32; tx_in : outpoint list; tx_out : txout list;
            tx_coinbase : bool }

(** val bytes_eqb : byte list -> byte list -> bool **)

let rec bytes_eqb a b =
  match a with
  | [] -> (match b with
           | [] -> true
           | _ :: _ -> false)
  | x :: xs ->
    (match b with
     | [] -> false
     | y :: ys -> (&&) (Nat.eqb x y) (bytes_eqb xs ys))

(** val outpoint_eqb : outpoint -> outpoint -> bool **)

let outpoint_eqb o1 o2 =
  (&&) (bytes_eqb o1.op_txid o2.op_txid) (Nat.eqb o1.op_index o2.op_index)

(** val is_output_op_b : tx -> outpoint -> bool **)

let is_output_op_b t o =
  (&&) (bytes_eqb o.op_txid t.tx_id) (Nat.ltb o.op_index (length t.tx_out))

(** val inputs_fresh_b : tx -> bool **)

let inputs_fresh_b t =
  forallb (fun o -> negb (is_output_op_b t o)) t.tx_in

(** val sum_values : txout list -> z **)

let rec sum_values = function
| [] -> Z0
| o :: rest -> Z.add o.txo_value (sum_values rest)

(** val nodupb : outpoint list -> bool **)

let rec nodupb = function
| [] -> true
| x :: xs -> (&&) (negb (existsb (outpoint_eqb x) xs)) (nodupb xs)

(** val nonemptyb : 'a1 list -> bool **)

let nonemptyb = function
| [] -> false
| _ :: _ -> true

(** val valid_tx_b : tx -> bool **)

let valid_tx_b t =
  (&&) (negb t.tx_coinbase)
    ((&&) (nonemptyb t.tx_in)
      ((&&) (nonemptyb t.tx_out)
        ((&&) (nodupb t.tx_in)
          ((&&) (forallb (fun o -> money_range_b o.txo_value) t.tx_out)
            ((&&) (money_range_b (sum_values t.tx_out)) (inputs_fresh_b t))))))

type uTXOState = tx

(** val check_utxo : uTXOState -> bool **)

let check_utxo =
  valid_tx_b

type input_domain =
| DomORU
| DomRES

type oru_tx = { tx_dom : input_domain; oru_in : z; oru_out : z; res_in : 
                z; res_out : z }

type kernel_tx = { kt_dom : input_domain; kt_oru_in : z; kt_oru_out : 
                   z; kt_res_in : z; kt_res_out : z; kt_old_supply : 
                   z; kt_subsidy : z }

(** val to_oru : kernel_tx -> oru_tx **)

let to_oru t =
  { tx_dom = t.kt_dom; oru_in = t.kt_oru_in; oru_out = t.kt_oru_out; res_in =
    t.kt_res_in; res_out = t.kt_res_out }

type oRUState = kernel_tx

(** val valid_oru_tx_b : oru_tx -> bool **)

let valid_oru_tx_b t =
  (&&) (Z.leb Z0 t.oru_in)
    ((&&) (Z.leb Z0 t.oru_out)
      ((&&) (Z.leb Z0 t.res_in)
        ((&&) (Z.leb Z0 t.res_out)
          ((&&) (Z.leb t.oru_out t.oru_in)
            (match t.tx_dom with
             | DomORU -> (&&) (Z.eqb t.res_in Z0) (Z.eqb t.res_out Z0)
             | DomRES -> (&&) (Z.ltb Z0 t.oru_in) (Z.leb t.res_out t.res_in))))))

(** val check_oru : oRUState -> bool **)

let check_oru s =
  valid_oru_tx_b (to_oru s)

type block_height = z

(** val rES_DESTROY_POLE : z **)

let rES_DESTROY_POLE =
  Z.shiftl (Zpos XH) (Zpos (XI (XO (XO (XI XH)))))

(** val rES_DESTROY_SHIFT : z **)

let rES_DESTROY_SHIFT =
  Zpos (XO (XO (XO (XI (XO XH)))))

(** val rES_MAX_SUPPLY : z **)

let rES_MAX_SUPPLY =
  Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
    (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XI (XO (XO (XO (XO
    (XI (XI (XI (XI (XO (XI (XO (XI (XI (XI (XI (XI (XO
    XH))))))))))))))))))))))))))))))))))))))))))))))))

(** val res_mint_authorized : block_height -> bool **)

let res_mint_authorized h =
  Z.leb Z0 h

(** val res_destroy_pole_sq : z -> z **)

let res_destroy_pole_sq h =
  Z.mul (Z.add rES_DESTROY_POLE h) (Z.add rES_DESTROY_POLE h)

type res_tx = { rt_coinbase : bool; rt_mint_ok : bool; rt_mint_amount : 
                z; rt_height : z; rt_burn_oru : z; rt_dom : input_domain;
                rt_res_in : z; rt_res_out : z; rt_oru_in : z }

(** val coinbase : res_tx -> bool **)

let coinbase tx0 =
  tx0.rt_coinbase

(** val mint_amount : res_tx -> z **)

let mint_amount tx0 =
  tx0.rt_mint_amount

(** val burn_oru : res_tx -> z **)

let burn_oru tx0 =
  tx0.rt_burn_oru

type rESState = res_tx

(** val check_res : rESState -> bool **)

let check_res s =
  (&&) (negb (coinbase s))
    ((&&) s.rt_mint_ok
      ((&&) (Z.ltb Z0 (mint_amount s))
        ((&&) (Z.leb (mint_amount s) rES_MAX_SUPPLY)
          ((&&) (res_mint_authorized s.rt_height)
            ((&&) (Z.leb Z0 (burn_oru s))
              (Z.leb
                (Z.mul (mint_amount s) (res_destroy_pole_sq s.rt_height))
                (Z.shiftl (burn_oru s) rES_DESTROY_SHIFT)))))))

type script = byte list

type scriptType =
| T_P2PKH
| T_P2SH
| T_WITNESS_V0
| T_WITNESS_V1
| T_NONSTANDARD
| T_MINT
| T_CONVERGE
| T_NULL_DATA

(** val script_type_eqb : scriptType -> scriptType -> bool **)

let script_type_eqb a b =
  match a with
  | T_P2PKH -> (match b with
                | T_P2PKH -> true
                | _ -> false)
  | T_P2SH -> (match b with
               | T_P2SH -> true
               | _ -> false)
  | T_WITNESS_V0 -> (match b with
                     | T_WITNESS_V0 -> true
                     | _ -> false)
  | T_WITNESS_V1 -> (match b with
                     | T_WITNESS_V1 -> true
                     | _ -> false)
  | T_NONSTANDARD -> (match b with
                      | T_NONSTANDARD -> true
                      | _ -> false)
  | T_MINT -> (match b with
               | T_MINT -> true
               | _ -> false)
  | T_CONVERGE -> (match b with
                   | T_CONVERGE -> true
                   | _ -> false)
  | T_NULL_DATA -> (match b with
                    | T_NULL_DATA -> true
                    | _ -> false)

(** val rIGOR_PUSH_0 : byte **)

let rIGOR_PUSH_0 =
  O

(** val rIGOR_PUSH_1 : byte **)

let rIGOR_PUSH_1 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSH_16 : byte **)

let rIGOR_PUSH_16 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_RETURN : byte **)

let rIGOR_MARKER_RETURN =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHBYTES_20 : byte **)

let rIGOR_PUSHBYTES_20 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O)))))))))))))))))))

(** val rIGOR_PUSHDATA1 : byte **)

let rIGOR_PUSHDATA1 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHDATA2 : byte **)

let rIGOR_PUSHDATA2 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHDATA4 : byte **)

let rIGOR_PUSHDATA4 =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_RES_MINT : byte **)

let rIGOR_MARKER_RES_MINT =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_CONVERGE : byte **)

let rIGOR_MARKER_CONVERGE =
  S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val cvg2_tag : byte list **)

let cvg2_tag =
  (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S O)))))))))))))))))))))))))))))))))))))))))))))))))) :: [])))

(** val res2_tag : byte list **)

let res2_tag =
  (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S
    O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S
    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
    (S O)))))))))))))))))))))))))))))))))))))))))))))))))) :: [])))

(** val witness_version_of : byte -> nat option **)

let witness_version_of b =
  if Nat.eqb b rIGOR_PUSH_0
  then Some O
  else if (&&) (Nat.leb rIGOR_PUSH_1 b) (Nat.leb b rIGOR_PUSH_16)
       then Some
              (sub b (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
                (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
                (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
                (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
                (S
                O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
       else None

(** val parse_witness_program : script -> (nat * byte list) option **)

let parse_witness_program s =
  let n0 = length s in
  if (||) (Nat.ltb n0 (S (S (S (S O)))))
       (Nat.ltb (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
         (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
         O)))))))))))))))))))))))))))))))))))))))))) n0)
  then None
  else (match s with
        | [] -> None
        | ver :: l ->
          (match l with
           | [] -> None
           | plen :: rest ->
             (match witness_version_of ver with
              | Some v ->
                if (&&) (Nat.ltb O plen) (Nat.eqb (length rest) plen)
                then Some (v, rest)
                else None
              | None -> None)))

(** val prefix4_eq : byte list -> byte list -> bool **)

let prefix4_eq prog tag =
  match prog with
  | [] -> false
  | a :: l ->
    (match l with
     | [] -> false
     | b :: l0 ->
       (match l0 with
        | [] -> false
        | c :: l1 ->
          (match l1 with
           | [] -> false
           | d :: _ ->
             (match tag with
              | [] -> false
              | x :: l3 ->
                (match l3 with
                 | [] -> false
                 | y :: l4 ->
                   (match l4 with
                    | [] -> false
                    | z0 :: l5 ->
                      (match l5 with
                       | [] -> false
                       | w :: l6 ->
                         (match l6 with
                          | [] ->
                            (&&) (Nat.eqb a x)
                              ((&&) (Nat.eqb b y)
                                ((&&) (Nat.eqb c z0) (Nat.eqb d w)))
                          | _ :: _ -> false))))))))

(** val is_native_res_program : byte list -> bool **)

let is_native_res_program prog =
  (&&)
    (Nat.eqb (length prog) (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
      (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
      O)))))))))))))))))))))))))))))))))
    ((&&) (negb (prefix4_eq prog cvg2_tag)) (negb (prefix4_eq prog res2_tag)))

(** val is_mint_script : script -> bool **)

let is_mint_script = function
| [] -> false
| sn_op :: rest ->
  let after_sn =
    if Nat.eqb sn_op rIGOR_PUSH_0
    then Some rest
    else if (&&) (Nat.leb (S O) sn_op) (Nat.leb sn_op (S (S (S (S O)))))
         then (match split_at sn_op rest with
               | Some p -> let (_, tail) = p in Some tail
               | None -> None)
         else None
  in
  (match after_sn with
   | Some l ->
     (match l with
      | [] -> false
      | plen :: body ->
        if Nat.eqb plen rIGOR_PUSHBYTES_20
        then (match split_at (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
                      (S (S (S (S O)))))))))))))))))))) body with
              | Some p ->
                let (_, marker_tl) = p in
                (match marker_tl with
                 | [] -> false
                 | op :: l0 ->
                   (match l0 with
                    | [] -> Nat.eqb op rIGOR_MARKER_RES_MINT
                    | _ :: _ -> false))
              | None -> false)
        else false)
   | None -> false)

(** val is_converge_script : script -> bool **)

let is_converge_script = function
| [] -> false
| plen :: rest ->
  (&&) (Nat.eqb plen rIGOR_PUSHBYTES_20)
    ((&&)
      (Nat.eqb (length rest) (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
        (S (S (S (S (S O))))))))))))))))))))))
      (match rev rest with
       | [] -> false
       | op :: hash_rev ->
         (&&) (Nat.eqb op rIGOR_MARKER_CONVERGE)
           (Nat.eqb (length hash_rev) (S (S (S (S (S (S (S (S (S (S (S (S (S
             (S (S (S (S (S (S (S O)))))))))))))))))))))))

(** val is_push_only_n : nat -> byte list -> bool **)

let rec is_push_only_n fuel rest =
  match fuel with
  | O -> (match rest with
          | [] -> true
          | _ :: _ -> false)
  | S fuel' ->
    (match rest with
     | [] -> true
     | op :: tail ->
       if Nat.leb op (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
            (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
            (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
            (S (S (S (S (S (S (S (S (S (S (S (S
            O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
       then (match split_at op tail with
             | Some p -> let (_, rest') = p in is_push_only_n fuel' rest'
             | None -> false)
       else if Nat.eqb op rIGOR_PUSHDATA1
            then (match tail with
                  | [] -> false
                  | ln :: data ->
                    (match split_at ln data with
                     | Some p ->
                       let (_, rest') = p in is_push_only_n fuel' rest'
                     | None -> false))
            else if Nat.eqb op rIGOR_PUSHDATA2
                 then (match tail with
                       | [] -> false
                       | lo :: l ->
                         (match l with
                          | [] -> false
                          | hi :: data ->
                            (match split_at
                                     (add lo
                                       (mul hi (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S (S (S (S (S (S (S
                                         (S (S (S (S (S (S
                                         O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                     data with
                             | Some p ->
                               let (_, rest') = p in
                               is_push_only_n fuel' rest'
                             | None -> false)))
                 else if Nat.eqb op rIGOR_PUSHDATA4
                      then (match tail with
                            | [] -> false
                            | a :: l ->
                              (match l with
                               | [] -> false
                               | b :: l0 ->
                                 (match l0 with
                                  | [] -> false
                                  | c :: l1 ->
                                    (match l1 with
                                     | [] -> false
                                     | d :: data ->
                                       (match split_at
                                                (add
                                                  (add
                                                    (add a
                                                      (mul b (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S
                                                        O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                    (mul
                                                      (mul c (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S
                                                        O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                  (mul
                                                    (mul
                                                      (mul d (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S (S
                                                        (S (S (S (S (S (S
                                                        O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      (S (S (S (S (S (S (S (S
                                                      O)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    (S (S (S (S (S (S (S (S
                                                    O))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                data with
                                        | Some p ->
                                          let (_, rest') = p in
                                          is_push_only_n fuel' rest'
                                        | None -> false)))))
                      else if Nat.leb op rIGOR_PUSH_16
                           then is_push_only_n fuel' tail
                           else false)

(** val is_push_only : byte list -> bool **)

let is_push_only rest =
  is_push_only_n (length rest) rest

(** val is_null_data : script -> bool **)

let is_null_data = function
| [] -> false
| x :: rest -> (&&) (Nat.eqb x rIGOR_MARKER_RETURN) (is_push_only rest)

(** val classify_script : script -> scriptType **)

let classify_script s =
  match parse_witness_program s with
  | Some p ->
    let (n0, prog) = p in
    (match n0 with
     | O ->
       if Nat.eqb (length prog) (S (S (S (S (S (S (S (S (S (S (S (S (S (S (S
            (S (S (S (S (S O))))))))))))))))))))
       then T_WITNESS_V0
       else T_NONSTANDARD
     | S n1 ->
       (match n1 with
        | O ->
          if is_native_res_program prog then T_WITNESS_V1 else T_NONSTANDARD
        | S _ -> T_NONSTANDARD))
  | None ->
    if is_mint_script s
    then T_MINT
    else if is_converge_script s
         then T_CONVERGE
         else if is_null_data s then T_NULL_DATA else T_NONSTANDARD

type scriptState = { ss_script : script; ss_expected : scriptType }

(** val check_script : scriptState -> bool **)

let check_script s =
  script_type_eqb (classify_script s.ss_script) s.ss_expected

(** val compact_size : z -> z **)

let compact_size nbits =
  Z.div nbits (Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
    (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO XH)))))))))))))))))))))))))

(** val compact_mantissa : z -> z **)

let compact_mantissa nbits =
  Z.modulo nbits (Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
    (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))))))))))))))))

(** val compact_decode : z -> z **)

let compact_decode nbits =
  let size0 = compact_size nbits in
  let word = compact_mantissa nbits in
  if Z.leb size0 (Zpos (XI XH))
  then Z.shiftr word
         (Z.mul (Zpos (XO (XO (XO XH)))) (Z.sub (Zpos (XI XH)) size0))
  else Z.shiftl word
         (Z.mul (Zpos (XO (XO (XO XH)))) (Z.sub size0 (Zpos (XI XH))))

(** val raw_size : z -> z **)

let raw_size t =
  Z.div (Z.add (Z.log2 t) (Zpos (XO (XO (XO XH))))) (Zpos (XO (XO (XO XH))))

(** val raw_mantissa : z -> z **)

let raw_mantissa t =
  let s = raw_size t in
  if Z.leb s (Zpos (XI XH))
  then Z.shiftl t (Z.mul (Zpos (XO (XO (XO XH)))) (Z.sub (Zpos (XI XH)) s))
  else Z.shiftr t (Z.mul (Zpos (XO (XO (XO XH)))) (Z.sub s (Zpos (XI XH))))

(** val adjust_signbit : z -> z -> z * z **)

let adjust_signbit m s =
  if Z.testbit m (Zpos (XI (XI (XI (XO XH)))))
  then ((Z.shiftr m (Zpos (XO (XO (XO XH))))), (Z.add s (Zpos XH)))
  else (m, s)

(** val compact_encode : z -> z **)

let compact_encode t =
  if Z.leb t Z0
  then Z0
  else let (m, s) = adjust_signbit (raw_mantissa t) (raw_size t) in
       Z.add
         (Z.mul s (Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
           (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
           XH)))))))))))))))))))))))))) m

type powHeader = { pow_digest_z : z; pow_target : z; pow_compact_t : z }

(** val check_pow : powHeader -> bool **)

let check_pow h =
  (&&) (Z.ltb Z0 h.pow_target)
    ((&&) (Z.leb h.pow_digest_z h.pow_target)
      ((&&) (Z.leb Z0 h.pow_compact_t)
        (Z.leb (compact_decode (compact_encode h.pow_compact_t))
          h.pow_compact_t)))

(** val aSERT_RADIX : z **)

let aSERT_RADIX =
  Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
    XH))))))))))))))))

(** val exp_q_raw : z -> z -> z **)

let exp_q_raw te tau =
  Z.quot (Z.mul te aSERT_RADIX) tau

type aSERTInput = { asert_te : z; asert_tau : z; asert_expected_q : z }

(** val check_asert : aSERTInput -> bool **)

let check_asert i =
  (&&) (Z.ltb i.asert_te Z0)
    ((&&) (Z.ltb Z0 i.asert_tau)
      ((&&) (Z.eqb (exp_q_raw i.asert_te i.asert_tau) i.asert_expected_q)
        (Z.eqb (exp_q_raw i.asert_te i.asert_tau)
          (Z.quot (Z.mul i.asert_te aSERT_RADIX) i.asert_tau))))

type kernelValidationRequest =
| KI_UTXO of uTXOState
| KI_ORU of oRUState
| KI_RES of rESState
| KI_Script of scriptState
| KI_Pow of powHeader
| KI_ASERT of aSERTInput

(** val validate_kernel : kernelValidationRequest -> bool **)

let validate_kernel = function
| KI_UTXO s -> check_utxo s
| KI_ORU s -> check_oru s
| KI_RES s -> check_res s
| KI_Script s -> check_script s
| KI_Pow h -> check_pow h
| KI_ASERT i -> check_asert i

(** val extracted_check_utxo : uTXOState -> bool **)

let extracted_check_utxo =
  check_utxo

(** val extracted_check_oru : oRUState -> bool **)

let extracted_check_oru =
  check_oru

(** val extracted_check_res : rESState -> bool **)

let extracted_check_res =
  check_res

(** val extracted_check_script : scriptState -> bool **)

let extracted_check_script =
  check_script

(** val extracted_check_pow : powHeader -> bool **)

let extracted_check_pow =
  check_pow

(** val extracted_check_asert : aSERTInput -> bool **)

let extracted_check_asert =
  check_asert
