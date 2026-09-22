
(** val negb : bool -> bool **)

let negb = function
| true -> false
| false -> true

(** val fst : ('a1 * 'a2) -> 'a1 **)

let fst = function
| (x, _) -> x

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

(** val add : int -> int -> int **)

let rec add = (+)

(** val mul : int -> int -> int **)

let rec mul = ( * )

(** val sub : int -> int -> int **)

let rec sub = fun n m -> Pervasives.max 0 (n-m)

module Nat =
 struct
  (** val ltb : int -> int -> bool **)

  let ltb n0 m =
    (<=) (Pervasives.succ n0) m
 end

module Pos =
 struct
  type mask =
  | IsNul
  | IsPos of int
  | IsNeg
 end

module Coq_Pos =
 struct
  (** val succ : int -> int **)

  let rec succ = Pervasives.succ

  (** val add : int -> int -> int **)

  let rec add = (+)

  (** val add_carry : int -> int -> int **)

  and add_carry x y =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> (fun p->1+2*p) (add_carry p q))
        (fun q -> (fun p->2*p) (add_carry p q))
        (fun _ -> (fun p->1+2*p) (succ p))
        y)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> (fun p->2*p) (add_carry p q))
        (fun q -> (fun p->1+2*p) (add p q))
        (fun _ -> (fun p->2*p) (succ p))
        y)
      (fun _ ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> (fun p->1+2*p) (succ q))
        (fun q -> (fun p->2*p) (succ q))
        (fun _ -> (fun p->1+2*p) 1)
        y)
      x

  (** val pred_double : int -> int **)

  let rec pred_double x =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p -> (fun p->1+2*p) ((fun p->2*p) p))
      (fun p -> (fun p->1+2*p) (pred_double p))
      (fun _ -> 1)
      x

  (** val pred_N : int -> int **)

  let pred_N x =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p -> ((fun p->2*p) p))
      (fun p -> (pred_double p))
      (fun _ -> 0)
      x

  type mask = Pos.mask =
  | IsNul
  | IsPos of int
  | IsNeg

  (** val succ_double_mask : mask -> mask **)

  let succ_double_mask = function
  | IsNul -> IsPos 1
  | IsPos p -> IsPos ((fun p->1+2*p) p)
  | IsNeg -> IsNeg

  (** val double_mask : mask -> mask **)

  let double_mask = function
  | IsPos p -> IsPos ((fun p->2*p) p)
  | x0 -> x0

  (** val double_pred_mask : int -> mask **)

  let double_pred_mask x =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p -> IsPos ((fun p->2*p) ((fun p->2*p) p)))
      (fun p -> IsPos ((fun p->2*p) (pred_double p)))
      (fun _ -> IsNul)
      x

  (** val sub_mask : int -> int -> mask **)

  let rec sub_mask x y =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> double_mask (sub_mask p q))
        (fun q -> succ_double_mask (sub_mask p q))
        (fun _ -> IsPos ((fun p->2*p) p))
        y)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> succ_double_mask (sub_mask_carry p q))
        (fun q -> double_mask (sub_mask p q))
        (fun _ -> IsPos (pred_double p))
        y)
      (fun _ ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> IsNeg)
        (fun _ -> IsNeg)
        (fun _ -> IsNul)
        y)
      x

  (** val sub_mask_carry : int -> int -> mask **)

  and sub_mask_carry x y =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> succ_double_mask (sub_mask_carry p q))
        (fun q -> double_mask (sub_mask p q))
        (fun _ -> IsPos (pred_double p))
        y)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> double_mask (sub_mask_carry p q))
        (fun q -> succ_double_mask (sub_mask_carry p q))
        (fun _ -> double_pred_mask p)
        y)
      (fun _ -> IsNeg)
      x

  (** val mul : int -> int -> int **)

  let rec mul = ( * )

  (** val iter : ('a1 -> 'a1) -> 'a1 -> int -> 'a1 **)

  let rec iter f x n0 =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun n' -> f (iter f (iter f x n') n'))
      (fun n' -> iter f (iter f x n') n')
      (fun _ -> f x)
      n0

  (** val div2 : int -> int **)

  let div2 p =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p0 -> p0)
      (fun p0 -> p0)
      (fun _ -> 1)
      p

  (** val div2_up : int -> int **)

  let div2_up p =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p0 -> succ p0)
      (fun p0 -> p0)
      (fun _ -> 1)
      p

  (** val size : int -> int **)

  let rec size p =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p0 -> succ (size p0))
      (fun p0 -> succ (size p0))
      (fun _ -> 1)
      p

  (** val compare_cont : comparison -> int -> int -> comparison **)

  let rec compare_cont = fun c x y -> if x=y then c else if x<y then Lt else Gt

  (** val compare : int -> int -> comparison **)

  let compare = fun x y -> if x=y then Eq else if x<y then Lt else Gt

  (** val eqb : int -> int -> bool **)

  let rec eqb p q =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p0 ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q0 -> eqb p0 q0)
        (fun _ -> false)
        (fun _ -> false)
        q)
      (fun p0 ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> false)
        (fun q0 -> eqb p0 q0)
        (fun _ -> false)
        q)
      (fun _ ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> false)
        (fun _ -> false)
        (fun _ -> true)
        q)
      p

  (** val testbit : int -> int -> bool **)

  let rec testbit p n0 =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p0 ->
      (fun f0 fp n -> if n=0 then f0 () else fp n)
        (fun _ -> true)
        (fun n1 -> testbit p0 (pred_N n1))
        n0)
      (fun p0 ->
      (fun f0 fp n -> if n=0 then f0 () else fp n)
        (fun _ -> false)
        (fun n1 -> testbit p0 (pred_N n1))
        n0)
      (fun _ ->
      (fun f0 fp n -> if n=0 then f0 () else fp n)
        (fun _ -> true)
        (fun _ -> false)
        n0)
      p
 end

