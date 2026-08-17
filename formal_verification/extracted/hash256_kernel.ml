
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

(** val pred : int -> int **)

let pred = fun n -> Pervasives.max 0 (n-1)

module Coq__1 = struct
 (** val add : int -> int -> int **)let rec add = (+)
end
include Coq__1

(** val sub : int -> int -> int **)

let rec sub = fun n m -> Pervasives.max 0 (n-m)

module Nat =
 struct
  (** val ltb : int -> int -> bool **)

  let ltb n0 m =
    (<=) (Pervasives.succ n0) m

  (** val min : int -> int -> int **)

  let rec min n0 m =
    (fun fO fS n -> if n=0 then fO () else fS (n-1))
      (fun _ -> 0)
      (fun n' ->
      (fun fO fS n -> if n=0 then fO () else fS (n-1))
        (fun _ -> 0)
        (fun m' -> Pervasives.succ (min n' m'))
        m)
      n0
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

  (** val coq_Nsucc_double : n -> n **)

  let coq_Nsucc_double = function
  | N0 -> Npos XH
  | Npos p -> Npos (XI p)

  (** val coq_Ndouble : n -> n **)

  let coq_Ndouble = function
  | N0 -> N0
  | Npos p -> Npos (XO p)

  (** val coq_lor : positive -> positive -> positive **)

  let rec coq_lor p q =
    match p with
    | XI p0 ->
      (match q with
       | XI q0 -> XI (coq_lor p0 q0)
       | XO q0 -> XI (coq_lor p0 q0)
       | XH -> p)
    | XO p0 ->
      (match q with
       | XI q0 -> XI (coq_lor p0 q0)
       | XO q0 -> XO (coq_lor p0 q0)
       | XH -> XI p0)
    | XH -> (match q with
             | XO q0 -> XI q0
             | _ -> q)

  (** val coq_land : positive -> positive -> n **)

  let rec coq_land p q =
    match p with
    | XI p0 ->
      (match q with
       | XI q0 -> coq_Nsucc_double (coq_land p0 q0)
       | XO q0 -> coq_Ndouble (coq_land p0 q0)
       | XH -> Npos XH)
    | XO p0 ->
      (match q with
       | XI q0 -> coq_Ndouble (coq_land p0 q0)
       | XO q0 -> coq_Ndouble (coq_land p0 q0)
       | XH -> N0)
    | XH -> (match q with
             | XO _ -> N0
             | _ -> Npos XH)

  (** val ldiff : positive -> positive -> n **)

  let rec ldiff p q =
    match p with
    | XI p0 ->
      (match q with
       | XI q0 -> coq_Ndouble (ldiff p0 q0)
       | XO q0 -> coq_Nsucc_double (ldiff p0 q0)
       | XH -> Npos (XO p0))
    | XO p0 ->
      (match q with
       | XI q0 -> coq_Ndouble (ldiff p0 q0)
       | XO q0 -> coq_Ndouble (ldiff p0 q0)
       | XH -> Npos p)
    | XH -> (match q with
             | XO _ -> Npos XH
             | _ -> N0)

  (** val coq_lxor : positive -> positive -> n **)

  let rec coq_lxor p q =
    match p with
    | XI p0 ->
      (match q with
       | XI q0 -> coq_Ndouble (coq_lxor p0 q0)
       | XO q0 -> coq_Nsucc_double (coq_lxor p0 q0)
       | XH -> Npos (XO p0))
    | XO p0 ->
      (match q with
       | XI q0 -> coq_Nsucc_double (coq_lxor p0 q0)
       | XO q0 -> coq_Ndouble (coq_lxor p0 q0)
       | XH -> Npos (XI p0))
    | XH ->
      (match q with
       | XI q0 -> Npos (XO q0)
       | XO q0 -> Npos (XI q0)
       | XH -> N0)

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

  let rec of_succ_nat n0 =
    (fun fO fS n -> if n=0 then fO () else fS (n-1))
      (fun _ -> XH)
      (fun x -> succ (of_succ_nat x))
      n0
 end

module N =
 struct
  (** val succ_pos : n -> positive **)

  let succ_pos = function
  | N0 -> XH
  | Npos p -> Pos.succ p

  (** val coq_lor : n -> n -> n **)

  let coq_lor n0 m =
    match n0 with
    | N0 -> m
    | Npos p -> (match m with
                 | N0 -> n0
                 | Npos q -> Npos (Pos.coq_lor p q))

  (** val coq_land : n -> n -> n **)

  let coq_land n0 m =
    match n0 with
    | N0 -> N0
    | Npos p -> (match m with
                 | N0 -> N0
                 | Npos q -> Pos.coq_land p q)

  (** val ldiff : n -> n -> n **)

  let ldiff n0 m =
    match n0 with
    | N0 -> N0
    | Npos p -> (match m with
                 | N0 -> n0
                 | Npos q -> Pos.ldiff p q)

  (** val coq_lxor : n -> n -> n **)

  let coq_lxor n0 m =
    match n0 with
    | N0 -> m
    | Npos p -> (match m with
                 | N0 -> n0
                 | Npos q -> Pos.coq_lxor p q)
 end

(** val rev : 'a1 list -> 'a1 list **)

let rec rev = function
| [] -> []
| x :: l' -> app (rev l') (x :: [])

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

  (** val pred : z -> z **)

  let pred x =
    add x (Zneg XH)

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

  (** val to_nat : z -> int **)

  let to_nat = function
  | Zpos p -> Pos.to_nat p
  | _ -> 0

  (** val of_nat : int -> z **)

  let of_nat n0 =
    (fun fO fS n -> if n=0 then fO () else fS (n-1))
      (fun _ -> Z0)
      (fun n1 -> Zpos (Pos.of_succ_nat n1))
      n0

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

  let shiftr a n0 =
    shiftl a (opp n0)

  (** val coq_lor : z -> z -> z **)

  let coq_lor a b =
    match a with
    | Z0 -> b
    | Zpos a0 ->
      (match b with
       | Z0 -> a
       | Zpos b0 -> Zpos (Pos.coq_lor a0 b0)
       | Zneg b0 -> Zneg (N.succ_pos (N.ldiff (Pos.pred_N b0) (Npos a0))))
    | Zneg a0 ->
      (match b with
       | Z0 -> a
       | Zpos b0 -> Zneg (N.succ_pos (N.ldiff (Pos.pred_N a0) (Npos b0)))
       | Zneg b0 ->
         Zneg (N.succ_pos (N.coq_land (Pos.pred_N a0) (Pos.pred_N b0))))

  (** val coq_land : z -> z -> z **)

  let coq_land a b =
    match a with
    | Z0 -> Z0
    | Zpos a0 ->
      (match b with
       | Z0 -> Z0
       | Zpos b0 -> of_N (Pos.coq_land a0 b0)
       | Zneg b0 -> of_N (N.ldiff (Npos a0) (Pos.pred_N b0)))
    | Zneg a0 ->
      (match b with
       | Z0 -> Z0
       | Zpos b0 -> of_N (N.ldiff (Npos b0) (Pos.pred_N a0))
       | Zneg b0 ->
         Zneg (N.succ_pos (N.coq_lor (Pos.pred_N a0) (Pos.pred_N b0))))

  (** val coq_lxor : z -> z -> z **)

  let coq_lxor a b =
    match a with
    | Z0 -> b
    | Zpos a0 ->
      (match b with
       | Z0 -> a
       | Zpos b0 -> of_N (Pos.coq_lxor a0 b0)
       | Zneg b0 -> Zneg (N.succ_pos (N.coq_lxor (Npos a0) (Pos.pred_N b0))))
    | Zneg a0 ->
      (match b with
       | Z0 -> a
       | Zpos b0 -> Zneg (N.succ_pos (N.coq_lxor (Pos.pred_N a0) (Npos b0)))
       | Zneg b0 -> of_N (N.coq_lxor (Pos.pred_N a0) (Pos.pred_N b0)))

  (** val lnot : z -> z **)

  let lnot a =
    pred (opp a)
 end

