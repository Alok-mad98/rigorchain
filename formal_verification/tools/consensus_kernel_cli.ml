(* Adapter only. Calls extracted Coq Consensus Kernel. No second formula. *)

module K = Consensus_kernel

let rec pos_of_int i =
  if i = 1 then K.XH
  else if i land 1 = 1 then K.XI (pos_of_int (i lsr 1))
  else K.XO (pos_of_int (i lsr 1))

let z_of_int n =
  if n = 0 then K.Z0
  else if n > 0 then K.Zpos (pos_of_int n)
  else K.Zneg (pos_of_int (-n))

let rec int_of_pos = function
  | K.XH -> 1
  | K.XO p -> int_of_pos p lsl 1
  | K.XI p -> (int_of_pos p lsl 1) + 1

(* Remainders 0..9 only. Full Z uses z_to_dec. *)
let int_of_z_small = function
  | K.Z0 -> 0
  | K.Zpos p -> int_of_pos p
  | K.Zneg p -> -int_of_pos p

let rec digits_of_z z acc =
  if K.Z.eqb z K.Z0 then acc
  else
    let ten = z_of_int 10 in
    let q = K.Z.div z ten in
    let r = int_of_z_small (K.Z.modulo z ten) in
    digits_of_z q (r :: acc)

let z_to_dec z =
  match z with
  | K.Z0 -> "0"
  | K.Zneg _ ->
      "-"
      ^
      String.concat ""
        (List.map string_of_int (digits_of_z (K.Z.opp z) []))
  | K.Zpos _ ->
      String.concat "" (List.map string_of_int (digits_of_z z []))

let usage () =
  prerr_endline
    "consensus_kernel_cli (extracted Coq adapter, not a second kernel)\n\
     commands:\n\
     epoch H | oru H | res-schedule H | res-issued H | issuance H\n\
     mature SPEND COIN | maturity-const\n\
     classify HEX|- \n\
     claimable FEES BURN SUBSIDY | ledger-delta SUB CONVERGE\n\
     res-coinbase FEES BOOT MINT\n\
     apply-delta H SUB MINT CONV BURN ORD AUTH INV\n\
     draw-empty | draw H C_HEX B_HEX TXID:WEIGHT[,TXID:WEIGHT...]\n\
     draw-t0 H C_HEX B_HEX TXID:WEIGHT[,TXID:WEIGHT...]\n\
     sum-oru LO HI";
  exit 2

let asset_tag = function
  | K.AssetORU -> "ORU"
  | K.AssetRES -> "RES"
  | K.AssetNone -> "NONE"
  | K.AssetInvalid -> "INVALID"

let hex_digit c =
  match c with
  | '0' .. '9' -> Char.code c - Char.code '0'
  | 'a' .. 'f' -> 10 + Char.code c - Char.code 'a'
  | 'A' .. 'F' -> 10 + Char.code c - Char.code 'A'
  | _ -> failwith ("bad hex digit: " ^ String.make 1 c)

let hex_to_bytes s =
  let n = String.length s in
  if n mod 2 <> 0 then failwith "odd hex length";
  let rec go i acc =
    if i >= n then List.rev acc
    else
      let b = (hex_digit s.[i] lsl 4) lor hex_digit s.[i + 1] in
      go (i + 2) (b :: acc)
  in
  go 0 []