module N =
 struct
  (** val succ_double : int -> int **)

  let succ_double x =
    (fun f0 fp n -> if n=0 then f0 () else fp n)
      (fun _ -> 1)
      (fun p -> ((fun p->1+2*p) p))
      x

  (** val double : int -> int **)

  let double n0 =
    (fun f0 fp n -> if n=0 then f0 () else fp n)
      (fun _ -> 0)
      (fun p -> ((fun p->2*p) p))
      n0

  (** val sub : int -> int -> int **)

  let sub = fun n m -> Pervasives.max 0 (n-m)

  (** val compare : int -> int -> comparison **)

  let compare = fun x y -> if x=y then Eq else if x<y then Lt else Gt

  (** val leb : int -> int -> bool **)

  let leb x y =
    match compare x y with
    | Gt -> false
    | _ -> true

  (** val pos_div_eucl : int -> int -> int * int **)

  let rec pos_div_eucl a b =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = succ_double r in
      if leb b r' then ((succ_double q), (sub r' b)) else ((double q), r'))
      (fun a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = double r in
      if leb b r' then ((succ_double q), (sub r' b)) else ((double q), r'))
      (fun _ ->
      (fun f0 fp n -> if n=0 then f0 () else fp n)
        (fun _ -> (0, 1))
        (fun p ->
        (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
          (fun _ -> (0, 1))
          (fun _ -> (0, 1))
          (fun _ -> (1, 0))
          p)
        b)
      a

  (** val testbit : int -> int -> bool **)

  let testbit a n0 =
    (fun f0 fp n -> if n=0 then f0 () else fp n)
      (fun _ -> false)
      (fun p -> Coq_Pos.testbit p n0)
      a
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
  (** val double : int -> int **)

  let double x =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> 0)
      (fun p -> ((fun p->2*p) p))
      (fun p -> (~-) ((fun p->2*p) p))
      x

  (** val succ_double : int -> int **)

  let succ_double x =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> 1)
      (fun p -> ((fun p->1+2*p) p))
      (fun p -> (~-) (Coq_Pos.pred_double p))
      x

  (** val pred_double : int -> int **)

  let pred_double x =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> (~-) 1)
      (fun p -> (Coq_Pos.pred_double p))
      (fun p -> (~-) ((fun p->1+2*p) p))
      x

  (** val pos_sub : int -> int -> int **)

  let rec pos_sub x y =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> double (pos_sub p q))
        (fun q -> succ_double (pos_sub p q))
        (fun _ -> ((fun p->2*p) p))
        y)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> pred_double (pos_sub p q))
        (fun q -> double (pos_sub p q))
        (fun _ -> (Coq_Pos.pred_double p))
        y)
      (fun _ ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun q -> (~-) ((fun p->2*p) q))
        (fun q -> (~-) (Coq_Pos.pred_double q))
        (fun _ -> 0)
        y)
      x

  (** val add : int -> int -> int **)

  let add = (+)

  (** val opp : int -> int **)

  let opp = (~-)

  (** val sub : int -> int -> int **)

  let sub = (-)

  (** val mul : int -> int -> int **)

  let mul = ( * )

  (** val compare : int -> int -> comparison **)

  let compare = fun x y -> if x=y then Eq else if x<y then Lt else Gt

  (** val leb : int -> int -> bool **)

  let leb x y =
    match compare x y with
    | Gt -> false
    | _ -> true

  (** val ltb : int -> int -> bool **)

  let ltb x y =
    match compare x y with
    | Lt -> true
    | _ -> false

  (** val eqb : int -> int -> bool **)

  let eqb x y =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> true)
        (fun _ -> false)
        (fun _ -> false)
        y)
      (fun p ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> false)
        (fun q -> Coq_Pos.eqb p q)
        (fun _ -> false)
        y)
      (fun p ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> false)
        (fun _ -> false)
        (fun q -> Coq_Pos.eqb p q)
        y)
      x

  (** val of_N : int -> int **)

  let of_N = fun p -> p

  (** val pos_div_eucl : int -> int -> int * int **)

  let rec pos_div_eucl a b =
    (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
      (fun a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = add (mul ((fun p->2*p) 1) r) 1 in
      if ltb r' b
      then ((mul ((fun p->2*p) 1) q), r')
      else ((add (mul ((fun p->2*p) 1) q) 1), (sub r' b)))
      (fun a' ->
      let (q, r) = pos_div_eucl a' b in
      let r' = mul ((fun p->2*p) 1) r in
      if ltb r' b
      then ((mul ((fun p->2*p) 1) q), r')
      else ((add (mul ((fun p->2*p) 1) q) 1), (sub r' b)))
      (fun _ -> if leb ((fun p->2*p) 1) b then (0, 1) else (1, 0))
      a

  (** val div_eucl : int -> int -> int * int **)

  let div_eucl a b =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> (0, 0))
      (fun a' ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> (0, a))
        (fun _ -> pos_div_eucl a' b)
        (fun b' ->
        let (q, r) = pos_div_eucl a' b' in
        ((fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
           (fun _ -> ((opp q), 0))
           (fun _ -> ((opp (add q 1)), (add b r)))
           (fun _ -> ((opp (add q 1)), (add b r)))
           r))
        b)
      (fun a' ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> (0, a))
        (fun _ ->
        let (q, r) = pos_div_eucl a' b in
        ((fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
           (fun _ -> ((opp q), 0))
           (fun _ -> ((opp (add q 1)), (sub b r)))
           (fun _ -> ((opp (add q 1)), (sub b r)))
           r))
        (fun b' -> let (q, r) = pos_div_eucl a' b' in (q, (opp r)))
        b)
      a

  (** val div : int -> int -> int **)

  let div a b =
    let (q, _) = div_eucl a b in q

  (** val modulo : int -> int -> int **)

  let modulo a b =
    let (_, r) = div_eucl a b in r

  (** val quotrem : int -> int -> int * int **)

  let quotrem a b =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> (0, 0))
      (fun a0 ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> (0, a))
        (fun b0 ->
        let (q, r) = N.pos_div_eucl a0 b0 in ((of_N q), (of_N r)))
        (fun b0 ->
        let (q, r) = N.pos_div_eucl a0 b0 in ((opp (of_N q)), (of_N r)))
        b)
      (fun a0 ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> (0, a))
        (fun b0 ->
        let (q, r) = N.pos_div_eucl a0 b0 in ((opp (of_N q)), (opp (of_N r))))
        (fun b0 ->
        let (q, r) = N.pos_div_eucl a0 b0 in ((of_N q), (opp (of_N r))))
        b)
      a

  (** val quot : int -> int -> int **)

  let quot a b =
    fst (quotrem a b)

  (** val odd : int -> bool **)

  let odd z0 =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> false)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> true)
        (fun _ -> false)
        (fun _ -> true)
        p)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> true)
        (fun _ -> false)
        (fun _ -> true)
        p)
      z0

  (** val div2 : int -> int **)

  let div2 z0 =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> 0)
      (fun p ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun _ -> (Coq_Pos.div2 p))
        (fun _ -> (Coq_Pos.div2 p))
        (fun _ -> 0)
        p)
      (fun p -> (~-) (Coq_Pos.div2_up p))
      z0

  (** val log2 : int -> int **)

  let log2 z0 =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> 0)
      (fun p0 ->
      (fun f2p1 f2p f1 p ->
  if p<=1 then f1 () else if p mod 2 = 0 then f2p (p/2) else f2p1 (p/2))
        (fun p -> (Coq_Pos.size p))
        (fun p -> (Coq_Pos.size p))
        (fun _ -> 0)
        p0)
      (fun _ -> 0)
      z0

  (** val testbit : int -> int -> bool **)

  let testbit a n0 =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> odd a)
      (fun p ->
      (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
        (fun _ -> false)
        (fun a0 -> Coq_Pos.testbit a0 p)
        (fun a0 -> negb (N.testbit (Coq_Pos.pred_N a0) p))
        a)
      (fun _ -> false)
      n0

  (** val shiftl : int -> int -> int **)

  let shiftl a n0 =
    (fun f0 fp fn z -> if z=0 then f0 () else if z>0 then fp z else fn (-z))
      (fun _ -> a)
      (fun p -> Coq_Pos.iter (mul ((fun p->2*p) 1)) a p)
      (fun p -> Coq_Pos.iter div2 a p)
      n0

  (** val shiftr : int -> int -> int **)

  let shiftr a n0 =
    shiftl a (opp n0)
 end

type byte = int

(** val split_at : int -> byte list -> (byte list * byte list) option **)

let rec split_at n0 xs =
  (fun fO fS n -> if n=0 then fO () else fS (n-1))
    (fun _ -> Some ([], xs))
    (fun n' ->
    match xs with
    | [] -> None
    | x :: xs' ->
      (match split_at n' xs' with
       | Some p -> let (pre, post) = p in Some ((x :: pre), post)
       | None -> None))
    n0

type amount = int

(** val cOIN : amount **)

let cOIN =
  ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p) ((fun p->1+2*p)
    ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->2*p) ((fun p->1+2*p)
    ((fun p->2*p) ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p)
    ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->2*p) 1))))))))))))))))))))))))))

