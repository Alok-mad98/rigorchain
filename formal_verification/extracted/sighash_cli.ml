(* Concatenate a sighash preimage from field hex/ints. Coq SighashPreimage.sighash_preimage. *)

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

let rec bytes_to_hex = function
  | [] -> ""
  | n :: rest -> Printf.sprintf "%02x" n ^ bytes_to_hex rest

let z_of_int n =
  if n = 0 then Sighash_kernel.Z0
  else if n > 0 then
    (* le_bytes uses Z, extracted as BinInt *)
    let rec pos_of i =
      if i = 1 then Sighash_kernel.XH
      else if i land 1 = 1 then Sighash_kernel.XI (pos_of (i lsr 1))
      else Sighash_kernel.XO (pos_of (i lsr 1))
    in
    Sighash_kernel.Zpos (pos_of n)
  else failwith "negative not used"

let () =
  if Array.length Sys.argv <> 13 then (
    prerr_endline
      "usage: sighash_cli tag chain_id version hashPrevouts hashSequence hashOutputs prevout_hash prevout_n script amount nSequence nLockTime";
    exit 2);
  let f : Sighash_kernel.sighashFields =
    {
      sh_tag = hex_to_bytes Sys.argv.(1);
      sh_chain_id = hex_to_bytes Sys.argv.(2);
      sh_version = z_of_int (int_of_string Sys.argv.(3));
      sh_hash_prevouts = hex_to_bytes Sys.argv.(4);
      sh_hash_sequence = hex_to_bytes Sys.argv.(5);
      sh_hash_outputs = hex_to_bytes Sys.argv.(6);
      sh_prevout_hash = hex_to_bytes Sys.argv.(7);
      sh_prevout_n = z_of_int (int_of_string Sys.argv.(8));
      sh_script = hex_to_bytes Sys.argv.(9);
      sh_amount = z_of_int (int_of_string Sys.argv.(10));
      sh_sequence = z_of_int (int_of_string Sys.argv.(11));
      sh_locktime = z_of_int (int_of_string Sys.argv.(12));
    }
  in
  print_endline (bytes_to_hex (Sighash_kernel.sighash_preimage f))