let () =
  if Array.length Sys.argv < 2 then usage ();
  match Sys.argv.(1) with
  | "epoch" ->
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      print_endline (z_to_dec (K.issuance_epoch h))
  | "oru" ->
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      print_endline (z_to_dec (K.oru_subsidy h))
  | "res-schedule" ->
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      print_endline (z_to_dec (K.get_res_mint_reward h))
  | "res-issued" ->
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      print_endline (z_to_dec (K.res_issued_at_height h))
  | "issuance" ->
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      let (e, o), r = K.compute_rigor_issuance h in
      Printf.printf "%s %s %s\n" (z_to_dec e) (z_to_dec o) (z_to_dec r)
  | "mature" ->
      let s = z_of_int (int_of_string Sys.argv.(2)) in
      let c = z_of_int (int_of_string Sys.argv.(3)) in
      print_endline (if K.coinbase_mature s c then "true" else "false")
  | "maturity-const" -> print_endline (z_to_dec K.cOINBASE_MATURITY)
  | "classify" ->
      let hex = if Array.length Sys.argv > 2 then Sys.argv.(2) else "" in
      let bytes = if hex = "-" || hex = "" then [] else hex_to_bytes hex in
      print_endline (asset_tag (K.classify bytes))
  | "claimable" ->
      let f = z_of_int (int_of_string Sys.argv.(2)) in
      let b = z_of_int (int_of_string Sys.argv.(3)) in
      let s = z_of_int (int_of_string Sys.argv.(4)) in
      print_endline (z_to_dec (K.coinbase_oru_cap f b s))
  | "ledger-delta" ->
      let s = z_of_int (int_of_string Sys.argv.(2)) in
      let c = z_of_int (int_of_string Sys.argv.(3)) in
      print_endline (z_to_dec (K.ledger_oru_delta s c))
  | "res-coinbase" ->
      let f = z_of_int (int_of_string Sys.argv.(2)) in
      let b = z_of_int (int_of_string Sys.argv.(3)) in
      let m = z_of_int (int_of_string Sys.argv.(4)) in
      print_endline (z_to_dec (K.rigor_res_coinbase_expected f b m))
  | "apply-delta" ->
      if Array.length Sys.argv <> 10 then usage ();
      let d : K.blockDelta =
        {
          bd_height = z_of_int (int_of_string Sys.argv.(2));
          bd_subsidy = z_of_int (int_of_string Sys.argv.(3));
          bd_valid_mint = z_of_int (int_of_string Sys.argv.(4));
          bd_converge = z_of_int (int_of_string Sys.argv.(5));
          bd_fee_burn = z_of_int (int_of_string Sys.argv.(6));
          bd_ordinary = Sys.argv.(7) <> "0";
          bd_mint_authorized = Sys.argv.(8) <> "0";
          bd_has_invalid = Sys.argv.(9) <> "0";
        }
      in
      (match K.apply_delta K.genesis_supply d with
      | None -> print_endline "REJECT"
      | Some s ->
          Printf.printf "%s %s %s %s\n" (z_to_dec s.us_ledger_oru)
            (z_to_dec s.us_circulating_oru) (z_to_dec s.us_res)
            (z_to_dec s.us_fee_burn_acc))
  | "draw-empty" ->
      (match
         K.resolve_convergence_draw [] [] [] (z_of_int 0) K.rES_PROBABILITY_TARGET
       with
      | None -> print_endline "NONE"
      | Some _ -> print_endline "WINNER")
  | "draw" | "draw-t0" as dcmd ->
      if Array.length Sys.argv <> 6 then usage ();
      let h = z_of_int (int_of_string Sys.argv.(2)) in
      let c = hex_to_bytes Sys.argv.(3) in
      let b = hex_to_bytes Sys.argv.(4) in
      let spec = Sys.argv.(5) in
      let target = if dcmd = "draw-t0" then K.Z0 else K.rES_PROBABILITY_TARGET in
      let rec split_on sep s =
        match String.index_opt s sep with
        | None -> if s = "" then [] else [ s ]
        | Some i ->
            String.sub s 0 i
            :: split_on sep (String.sub s (i + 1) (String.length s - i - 1))
      in
      let parse_one one =
        match String.index_opt one ':' with
        | None -> failwith "draw spec TXID:WEIGHT"
        | Some i ->
            let hx = String.sub one 0 i in
            let w = int_of_string (String.sub one (i + 1) (String.length one - i - 1)) in
            let bytes = hex_to_bytes hx in
            ( {
                K.tx_id = K.Z0;
                tx_id_bytes = bytes;
                asset_in = K.ORU;
                fee = K.Z0;
                mint_cost = z_of_int w;
              },
              hx )
      in
      let parsed = List.map parse_one (split_on ',' spec) in
      let f = List.map fst parsed in
      (match K.resolve_convergence_draw f c b h target with
      | None -> print_endline "{\"draw\":\"NONE\"}"
      | Some w ->
          let rec find_hex = function
            | [] -> "unknown"
            | (t, hx) :: rest ->
                if t.K.tx_id_bytes = w.K.tx_id_bytes then hx else find_hex rest
          in
          Printf.printf "{\"draw\":\"WINNER\",\"txid\":\"%s\"}\n" (find_hex parsed))
  | "sum-oru" ->
      let lo = int_of_string Sys.argv.(2) in
      let hi = int_of_string Sys.argv.(3) in
      let rec go h acc =
        if h > hi then acc
        else go (h + 1) (K.Z.add acc (K.oru_subsidy (z_of_int h)))
      in
      print_endline (z_to_dec (go lo K.Z0))
  | _ -> usage ()