(** val mAX_MONEY : amount **)

let mAX_MONEY =
  Z.mul ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->1+2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p)
    ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p)
    ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p)
    ((fun p->1+2*p) ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p) ((fun p->2*p)
    1))))))))))))))))))))))))))))))))) cOIN

(** val money_range_b : amount -> bool **)

let money_range_b v =
  (&&) (Z.leb 0 v) (Z.leb v mAX_MONEY)

type hash32 = byte list

type outpoint = { op_txid : hash32; op_index : int }

type txout = { txo_value : int; txo_spk : byte list }

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
     | y :: ys -> (&&) ((=) x y) (bytes_eqb xs ys))

(** val outpoint_eqb : outpoint -> outpoint -> bool **)

let outpoint_eqb o1 o2 =
  (&&) (bytes_eqb o1.op_txid o2.op_txid) ((=) o1.op_index o2.op_index)

(** val is_output_op_b : tx -> outpoint -> bool **)

let is_output_op_b t o =
  (&&) (bytes_eqb o.op_txid t.tx_id) (Nat.ltb o.op_index (length t.tx_out))

(** val inputs_fresh_b : tx -> bool **)

let inputs_fresh_b t =
  forallb (fun o -> negb (is_output_op_b t o)) t.tx_in

(** val sum_values : txout list -> int **)

