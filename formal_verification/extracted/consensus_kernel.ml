
(** val negb : bool -> bool **)

let negb = function
| true -> false
| false -> true

(** val length : 'a1 list -> int **)

let rec length = function
| [] -> 0
| _ :: l' -> Pervasives.succ (length l')

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

module Coq__1 = struct
 (** val add : int -> int -> int **)let rec add = (+)
end
include Coq__1

(** val mul : int -> int -> int **)

let rec mul = ( * )

(** val sub : int -> int -> int **)

let rec sub = fun n m -> Pervasives.max 0 (n-m)

module Nat =
 struct
  (** val ltb : int -> int -> bool **)

  let ltb n m =
    (<=) (Pervasives.succ n) m
 end

type positive =
| XI of positive
| XO of positive
| XH

type z =
| Z0
| Zpos of positive
| Zneg of positive

module Pos =
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

  (** val iter_op : ('a1 -> 'a1 -> 'a1) -> positive -> 'a1 -> 'a1 **)

  let rec iter_op op p a =
    match p with
    | XI p0 -> op a (iter_op op p0 (op a a))
    | XO p0 -> iter_op op p0 (op a a)
    | XH -> a

  (** val to_nat : positive -> int **)

  let to_nat x =
    iter_op Coq__1.add x (Pervasives.succ 0)

  (** val of_succ_nat : int -> positive **)

  let rec of_succ_nat n =
    (fun fO fS n -> if n=0 then fO () else fS (n-1))
      (fun _ -> XH)
      (fun x -> succ (of_succ_nat x))
      n
 end

(** val rev : 'a1 list -> 'a1 list **)

let rec rev = function
| [] -> []
| x :: l' -> app (rev l') (x :: [])

(** val list_eq_dec : ('a1 -> 'a1 -> bool) -> 'a1 list -> 'a1 list -> bool **)

let rec list_eq_dec eq_dec l l' =
  match l with
  | [] -> (match l' with
           | [] -> true
           | _ :: _ -> false)
  | y :: l0 ->
    (match l' with
     | [] -> false
     | a :: l1 -> if eq_dec y a then list_eq_dec eq_dec l0 l1 else false)

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
  | Zneg p -> Zneg (Pos.pred_double p)

  (** val pred_double : z -> z **)

  let pred_double = function
  | Z0 -> Zneg XH
  | Zpos p -> Zpos (Pos.pred_double p)
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
       | XH -> Zpos (Pos.pred_double p))
    | XH ->
      (match y with
       | XI q -> Zneg (XO q)
       | XO q -> Zneg (Pos.pred_double q)
       | XH -> Z0)

  (** val add : z -> z -> z **)

  let add x y =
    match x with
    | Z0 -> y
    | Zpos x' ->
      (match y with
       | Z0 -> x
       | Zpos y' -> Zpos (Pos.add x' y')
       | Zneg y' -> pos_sub x' y')
    | Zneg x' ->
      (match y with
       | Z0 -> x
       | Zpos y' -> pos_sub y' x'
       | Zneg y' -> Zneg (Pos.add x' y'))

  (** val opp : z -> z **)

  let opp = function
  | Z0 -> Z0
  | Zpos x0 -> Zneg x0
  | Zneg x0 -> Zpos x0

  (** val sub : z -> z -> z **)

  let sub m n =
    add m (opp n)

  (** val mul : z -> z -> z **)

  let mul x y =
    match x with
    | Z0 -> Z0
    | Zpos x' ->
      (match y with
       | Z0 -> Z0
       | Zpos y' -> Zpos (Pos.mul x' y')
       | Zneg y' -> Zneg (Pos.mul x' y'))
    | Zneg x' ->
      (match y with
       | Z0 -> Z0
       | Zpos y' -> Zneg (Pos.mul x' y')
       | Zneg y' -> Zpos (Pos.mul x' y'))

  (** val compare : z -> z -> comparison **)

  let compare x y =
    match x with
    | Z0 -> (match y with
             | Z0 -> Eq
             | Zpos _ -> Lt
             | Zneg _ -> Gt)
    | Zpos x' -> (match y with
                  | Zpos y' -> Pos.compare x' y'
                  | _ -> Gt)
    | Zneg x' ->
      (match y with
       | Zneg y' -> compOpp (Pos.compare x' y')
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

  (** val geb : z -> z -> bool **)

  let geb x y =
    match compare x y with
    | Lt -> false
    | _ -> true

  (** val eqb : z -> z -> bool **)

  let eqb x y =
    match x with
    | Z0 -> (match y with
             | Z0 -> true
             | _ -> false)
    | Zpos p -> (match y with
                 | Zpos q -> Pos.eqb p q
                 | _ -> false)
    | Zneg p -> (match y with
                 | Zneg q -> Pos.eqb p q
                 | _ -> false)

  (** val to_nat : z -> int **)

  let to_nat = function
  | Zpos p -> Pos.to_nat p
  | _ -> 0

  (** val of_nat : int -> z **)

  let of_nat n =
    (fun fO fS n -> if n=0 then fO () else fS (n-1))
      (fun _ -> Z0)
      (fun n0 -> Zpos (Pos.of_succ_nat n0))
      n

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

  (** val div2 : z -> z **)

  let div2 = function
  | Z0 -> Z0
  | Zpos p -> (match p with
               | XH -> Z0
               | _ -> Zpos (Pos.div2 p))
  | Zneg p -> Zneg (Pos.div2_up p)

  (** val shiftl : z -> z -> z **)

  let shiftl a = function
  | Z0 -> a
  | Zpos p -> Pos.iter (mul (Zpos (XO XH))) a p
  | Zneg p -> Pos.iter div2 a p

  (** val shiftr : z -> z -> z **)

  let shiftr a n =
    shiftl a (opp n)
 end

(** val cOIN : z **)

let cOIN =
  Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XI (XO (XO (XO (XO (XI (XI (XI (XI
    (XO (XI (XO (XI (XI (XI (XI (XI (XO XH))))))))))))))))))))))))))