type byte = int

(** val w32 : z -> z **)

let w32 z0 =
  Z.modulo z0 (Z.shiftl (Zpos XH) (Zpos (XO (XO (XO (XO (XO XH)))))))

(** val w32_add : z -> z -> z **)

let w32_add a b =
  w32 (Z.add a b)

(** val w32_xor : z -> z -> z **)

let w32_xor a b =
  w32 (Z.coq_lxor a b)

(** val w32_and : z -> z -> z **)

let w32_and a b =
  w32 (Z.coq_land a b)

(** val w32_not : z -> z **)

let w32_not a =
  w32 (Z.lnot a)

(** val rotr32 : z -> int -> z **)

let rotr32 x n0 =
  let nZ =
    Z.of_nat
      (Nat.min n0 (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ 0)))))))))))))))))))))))))))))))))
  in
  w32
    (Z.coq_lor (Z.shiftr x nZ)
      (Z.shiftl x (Z.sub (Zpos (XO (XO (XO (XO (XO XH)))))) nZ)))

(** val shr32 : z -> int -> z **)

let shr32 x n0 =
  Z.shiftr x
    (Z.of_nat
      (Nat.min n0 (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ 0))))))))))))))))))))))))))))))))))

(** val ch : z -> z -> z -> z **)

let ch x y z0 =
  w32_xor (w32_and x y) (w32_and (w32_not x) z0)

(** val maj : z -> z -> z -> z **)

let maj x y z0 =
  w32_xor (w32_xor (w32_and x y) (w32_and x z0)) (w32_and y z0)