let rec sum_values = function
| [] -> 0
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

type oru_tx = { tx_dom : input_domain; oru_in : int; oru_out : int;
                res_in : int; res_out : int }

type kernel_tx = { kt_dom : input_domain; kt_oru_in : int; kt_oru_out : 
                   int; kt_res_in : int; kt_res_out : int;
                   kt_old_supply : int; kt_subsidy : int }

(** val to_oru : kernel_tx -> oru_tx **)

let to_oru t =
  { tx_dom = t.kt_dom; oru_in = t.kt_oru_in; oru_out = t.kt_oru_out; res_in =
    t.kt_res_in; res_out = t.kt_res_out }

type oRUState = kernel_tx

(** val valid_oru_tx_b : oru_tx -> bool **)

let valid_oru_tx_b t =
  (&&) (Z.leb 0 t.oru_in)
    ((&&) (Z.leb 0 t.oru_out)
      ((&&) (Z.leb 0 t.res_in)
        ((&&) (Z.leb 0 t.res_out)
          ((&&) (Z.leb t.oru_out t.oru_in)
            (match t.tx_dom with
             | DomORU -> (&&) (Z.eqb t.res_in 0) (Z.eqb t.res_out 0)
             | DomRES -> (&&) (Z.ltb 0 t.oru_in) (Z.leb t.res_out t.res_in))))))