(** val rES_DESTROY_COST_GENESIS : z **)

let rES_DESTROY_COST_GENESIS =
  Z.mul (Zpos (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))) cOIN

(** val rES_DESTROY_POLE : z **)

let rES_DESTROY_POLE =
  Z.shiftl (Zpos XH) (Zpos (XI (XO (XO (XI XH)))))

(** val oRU_FLAT_BLOCK_SUBSIDY : z **)

let oRU_FLAT_BLOCK_SUBSIDY =
  Z.mul (Zpos (XO (XO (XO (XO (XO (XO (XO XH)))))))) cOIN

(** val res_destroy_pole_sq : z -> z **)

let res_destroy_pole_sq h =
  Z.mul (Z.add rES_DESTROY_POLE h) (Z.add rES_DESTROY_POLE h)

(** val res_destroy_cost_at_height : z -> z **)

let res_destroy_cost_at_height height =
  if Z.ltb height Z0
  then Z0
  else Z.shiftr (Z.mul rES_DESTROY_COST_GENESIS (res_destroy_pole_sq height))
         (Zpos (XO (XI (XO (XO (XI XH))))))

(** val coinbase_oru_cap : z -> z -> z -> z **)

let coinbase_oru_cap fees_oru burn_acc subsidy =
  Z.add (Z.sub fees_oru burn_acc) subsidy

(** val cALENDAR_INTERVAL : z **)

let cALENDAR_INTERVAL =
  Zpos (XO (XO (XO (XO (XI (XO (XI (XO (XO (XO (XI (XO (XI (XI (XO (XO (XI
    XH)))))))))))))))))

(** val oRU_FLAT_SATS : z **)

let oRU_FLAT_SATS =
  oRU_FLAT_BLOCK_SUBSIDY

(** val issuance_epoch : z -> z **)

let issuance_epoch h =
  if Z.ltb h Z0 then Z0 else Z.div h cALENDAR_INTERVAL

(** val oru_subsidy : z -> z **)

let oru_subsidy h =
  if Z.leb h Z0 then Z0 else oRU_FLAT_SATS

type byte = int

(** val app_bytes : byte list -> byte list -> byte list **)

let rec app_bytes xs ys =
  match xs with
  | [] -> ys
  | x :: xrest -> x :: (app_bytes xrest ys)

(** val split_at : int -> byte list -> (byte list * byte list) option **)

let rec split_at n xs =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> Some ([], xs))
    (fun n' ->
    match xs with
    | [] -> None
    | x :: xs' ->
      (match split_at n' xs' with
       | Some p -> let (pre, post) = p in Some ((x :: pre), post)
       | None -> None))
    n

(** val rIGOR_PUSH_0 : byte **)

let rIGOR_PUSH_0 =
  0

(** val rIGOR_PUSH_1 : byte **)

let rIGOR_PUSH_1 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSH_16 : byte **)

let rIGOR_PUSH_16 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_RETURN : byte **)

let rIGOR_MARKER_RETURN =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHBYTES_20 : byte **)

let rIGOR_PUSHBYTES_20 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))

(** val rIGOR_PUSHDATA1 : byte **)

let rIGOR_PUSHDATA1 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHDATA2 : byte **)

let rIGOR_PUSHDATA2 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_PUSHDATA4 : byte **)

let rIGOR_PUSHDATA4 =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_RES_MINT : byte **)

let rIGOR_MARKER_RES_MINT =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_MARKER_CONVERGE : byte **)

let rIGOR_MARKER_CONVERGE =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val rIGOR_RESERVED_LEGACY_1NEGATE : byte **)