(** val bsig0 : z -> z **)

let bsig0 x =
  w32_xor
    (w32_xor (rotr32 x (Pervasives.succ (Pervasives.succ 0)))
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ 0)))))))))))))))
    (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ
      0)))))))))))))))))))))))

(** val bsig1 : z -> z **)

let bsig1 x =
  w32_xor
    (w32_xor
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ 0)))))))
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        0)))))))))))))
    (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ 0))))))))))))))))))))))))))

(** val ssig0 : z -> z **)

let ssig0 x =
  w32_xor
    (w32_xor
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        0))))))))
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ
        0))))))))))))))))))))
    (shr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ 0))))

(** val ssig1 : z -> z **)

let ssig1 x =
  w32_xor
    (w32_xor
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ 0))))))))))))))))))
      (rotr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
        0)))))))))))))))))))))
    (shr32 x (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ 0)))))))))))

(** val k256 : z list **)

let k256 =
  (Zpos (XO (XO (XO (XI (XI (XO (XO (XI (XI (XI (XI (XI (XO (XI (XO (XO (XO
    (XI (XO (XI (XO (XO (XO (XI (XO (XI (XO (XO (XO (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XO (XO (XI (XO (XO
    (XI (XO (XO (XI (XO (XO (XO (XI (XO (XI (XI (XI (XO (XI (XI (XO (XO (XI
    (XO (XO (XO (XI (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI
    (XI (XI (XO (XO (XI (XI (XI (XI (XO (XI (XI (XI (XI (XI (XO (XO (XO (XO
    (XO (XO (XI (XI (XI (XO (XI (XO (XI (XI (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XO (XI (XO
    (XI (XI (XI (XO (XI (XI (XO (XI (XI (XI (XO (XI (XO (XI (XI (XO (XI (XI
    (XO (XO (XI (XO (XI (XI XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XO (XI (XI (XO (XI (XO (XO (XI (XO (XO (XO (XO (XI (XI (XO (XI (XI
    (XO (XI (XO (XI (XO (XI (XO (XO (XI (XI
    XH)))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XO (XO (XI (XI (XI
    (XI (XI (XO (XO (XO (XI (XO (XO (XO (XI (XO (XO (XO (XI (XI (XI (XI (XI
    (XO (XO (XI (XI (XO XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO
    (XI (XO (XO (XI (XO (XI (XO (XI (XO (XO (XO (XO (XO (XI (XI (XI (XI (XI
    (XI (XI (XO (XO (XO (XI (XO (XO (XI (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XI (XO (XI
    (XI (XO (XI (XI (XI (XI (XO (XI (XO (XO (XO (XI (XI (XI (XO (XO (XO (XI
    (XI (XO (XI (XO (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XO (XO (XI (XI (XO (XO (XI (XO (XI (XO (XI (XO (XI (XO (XI (XI (XI (XI
    (XO (XO (XO (XO (XO (XO (XO (XO (XI (XI (XO (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XO (XO (XO (XO (XO
    (XO (XI (XI (XO (XI (XI (XO (XI (XO (XI (XI (XO (XO (XO (XO (XO (XI (XO
    (XI (XO (XO XH))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XI (XI (XI
    (XI (XO (XI (XI (XO (XI (XO (XO (XO (XO (XI (XI (XO (XO (XO (XI (XI (XO
    (XO (XO (XO (XI (XO (XO XH)))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XO (XO (XO (XO (XI (XI (XI (XO (XI (XI (XI (XI (XI (XO (XO (XO (XI
    (XI (XO (XO (XO (XO (XI (XO (XI (XO (XI (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XI (XO (XI (XI (XI
    (XO (XI (XO (XI (XI (XI (XO (XI (XO (XO (XI (XI (XI (XI (XI (XO (XI (XO
    (XI (XO (XO (XI (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI
    (XI (XI (XI (XI (XI (XI (XI (XO (XO (XO (XI (XI (XO (XI (XO (XI (XI (XI
    (XI (XO (XI (XI (XO (XO (XO (XO (XO (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XI (XO (XO (XI (XO
    (XI (XO (XI (XI (XO (XO (XO (XO (XO (XO (XO (XI (XI (XI (XO (XI (XI (XI
    (XI (XO (XI (XI (XO (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XO (XI (XO (XI (XI (XI (XO (XI (XO (XO (XO (XI (XI (XI (XI (XI (XI (XO
    (XI (XI (XO (XO (XI (XI (XO (XO (XO (XO (XO (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XO (XO (XO (XO (XI
    (XI (XI (XO (XO (XI (XO (XI (XI (XO (XI (XI (XO (XI (XI (XO (XO (XI (XO
    (XO (XI (XO (XO (XI (XI XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XI (XI (XO (XO (XO (XO (XI (XI (XI (XI (XO (XO (XO (XI (XO (XO (XI (XI
    (XI (XI (XI (XO (XI (XI (XI (XI (XI (XO (XI (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XI (XO (XO (XO (XI
    (XI (XI (XO (XI (XI (XI (XO (XO (XI (XI (XO (XO (XO (XO (XO (XI (XI (XI
    (XI (XI XH)))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XI (XI (XO (XO
    (XI (XI (XI (XO (XO (XO (XO (XI (XO (XI (XO (XO (XI (XI (XO (XO (XO (XO
    (XO (XO (XI (XO (XO XH)))))))))))))))))))))))))))))) :: ((Zpos (XI (XI
    (XI (XI (XO (XI (XI (XO (XO (XO (XI (XI (XO (XI (XO (XO (XI (XO (XO (XI
    (XO (XI (XI (XI (XI (XO (XI (XI (XO
    XH)))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XO (XI (XO (XI (XO
    (XI (XO (XO (XI (XO (XO (XO (XO (XI (XO (XO (XI (XO (XI (XI (XI (XO (XO
    (XI (XO (XI (XO (XO XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO
    (XI (XI (XI (XO (XI (XI (XI (XO (XO (XI (XO (XI (XO (XI (XO (XO (XO (XO
    (XI (XI (XO (XI (XO (XO (XI (XI (XI (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XO (XI (XI (XO (XI
    (XI (XO (XO (XO (XI (XO (XO (XO (XI (XI (XO (XO (XI (XI (XI (XI (XI (XO
    (XI (XI (XO (XI (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI
    (XO (XO (XI (XO (XI (XO (XI (XO (XO (XO (XI (XO (XI (XO (XO (XI (XI (XI
    (XI (XI (XO (XO (XO (XO (XO (XI (XI (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XI (XO (XI (XI
    (XO (XO (XI (XI (XO (XO (XO (XI (XI (XI (XO (XO (XO (XI (XI (XO (XO (XO
    (XO (XO (XI (XO (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XO (XO (XI (XO (XO (XI (XI (XI (XI (XI (XO (XO (XI (XO (XO (XI (XI (XO
    (XO (XO (XO (XO (XO (XO (XO (XO (XO (XI (XI (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XI (XO (XO (XO (XI
    (XI (XI (XI (XI (XI (XI (XI (XI (XO (XI (XO (XO (XI (XI (XO (XI (XO (XI
    (XI (XI (XI (XI (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XO (XO (XI (XI (XI (XI (XI (XI (XO (XI (XO (XO (XO (XO (XO (XO (XO
    (XO (XO (XI (XI (XI (XO (XI (XI (XO (XO (XO (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XI (XO (XO (XO (XI
    (XO (XI (XO (XO (XO (XI (XO (XO (XI (XI (XI (XI (XO (XO (XI (XO (XI (XI
    (XO (XI (XO (XI (XO (XI XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XO (XO (XO (XI (XO (XI (XO (XI (XI (XO (XO (XO (XI (XI (XO (XO (XI (XO
    (XI (XO (XO (XI (XI (XO (XI XH))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XI (XO (XO (XI (XI (XO (XI (XO (XO (XI (XO (XI (XO (XO (XI (XO (XO
    (XI (XO (XI (XO (XO (XO (XO (XI (XO
    XH))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XO (XO (XO (XI
    (XO (XI (XO (XI (XO (XO (XO (XO (XI (XI (XI (XO (XI (XI (XO (XI (XI (XI
    (XI (XO (XO XH)))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XO (XI
    (XI (XI (XO (XO (XI (XO (XO (XO (XO (XI (XO (XO (XI (XI (XO (XI (XI (XO
    (XO (XO (XO (XI (XI (XI (XO XH)))))))))))))))))))))))))))))) :: ((Zpos
    (XO (XO (XI (XI (XI (XI (XI (XI (XI (XO (XI (XI (XO (XI (XI (XO (XO (XO
    (XI (XI (XO (XI (XO (XO (XI (XO (XI (XI (XO (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XO (XI (XO (XO
    (XO (XI (XO (XI (XI (XO (XO (XO (XO (XO (XO (XO (XI (XI (XI (XO (XO (XI
    (XI (XO (XO (XI (XO XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO
    (XI (XO (XI (XO (XI (XO (XI (XI (XO (XO (XI (XI (XI (XO (XO (XI (XO (XI
    (XO (XO (XO (XO (XI (XO (XI (XO (XO (XI
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XI (XI (XI (XO
    (XI (XO (XI (XO (XI (XO (XO (XO (XO (XO (XI (XO (XI (XO (XI (XI (XO (XO
    (XI (XI (XO (XI (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI
    (XI (XI (XO (XI (XO (XO (XI (XO (XO (XI (XO (XO (XI (XI (XO (XI (XO (XO
    (XO (XO (XI (XI (XI (XO (XO (XO (XO (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XO (XO (XO
    (XI (XO (XO (XI (XI (XO (XI (XO (XO (XO (XI (XO (XO (XI (XI (XI (XO (XO
    (XI (XO (XO (XI (XO (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XO (XO (XO (XO (XI (XO (XI (XO (XO (XO (XI (XO (XI (XI (XI (XI (XI (XI
    (XI (XI (XI (XO (XI (XO (XI (XO (XO (XO (XI (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XI (XO (XO (XI
    (XO (XO (XI (XI (XO (XO (XI (XI (XO (XO (XI (XO (XI (XI (XO (XO (XO (XO
    (XO (XO (XI (XO (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XO (XO (XO (XI (XI (XI (XO (XI (XI (XO (XI (XO (XO (XO (XI (XI (XI (XO
    (XI (XO (XO (XI (XO (XO (XI (XO (XO (XO (XO (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XO (XO (XI (XO
    (XI (XI (XO (XO (XO (XI (XO (XI (XO (XO (XO (XI (XI (XO (XI (XI (XO (XI
    (XI (XI (XO (XO (XO (XI XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XO (XO (XI (XI (XO (XO (XO (XO (XO (XO (XI (XO (XI (XI (XI (XO (XI (XO
    (XO (XI (XO (XO (XI (XI (XO (XO (XO (XI (XO (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XI (XO (XO (XI (XO
    (XO (XO (XI (XI (XO (XO (XO (XO (XO (XI (XO (XO (XI (XI (XO (XO (XI (XO
    (XI (XI (XO (XI (XO (XI XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XO (XI (XO (XO (XO (XO (XI (XI (XO (XI (XO (XI (XI (XO (XO (XO (XI (XI
    (XI (XO (XO (XO (XO (XO (XO (XI (XO (XI (XI (XI
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XO (XO (XI (XI (XI
    (XO (XO (XO (XO (XO (XO (XI (XO (XI (XO (XI (XO (XI (XO (XI (XI (XO (XO
    (XO (XO (XO XH))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XI (XO (XI
    (XO (XO (XO (XI (XO (XO (XO (XO (XO (XI (XI (XO (XO (XI (XO (XO (XI (XO
    (XI (XI (XO (XO (XI XH))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XO
    (XI (XO (XO (XO (XO (XO (XO (XI (XI (XO (XI (XI (XO (XI (XI (XI (XO (XI
    (XI (XO (XO (XO (XI (XI (XI XH))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XO (XI (XI (XO (XO (XI (XO (XI (XI (XI (XO (XI (XI (XI (XO (XO (XO (XO
    (XI (XO (XO (XI (XO (XI (XI (XI (XO (XO
    XH)))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XI (XI (XO
    (XI (XO (XO (XI (XI (XI (XI (XO (XI (XO (XO (XO (XO (XI (XI (XO (XI (XO
    (XO (XI (XO (XI XH)))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO
    (XO (XI (XI (XO (XI (XO (XO (XI (XI (XO (XO (XO (XO (XO (XO (XI (XI (XI
    (XO (XO (XO (XI (XO (XO (XI (XI
    XH)))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XO (XI (XO (XO (XI
    (XO (XO (XI (XO (XI (XO (XI (XO (XI (XO (XO (XO (XI (XI (XO (XI (XI (XO
    (XI (XI (XI (XO (XO XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI
    (XI (XI (XO (XO (XI (XO (XO (XI (XO (XI (XO (XO (XI (XI (XO (XO (XI (XI
    (XI (XO (XO (XI (XI (XI (XO (XI (XI (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XO (XI (XI (XI
    (XI (XI (XI (XI (XI (XO (XI (XI (XO (XO (XI (XI (XI (XO (XI (XO (XO (XO
    (XO (XO (XI (XO (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI
    (XI (XI (XO (XI (XI (XI (XO (XI (XO (XO (XO (XO (XO (XI (XI (XI (XI (XI
    (XO (XO (XO (XI (XO (XO (XI (XO (XI (XI
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XI (XI (XO (XI (XI
    (XO (XI (XI (XO (XO (XO (XI (XI (XO (XI (XO (XI (XO (XO (XI (XO (XI (XO
    (XO (XO (XI (XI (XI XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO
    (XI (XO (XI (XO (XO (XO (XO (XO (XO (XI (XI (XI (XI (XO (XO (XO (XO (XI
    (XO (XO (XI (XI (XO (XO (XI (XO (XO (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XO (XI (XO (XO (XO
    (XO (XO (XI (XO (XO (XO (XO (XO (XO (XI (XI (XI (XO (XO (XO (XI (XI (XO
    (XO (XI (XI (XO (XO (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XI (XO (XI (XI (XI (XI (XI (XI (XI (XI (XI (XI (XI (XI (XI (XO (XI (XI
    (XI (XI (XI (XO (XI (XO (XO (XO (XO (XI (XO (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI (XI (XO (XI (XO (XI (XI
    (XI (XO (XO (XI (XI (XO (XI (XI (XO (XO (XO (XO (XO (XI (XO (XI (XO (XO
    (XO (XI (XO (XO (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XI (XO (XI (XI (XI (XI (XI (XI (XO (XO (XO (XI (XO (XI (XI (XO (XO
    (XI (XI (XI (XI (XI (XO (XI (XI (XI (XI (XI (XO
    XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XO (XO (XI (XI (XI
    (XI (XO (XO (XO (XI (XI (XI (XI (XO (XI (XO (XO (XO (XI (XI (XI (XO (XO
    (XI (XI (XO (XO (XO (XI
    XH)))))))))))))))))))))))))))))))) :: [])))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))

(** val h256_init : z list **)

let h256_init =
  (Zpos (XI (XI (XI (XO (XO (XI (XI (XO (XO (XI (XI (XO (XO (XI (XI (XI (XI
    (XO (XO (XI (XO (XO (XO (XO (XO (XI (XO (XI (XO (XI
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XI (XO (XO (XO (XO
    (XI (XO (XI (XI (XI (XO (XI (XO (XI (XI (XI (XI (XO (XO (XI (XI (XO (XI
    (XI (XO (XI (XI (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XO
    (XI (XO (XO (XI (XI (XI (XO (XI (XI (XO (XO (XI (XI (XI (XI (XO (XI (XI
    (XI (XO (XI (XI (XO (XO (XO (XI (XI (XI
    XH)))))))))))))))))))))))))))))) :: ((Zpos (XO (XI (XO (XI (XI (XI (XO
    (XO (XI (XO (XI (XO (XI (XI (XI (XI (XI (XI (XI (XI (XO (XO (XI (XO (XI
    (XO (XI (XO (XO (XI (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XI (XI (XI (XI (XI (XO (XO (XI (XO (XO (XI (XO (XI (XO (XO (XI (XI
    (XI (XO (XO (XO (XO (XI (XO (XO (XO (XI (XO
    XH))))))))))))))))))))))))))))))) :: ((Zpos (XO (XO (XI (XI (XO (XO (XO
    (XI (XO (XO (XO (XI (XO (XI (XI (XO (XI (XO (XI (XO (XO (XO (XO (XO (XI
    (XI (XO (XI (XI (XO (XO XH)))))))))))))))))))))))))))))))) :: ((Zpos (XI
    (XI (XO (XI (XO (XI (XO (XI (XI (XO (XO (XI (XI (XO (XI (XI (XI (XI (XO
    (XO (XO (XO (XO (XI (XI (XI (XI (XI
    XH))))))))))))))))))))))))))))) :: ((Zpos (XI (XO (XO (XI (XI (XO (XO (XO
    (XI (XO (XI (XI (XO (XO (XI (XI (XO (XO (XO (XO (XO (XI (XI (XI (XI (XI
    (XO (XI (XI (XO XH))))))))))))))))))))))))))))))) :: [])))))))

(** val get_k : int -> z list -> z **)

let rec get_k i = function
| [] -> Z0
| k :: rest -> if (=) i 0 then k else get_k (pred i) rest

(** val nth_word : int -> z list -> z **)

let rec nth_word i ws =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> match ws with
              | [] -> Z0
              | w :: _ -> w)
    (fun i' -> match ws with
               | [] -> Z0
               | _ :: rest -> nth_word i' rest)
    i

(** val build_w_go : int -> z list -> z list -> z list **)

let rec build_w_go i m acc =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> rev acc)
    (fun i' ->
    let r =
      sub (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
        0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) i
    in
    let wi =
      if Nat.ltb r (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ (Pervasives.succ (Pervasives.succ
           (Pervasives.succ 0))))))))))))))))
      then nth_word r m
      else let w_im2 = nth_word (Pervasives.succ 0) acc in
           let w_im7 =
             nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ 0)))))) acc
           in
           let w_im15 =
             nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ 0)))))))))))))) acc
           in
           let w_im16 =
             nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               (Pervasives.succ (Pervasives.succ (Pervasives.succ
               0))))))))))))))) acc
           in
           w32_add (w32_add (w32_add (ssig1 w_im2) w_im16) (ssig0 w_im15))
             w_im7
    in
    build_w_go i' m (wi :: acc))
    i

(** val build_w : z list -> z list **)

let build_w m =
  build_w_go (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
    0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) m []

(** val sha256_round :
    int -> z -> z -> z -> z -> z -> z -> z -> z -> z list -> z list **)

let rec sha256_round t a b c d e f g h w =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ ->
    a :: (b :: (c :: (d :: (e :: (f :: (g :: (h :: []))))))))
    (fun t' ->
    let r =
      sub (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
        0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) t
    in
    let w0 = nth_word r w in
    let k = get_k r k256 in
    let t1 = w32_add (w32_add (w32_add (w32_add h (bsig1 e)) (ch e f g)) k) w0
    in
    let t2 = w32_add (bsig0 a) (maj a b c) in
    sha256_round t' (w32_add t1 t2) a b c (w32_add d t1) e f g w)
    t

(** val sha256_compress_block : z list -> z list -> z list **)

let sha256_compress_block hstate m =
  let w = build_w m in
  let a = nth_word 0 hstate in
  let b = nth_word (Pervasives.succ 0) hstate in
  let c = nth_word (Pervasives.succ (Pervasives.succ 0)) hstate in
  let d =
    nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ 0))) hstate
  in
  let e =
    nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ 0)))) hstate
  in
  let f =
    nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ 0))))) hstate
  in
  let g =
    nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ 0)))))) hstate
  in
  let h =
    nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      0))))))) hstate
  in
  let r =
    sha256_round (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
      0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) a b c
      d e f g h w
  in
  (w32_add a (nth_word 0 r)) :: ((w32_add b (nth_word (Pervasives.succ 0) r)) :: (
  (w32_add c (nth_word (Pervasives.succ (Pervasives.succ 0)) r)) :: (
  (w32_add d
    (nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ 0))) r)) :: (
  (w32_add e
    (nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ 0)))) r)) :: ((w32_add f
                                       (nth_word (Pervasives.succ
                                         (Pervasives.succ (Pervasives.succ
                                         (Pervasives.succ (Pervasives.succ
                                         0))))) r)) :: ((w32_add g
                                                          (nth_word
                                                            (Pervasives.succ
                                                            (Pervasives.succ
                                                            (Pervasives.succ
                                                            (Pervasives.succ
                                                            (Pervasives.succ
                                                            (Pervasives.succ
                                                            0)))))) r)) :: (
  (w32_add h
    (nth_word (Pervasives.succ (Pervasives.succ (Pervasives.succ
      (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
      0))))))) r)) :: [])))))))

(** val bytes_to_words_go : byte list -> z list -> int -> z list **)

let rec bytes_to_words_go bs acc n0 =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> rev acc)
    (fun n' ->
    match bs with
    | [] -> rev acc
    | b0 :: l ->
      (match l with
       | [] -> rev acc
       | b1 :: l0 ->
         (match l0 with
          | [] -> rev acc
          | b2 :: l1 ->
            (match l1 with
             | [] -> rev acc
             | b3 :: rest ->
               let w =
                 w32
                   (Z.add
                     (Z.add
                       (Z.add
                         (Z.mul (Z.of_nat b0) (Zpos (XO (XO (XO (XO (XO (XO
                           (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
                           (XO (XO (XO (XO (XO (XO
                           XH))))))))))))))))))))))))))
                         (Z.mul (Z.of_nat b1) (Zpos (XO (XO (XO (XO (XO (XO
                           (XO (XO (XO (XO (XO (XO (XO (XO (XO (XO
                           XH)))))))))))))))))))
                       (Z.mul (Z.of_nat b2) (Zpos (XO (XO (XO (XO (XO (XO (XO
                         (XO XH))))))))))) (Z.of_nat b3))
               in
               bytes_to_words_go rest (w :: acc) n'))))
    n0

(** val bytes_to_block16 : byte list -> z list **)

let bytes_to_block16 bs =
  bytes_to_words_go bs [] (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ (Pervasives.succ (Pervasives.succ (Pervasives.succ
    (Pervasives.succ 0))))))))))))))))

(** val sha256_blocks : z list -> byte list list -> z list **)

let rec sha256_blocks hstate = function
| [] -> hstate
| blk :: rest ->
  sha256_blocks (sha256_compress_block hstate (bytes_to_block16 blk)) rest

(** val repeat_n : int -> byte -> byte list **)

let rec repeat_n n0 x =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> [])
    (fun n' -> x :: (repeat_n n' x))
    n0

(** val app_list : byte list -> byte list -> byte list **)

let rec app_list xs ys =
  match xs with
  | [] -> ys
  | x :: xr -> x :: (app_list xr ys)

(** val u64_be_bytes : z -> byte list **)

let u64_be_bytes len_bits =
  (Z.to_nat
    (Z.modulo (Z.shiftr len_bits (Zpos (XO (XO (XO (XI (XI XH))))))) (Zpos
      (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))) :: ((Z.to_nat
                                                          (Z.modulo
                                                            (Z.shiftr
                                                              len_bits (Zpos
                                                              (XO (XO (XO (XO
                                                              (XI XH)))))))
                                                            (Zpos (XO (XO (XO
                                                            (XO (XO (XO (XO
                                                            (XO XH))))))))))) :: (
    (Z.to_nat
      (Z.modulo (Z.shiftr len_bits (Zpos (XO (XO (XO (XI (XO XH))))))) (Zpos
        (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))) :: ((Z.to_nat
                                                            (Z.modulo
                                                              (Z.shiftr
                                                                len_bits
                                                                (Zpos (XO (XO
                                                                (XO (XO (XO
                                                                XH)))))))
                                                              (Zpos (XO (XO
                                                              (XO (XO (XO (XO
                                                              (XO (XO
                                                              XH))))))))))) :: (
    (Z.to_nat
      (Z.modulo (Z.shiftr len_bits (Zpos (XO (XO (XO (XI XH)))))) (Zpos (XO
        (XO (XO (XO (XO (XO (XO (XO XH))))))))))) :: ((Z.to_nat
                                                        (Z.modulo
                                                          (Z.shiftr len_bits
                                                            (Zpos (XO (XO (XO
                                                            (XO XH))))))
                                                          (Zpos (XO (XO (XO
                                                          (XO (XO (XO (XO (XO
                                                          XH))))))))))) :: (
    (Z.to_nat
      (Z.modulo (Z.shiftr len_bits (Zpos (XO (XO (XO XH))))) (Zpos (XO (XO
        (XO (XO (XO (XO (XO (XO XH))))))))))) :: ((Z.to_nat
                                                    (Z.modulo len_bits (Zpos
                                                      (XO (XO (XO (XO (XO (XO
                                                      (XO (XO XH))))))))))) :: [])))))))

(** val take_bytes : int -> byte list -> byte list * byte list **)

let rec take_bytes n0 l =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> ([], l))
    (fun n' ->
    match l with
    | [] -> ([], [])
    | x :: xr -> let (t, r) = take_bytes n' xr in ((x :: t), r))
    n0

(** val split_blocks_fuel :
    int -> byte list -> byte list list -> byte list list **)

let rec split_blocks_fuel fuel l acc =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> rev acc)
    (fun fuel' ->
    match l with
    | [] -> rev acc
    | _ :: _ ->
      let (blk, rest) =
        take_bytes (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
          0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) l
      in
      split_blocks_fuel fuel' rest (blk :: acc))
    fuel

(** val split_blocks_go : byte list -> byte list list -> byte list list **)

let split_blocks_go l acc =
  split_blocks_fuel (length l) l acc

(** val sha256_pad : byte list -> byte list list **)

let sha256_pad msg =
  let n0 = length msg in
  let k =
    Z.to_nat
      (Z.modulo (Z.sub (Zpos (XI (XI (XI (XO (XI XH)))))) (Z.of_nat n0))
        (Zpos (XO (XO (XO (XO (XO (XO XH))))))))
  in
  let padded =
    app_list msg ((Pervasives.succ (Pervasives.succ (Pervasives.succ
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
      0)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))) :: 
      (app_list (repeat_n k 0)
        (u64_be_bytes (Z.mul (Z.of_nat n0) (Zpos (XO (XO (XO XH))))))))
  in
  split_blocks_go padded []

(** val word_to_bytes : z -> byte list **)

let word_to_bytes w =
  (Z.to_nat
    (Z.modulo (Z.shiftr w (Zpos (XO (XO (XO (XI XH)))))) (Zpos (XO (XO (XO
      (XO (XO (XO (XO (XO XH))))))))))) :: ((Z.to_nat
                                              (Z.modulo
                                                (Z.shiftr w (Zpos (XO (XO (XO
                                                  (XO XH)))))) (Zpos (XO (XO
                                                (XO (XO (XO (XO (XO (XO
                                                XH))))))))))) :: ((Z.to_nat
                                                                    (Z.modulo
                                                                    (Z.shiftr
                                                                    w (Zpos
                                                                    (XO (XO
                                                                    (XO
                                                                    XH)))))
                                                                    (Zpos (XO
                                                                    (XO (XO
                                                                    (XO (XO
                                                                    (XO (XO
                                                                    (XO
                                                                    XH))))))))))) :: (
    (Z.to_nat (Z.modulo w (Zpos (XO (XO (XO (XO (XO (XO (XO (XO XH))))))))))) :: [])))

(** val words_to_bytes_go : z list -> byte list **)

let rec words_to_bytes_go = function
| [] -> []
| w :: rest -> app_list (word_to_bytes w) (words_to_bytes_go rest)

(** val words_to_bytes32 : z list -> byte list **)

let words_to_bytes32 =
  words_to_bytes_go

(** val sha256_bytes : byte list -> byte list **)

let sha256_bytes msg =
  let blocks = sha256_pad msg in
  words_to_bytes32 (sha256_blocks h256_init blocks)

(** val hash256_bytes : byte list -> byte list **)

let hash256_bytes msg =
  sha256_bytes (sha256_bytes msg)