(** val check_oru : oRUState -> bool **)

let check_oru s =
  valid_oru_tx_b (to_oru s)

type block_height = int

(** val rES_DESTROY_POLE : int **)

let rES_DESTROY_POLE =
  Z.shiftl 1 ((fun p->1+2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p) 1))))

(** val rES_DESTROY_SHIFT : int **)

let rES_DESTROY_SHIFT =
  ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p)
    1)))))

(** val rES_MAX_SUPPLY : int **)

let rES_MAX_SUPPLY =
  ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->1+2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p)
    ((fun p->2*p) ((fun p->1+2*p) ((fun p->2*p) ((fun p->1+2*p)
    ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p)
    ((fun p->2*p) 1))))))))))))))))))))))))))))))))))))))))))))))))

(** val res_mint_authorized : block_height -> bool **)

let res_mint_authorized h =
  Z.leb 0 h

(** val res_destroy_pole_sq : int -> int **)

let res_destroy_pole_sq h =
  Z.mul (Z.add rES_DESTROY_POLE h) (Z.add rES_DESTROY_POLE h)

type res_tx = { rt_coinbase : bool; rt_mint_ok : bool; rt_mint_amount : 
                int; rt_height : int; rt_burn_oru : int;
                rt_dom : input_domain; rt_res_in : int; rt_res_out : 
                int; rt_oru_in : int }

(** val coinbase : res_tx -> bool **)

let coinbase tx0 =
  tx0.rt_coinbase

(** val mint_amount : res_tx -> int **)

let mint_amount tx0 =
  tx0.rt_mint_amount

(** val burn_oru : res_tx -> int **)

let burn_oru tx0 =
  tx0.rt_burn_oru

type rESState = res_tx

(** val check_res : rESState -> bool **)

let check_res s =
  (&&) (negb (coinbase s))
    ((&&) s.rt_mint_ok
      ((&&) (Z.ltb 0 (mint_amount s))
        ((&&) (Z.leb (mint_amount s) rES_MAX_SUPPLY)
          ((&&) (res_mint_authorized s.rt_height)
            ((&&) (Z.leb 0 (burn_oru s))
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

(** val parse_witness_program : script -> (int * byte list) option **)

let parse_witness_program s =
  let n0 = length s in
  if (||)
       (Nat.ltb n0 (Pervasives.succ (Pervasives.succ (Pervasives.succ
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
         0)))))))))))))))))))))))))))))))))))))))))) n0)
  then None
  else (match s with
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

(** val is_mint_script : script -> bool **)

let is_mint_script = function
| [] -> false
| sn_op :: rest ->
  let after_sn =
    if (=) sn_op rIGOR_PUSH_0
    then Some rest
    else if (&&) ((<=) (Pervasives.succ 0) sn_op)
              ((<=) sn_op (Pervasives.succ (Pervasives.succ (Pervasives.succ
                (Pervasives.succ 0)))))
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
        if (=) plen rIGOR_PUSHBYTES_20
        then (match split_at (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      (Pervasives.succ (Pervasives.succ (Pervasives.succ
                      0)))))))))))))))))))) body with
              | Some p ->
                let (_, marker_tl) = p in
                (match marker_tl with
                 | [] -> false
                 | op :: l0 ->
                   (match l0 with
                    | [] -> (=) op rIGOR_MARKER_RES_MINT
                    | _ :: _ -> false))
              | None -> false)
        else false)
   | None -> false)

(** val is_converge_script : script -> bool **)

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

(** val is_null_data : script -> bool **)

let is_null_data = function
| [] -> false
| x :: rest -> (&&) ((=) x rIGOR_MARKER_RETURN) (is_push_only rest)

(** val classify_script : script -> scriptType **)