let rIGOR_RESERVED_LEGACY_1NEGATE =
  Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val cvg2_tag : byte list **)

let cvg2_tag =
  (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))) :: [])))

(** val res2_tag : byte list **)

let res2_tag =
  (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ
    0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: ((Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ
    0)))))))))))))))))))))))))))))))))))))))))))))))))) :: [])))

type assetType =
| AssetORU
| AssetRES
| AssetNone
| AssetInvalid

(** val witness_version_of : byte -> int option **)

let witness_version_of b =
  if (=) b rIGOR_PUSH_0
  then Some 0
  else if (&&) ((<=) rIGOR_PUSH_1 b) ((<=) b rIGOR_PUSH_16)
       then Some
              (sub b (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ (Pervasives.succ
                0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
       else None

(** val parse_witness_program : byte list -> (int * byte list) option **)

let parse_witness_program spk =
  let n = length spk in
  if (||)
       (Nat.ltb n (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ 0)))))
       (Nat.ltb (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
         (Pervasives.succ (Pervasives.succ (Pervasives.succ
         0)))))))))))))))))))))))))))))))))))))))))) n)
  then None
  else (match spk with
        | [] -> None
        | ver :: l ->
          (match l with
           | [] -> None
           | plen :: rest ->
             (match witness_version_of ver with
              | Some v ->
                if (&&) (Nat.ltb 0 plen) ((=) (length rest) plen)
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
                            (&&) ((=) a x)
                              ((&&) ((=) b y) ((&&) ((=) c z0) ((=) d w)))
                          | _ :: _ -> false))))))))

(** val is_native_res_program : byte list -> bool **)

let is_native_res_program prog =
  (&&)
    ((=) (length prog) (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ 0)))))))))))))))))))))))))))))))))
    ((&&) (negb (prefix4_eq prog cvg2_tag)) (negb (prefix4_eq prog res2_tag)))

(** val pos_le_bytes : z -> int -> byte list **)

let rec pos_le_bytes n fuel =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> [])
    (fun f ->
    if Z.eqb n Z0
    then []
    else (Z.to_nat
           (Z.modulo n (Zpos (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))) :: 
           (pos_le_bytes
             (Z.div n (Zpos (XO (XO (XO (XO (XO (XO (XO (XO XH)))))))))) f))
    fuel

(** val scriptnum_payload : z -> byte list **)

let scriptnum_payload n =
  let raw =
    pos_le_bytes n (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ 0)))))
  in
  (match rev raw with
   | [] -> []
   | last :: _ ->
     if (<=) (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
          (Pervasives.succ
          0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
          last
     then app_bytes raw (0 :: [])
     else raw)

(** val encode_scriptnum : z -> byte list **)

let encode_scriptnum n =
  if Z.ltb n Z0
  then rIGOR_RESERVED_LEGACY_1NEGATE :: []
  else let p = scriptnum_payload n in
       (match p with
        | [] -> rIGOR_PUSH_0 :: []
        | _ :: _ -> (length p) :: p)

(** val le_to_Z : byte list -> z **)

let rec le_to_Z = function
| [] -> Z0
| b :: rest ->
  Z.add (Z.of_nat b)
    (Z.mul (Zpos (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))) (le_to_Z rest))

(** val scriptnum_to_Z : byte list -> z **)

let scriptnum_to_Z payload =
  match rev payload with
  | [] -> le_to_Z payload
  | b :: rest ->
    ((fun fO fS n -> if n=0 then fO () else fS (n-1))
       (fun _ -> le_to_Z (rev rest))
       (fun _ -> le_to_Z payload)
       b)

(** val decode_scriptnum_prefix : byte list -> (z * byte list) option **)

let decode_scriptnum_prefix = function
| [] -> None
| b :: r ->
  if (=) b rIGOR_PUSH_0
  then Some (Z0, r)
  else if (&&) ((<=) (Pervasives.succ 0) b)
            ((<=) b (Pervasives.succ (Pervasives.succ (Pervasives.succ
              (Pervasives.succ 0)))))
       then (match split_at b r with
             | Some p ->
               let (payload, rest) = p in
               let v = scriptnum_to_Z payload in
               if list_eq_dec (=) (encode_scriptnum v) (b :: payload)
               then Some (v, rest)
               else None
             | None -> None)
       else None

(** val parse_canonical_mint : byte list -> (z * byte list) option **)

let parse_canonical_mint spk =
  match decode_scriptnum_prefix spk with
  | Some p ->
    let (maturity, rest) = p in
    (match rest with
     | [] -> None
     | plen :: body ->
       if (=) plen rIGOR_PUSHBYTES_20
       then (match split_at (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     (Pervasives.succ (Pervasives.succ (Pervasives.succ
                     0)))))))))))))))))))) body with
             | Some p0 ->
               let (pkh, marker_tl) = p0 in
               (match marker_tl with
                | [] -> None
                | op :: l ->
                  (match l with
                   | [] ->
                     if (=) op rIGOR_MARKER_RES_MINT
                     then Some (maturity, pkh)
                     else None
                   | _ :: _ -> None))
             | None -> None)
       else None)
  | None -> None

(** val is_res_mint_script : byte list -> bool **)

let is_res_mint_script spk =
  match parse_canonical_mint spk with
  | Some _ -> true
  | None -> false

(** val is_converge_script : byte list -> bool **)

let is_converge_script = function
| [] -> false
| plen :: rest ->
  (&&) ((=) plen rIGOR_PUSHBYTES_20)
    ((&&)
      ((=) (length rest) (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ 0))))))))))))))))))))))
      (match rev rest with
       | [] -> false
       | op :: hash_rev ->
         (&&) ((=) op rIGOR_MARKER_CONVERGE)
           ((=) (length hash_rev) (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             (Pervasives.succ (Pervasives.succ (Pervasives.succ
             0)))))))))))))))))))))))

(** val is_push_only_n : int -> byte list -> bool **)

let rec is_push_only_n fuel rest =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> match rest with
              | [] -> true
              | _ :: _ -> false)
    (fun fuel' ->
    match rest with
    | [] -> true
    | op :: tail ->
      if (<=) op (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
      then (match split_at op tail with
            | Some p -> let (_, rest') = p in is_push_only_n fuel' rest'
            | None -> false)
      else if (=) op rIGOR_PUSHDATA1
           then (match tail with
                 | [] -> false
                 | ln :: data ->
                   (match split_at ln data with
                    | Some p ->
                      let (_, rest') = p in is_push_only_n fuel' rest'
                    | None -> false))
           else if (=) op rIGOR_PUSHDATA2
                then (match tail with
                      | [] -> false
                      | lo :: l ->
                        (match l with
                         | [] -> false
                         | hi :: data ->
                           (match split_at
                                    (add lo
                                      (mul hi (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ (Pervasives.succ
                                        (Pervasives.succ
                                        0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                    data with
                            | Some p ->
                              let (_, rest') = p in is_push_only_n fuel' rest'
                            | None -> false)))
                else if (=) op rIGOR_PUSHDATA4
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
                                                     (mul b (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                   (mul
                                                     (mul c (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                 (mul
                                                   (mul
                                                     (mul d (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       (Pervasives.succ
                                                       0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     (Pervasives.succ
                                                     0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   (Pervasives.succ
                                                   0))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))
                                               data with
                                       | Some p ->
                                         let (_, rest') = p in
                                         is_push_only_n fuel' rest'
                                       | None -> false)))))
                     else if (<=) op rIGOR_PUSH_16
                          then is_push_only_n fuel' tail
                          else false)
    fuel

(** val is_push_only : byte list -> bool **)

let is_push_only rest =
  is_push_only_n (length rest) rest

(** val is_op_return : byte list -> bool **)

let is_op_return = function
| [] -> false
| x :: rest -> (&&) ((=) x rIGOR_MARKER_RETURN) (is_push_only rest)

(** val classify : byte list -> assetType **)

let classify spk =
  match parse_witness_program spk with
  | Some p ->
    let (n, prog) = p in
    ((fun fO fS n -> if n=0 then fO () else fS (n-1))
       (fun _ ->
       if (=) (length prog) (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            (Pervasives.succ (Pervasives.succ (Pervasives.succ
            0))))))))))))))))))))
       then AssetORU
       else AssetInvalid)
       (fun n0 ->
       (fun fO fS n -> if n=0 then fO () else fS (n-1))
         (fun _ ->
         if is_native_res_program prog then AssetRES else AssetInvalid)
         (fun _ -> AssetInvalid)
         n0)
       n)
  | None ->
    if is_res_mint_script spk
    then AssetRES
    else if is_converge_script spk
         then AssetORU
         else if is_op_return spk then AssetNone else AssetInvalid

(** val compute_rigor_issuance : z -> (z * z) * z **)

let compute_rigor_issuance h =
  (((issuance_epoch h), (oru_subsidy h)), (res_destroy_cost_at_height h))

(** val rigor_res_coinbase_expected : z -> z -> z **)

let rigor_res_coinbase_expected =
  Z.add

(** val ledger_oru_delta : z -> z -> z **)

let ledger_oru_delta =
  Z.sub

(** val cOINBASE_MATURITY : z **)

let cOINBASE_MATURITY =
  Zpos (XO (XO (XI (XO (XO (XI XH))))))

(** val coinbase_mature : z -> z -> bool **)

let coinbase_mature spend_height coin_height =
  Z.geb (Z.sub spend_height coin_height) cOINBASE_MATURITY

(** val res_issued_at_height : z -> z **)

let res_issued_at_height _ =
  Z0