let classify_script s =
  match parse_witness_program s with
  | Some p ->
    let (n0, prog) = p in
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
       then T_WITNESS_V0
       else T_NONSTANDARD)
       (fun n1 ->
       (fun fO fS n -> if n=0 then fO () else fS (n-1))
         (fun _ ->
         if is_native_res_program prog then T_WITNESS_V1 else T_NONSTANDARD)
         (fun _ -> T_NONSTANDARD)
         n1)
       n0)
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

(** val compact_size : int -> int **)

let compact_size nbits =
  Z.div nbits ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    1))))))))))))))))))))))))

(** val compact_mantissa : int -> int **)

let compact_mantissa nbits =
  Z.modulo nbits ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    1)))))))))))))))))))))))

(** val compact_decode : int -> int **)

let compact_decode nbits =
  let size0 = compact_size nbits in
  let word = compact_mantissa nbits in
  if Z.leb size0 ((fun p->1+2*p) 1)
  then Z.shiftr word
         (Z.mul ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))
           (Z.sub ((fun p->1+2*p) 1) size0))
  else Z.shiftl word
         (Z.mul ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))
           (Z.sub size0 ((fun p->1+2*p) 1)))

(** val raw_size : int -> int **)

let raw_size t =
  Z.div (Z.add (Z.log2 t) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1))))
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))

(** val raw_mantissa : int -> int **)

let raw_mantissa t =
  let s = raw_size t in
  if Z.leb s ((fun p->1+2*p) 1)
  then Z.shiftl t
         (Z.mul ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))
           (Z.sub ((fun p->1+2*p) 1) s))
  else Z.shiftr t
         (Z.mul ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))
           (Z.sub s ((fun p->1+2*p) 1)))

(** val adjust_signbit : int -> int -> int * int **)

let adjust_signbit m s =
  if Z.testbit m ((fun p->1+2*p) ((fun p->1+2*p) ((fun p->1+2*p)
       ((fun p->2*p) 1))))
  then ((Z.shiftr m ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) 1)))),
         (Z.add s 1))
  else (m, s)

(** val compact_encode : int -> int **)

let compact_encode t =
  if Z.leb t 0
  then 0
  else let (m, s) = adjust_signbit (raw_mantissa t) (raw_size t) in
       Z.add
         (Z.mul s ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
           1))))))))))))))))))))))))) m

type powHeader = { pow_digest_z : int; pow_target : int; pow_compact_t : int }

(** val check_pow : powHeader -> bool **)

let check_pow h =
  (&&) (Z.ltb 0 h.pow_target)
    ((&&) (Z.leb h.pow_digest_z h.pow_target)
      ((&&) (Z.leb 0 h.pow_compact_t)
        (Z.leb (compact_decode (compact_encode h.pow_compact_t))
          h.pow_compact_t)))

(** val aSERT_RADIX : int **)

let aSERT_RADIX =
  ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p) ((fun p->2*p)
    ((fun p->2*p) 1))))))))))))))))

(** val exp_q_raw : int -> int -> int **)

let exp_q_raw te tau =
  Z.quot (Z.mul te aSERT_RADIX) tau

type aSERTInput = { asert_te : int; asert_tau : int; asert_expected_q : int }

(** val check_asert : aSERTInput -> bool **)

let check_asert i =
  (&&) (Z.ltb i.asert_te 0)
    ((&&) (Z.ltb 0 i.asert_tau)
      ((&&) (Z.eqb (exp_q_raw i.asert_te i.asert_tau) i.asert_expected_q)
        (Z.eqb (exp_q_raw i.asert_te i.asert_tau)
          (Z.quot (Z.mul i.asert_te aSERT_RADIX) i.asert_tau))))

type consensusKernel = { check_utxo0 : (uTXOState -> bool);
                         check_oru0 : (oRUState -> bool);
                         check_res0 : (rESState -> bool);
                         check_script0 : (scriptState -> bool);
                         check_pow0 : (powHeader -> bool);
                         check_asert0 : (aSERTInput -> bool) }

(** val rigor_kernel : consensusKernel **)

let rigor_kernel =
  { check_utxo0 = check_utxo; check_oru0 = check_oru; check_res0 = check_res;
    check_script0 = check_script; check_pow0 = check_pow; check_asert0 =
    check_asert }

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
