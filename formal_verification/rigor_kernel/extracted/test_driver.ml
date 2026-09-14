(* extracted-kernel-checker — run golden vectors on faithfully extracted Coq.
   Calls extracted_check_* (the names in extraction_source_equivalence).
   Z/nat are Coq inductives (no ExtrOcamlZInt). Compiler correctness is
   not claimed. *)
open ExtractedKernel

type json =
  | JNull
  | JBool of bool
  | JInt of int64
  | JStr of string
  | JArr of json list
  | JObj of (string * json) list

let rec skip_ws s i =
  if i >= String.length s then i
  else match s.[i] with
  | ' ' | '\n' | '\r' | '\t' -> skip_ws s (i + 1)
  | _ -> i

let parse_string s i =
  let i = skip_ws s i in
  if i >= String.length s || s.[i] <> '"' then failwith "expected string";
  let buf = Buffer.create 16 in
  let rec loop j =
    if j >= String.length s then failwith "unterminated string";
    match s.[j] with
    | '"' -> (Buffer.contents buf, j + 1)
    | '\\' ->
        if j + 1 >= String.length s then failwith "bad escape";
        Buffer.add_char buf s.[j + 1];
        loop (j + 2)
    | c -> Buffer.add_char buf c; loop (j + 1)
  in
  loop (i + 1)

let parse_number s i =
  let i = skip_ws s i in
  let n = String.length s in
  let start = i in
  let i = if i < n && s.[i] = '-' then i + 1 else i in
  if i >= n || s.[i] < '0' || s.[i] > '9' then failwith "expected number";
  let rec loop j =
    if j < n && s.[j] >= '0' && s.[j] <= '9' then loop (j + 1) else j
  in
  let j = loop i in
  (JInt (Int64.of_string (String.sub s start (j - start))), j)

let rec parse_value s i =
  let i = skip_ws s i in
  if i >= String.length s then failwith "unexpected eof";
  match s.[i] with
  | '"' -> let (str, j) = parse_string s i in (JStr str, j)
  | '-' | '0' .. '9' -> parse_number s i
  | 't' ->
      if String.sub s i 4 = "true" then (JBool true, i + 4)
      else failwith "bad true"
  | 'f' ->
      if String.sub s i 5 = "false" then (JBool false, i + 5)
      else failwith "bad false"
  | 'n' ->
      if String.sub s i 4 = "null" then (JNull, i + 4)
      else failwith "bad null"
  | '[' -> parse_array s (i + 1)
  | '{' -> parse_object s (i + 1)
  | c -> failwith (Printf.sprintf "unexpected %c" c)

and parse_array s i =
  let rec loop i acc =
    let i = skip_ws s i in
    if i >= String.length s then failwith "unterminated array";
    if s.[i] = ']' then (JArr (List.rev acc), i + 1)
    else
      let (v, j) = parse_value s i in
      let j = skip_ws s j in
      if j < String.length s && s.[j] = ',' then loop (j + 1) (v :: acc)
      else if j < String.length s && s.[j] = ']' then
        (JArr (List.rev (v :: acc)), j + 1)
      else failwith "bad array"
  in
  loop i []

and parse_object s i =
  let rec loop i acc =
    let i = skip_ws s i in
    if i >= String.length s then failwith "unterminated object";
    if s.[i] = '}' then (JObj (List.rev acc), i + 1)
    else
      let (k, j) = parse_string s i in
      let j = skip_ws s j in
      if j >= String.length s || s.[j] <> ':' then failwith "expected ':'";
      let (v, j) = parse_value s (j + 1) in
      let j = skip_ws s j in
      if j < String.length s && s.[j] = ',' then loop (j + 1) ((k, v) :: acc)
      else if j < String.length s && s.[j] = '}' then
        (JObj (List.rev ((k, v) :: acc)), j + 1)
      else failwith "bad object"
  in
  loop i []

let parse_json s =
  let (v, i) = parse_value s 0 in
  let i = skip_ws s i in
  if i <> String.length s then failwith "trailing json";
  v

let rec assoc k = function
  | [] -> raise Not_found
  | (kk, v) :: rest -> if kk = k then v else assoc k rest

let obj_field o k =
  match o with
  | JObj fs -> assoc k fs
  | _ -> failwith "expected object"

let opt_field o k =
  match o with
  | JObj fs -> (try Some (assoc k fs) with Not_found -> None)
  | _ -> None

let as_int64 = function JInt n -> n | _ -> failwith "expected int"
let as_bool = function JBool b -> b | _ -> failwith "expected bool"
let as_str = function JStr s -> s | _ -> failwith "expected string"
let as_arr = function JArr xs -> xs | _ -> failwith "expected array"

let rec nat_of_int n =
  if n <= 0 then O else S (nat_of_int (n - 1))

let rec positive_of_int64 n =
  if n = 1L then XH
  else if Int64.logand n 1L = 1L then
    XI (positive_of_int64 (Int64.shift_right_logical n 1))
  else
    XO (positive_of_int64 (Int64.shift_right_logical n 1))

let z_of_int64 n =
  if n = 0L then Z0
  else if n > 0L then Zpos (positive_of_int64 n)
  else Zneg (positive_of_int64 (Int64.neg n))

let nat_of_json v = nat_of_int (Int64.to_int (as_int64 v))
let z_of_json v = z_of_int64 (as_int64 v)
let nat_list v = List.map nat_of_json (as_arr v)

let z_field_or o k default =
  match opt_field o k with
  | Some v -> z_of_json v
  | None -> z_of_int64 default

let parse_domain = function
  | "ORU" -> DomORU
  | "RES" -> DomRES
  | _ -> failwith "dom must be ORU or RES"

let parse_script_type = function
  | "P2PKH" -> T_P2PKH
  | "P2SH" -> T_P2SH
  | "WITNESS_V0" -> T_WITNESS_V0
  | "WITNESS_V1" -> T_WITNESS_V1
  | "NONSTANDARD" -> T_NONSTANDARD
  | "MINT" -> T_MINT
  | "CONVERGE" -> T_CONVERGE
  | "NULL_DATA" -> T_NULL_DATA
  | s -> failwith ("unknown script type " ^ s)

let check_utxo_json o =
  let inputs =
    List.map (fun x ->
      match x with
      | JObj _ ->
          { op_txid = nat_list (obj_field x "txid");
            op_index = nat_of_json (obj_field x "index") }
      | _ -> failwith "bad input")
      (as_arr (obj_field o "inputs"))
  in
  let outputs =
    List.map (fun x ->
      match x with
      | JObj _ ->
          { txo_value = z_of_json (obj_field x "value");
            txo_spk = nat_list (obj_field x "spk") }
      | _ -> failwith "bad output")
      (as_arr (obj_field o "outputs"))
  in
  extracted_check_utxo
    { tx_id = nat_list (obj_field o "tx_id");
      tx_in = inputs;
      tx_out = outputs;
      tx_coinbase = as_bool (obj_field o "coinbase") }

let check_oru_json o =
  extracted_check_oru
    { kt_dom = parse_domain (as_str (obj_field o "dom"));
      kt_oru_in = z_of_json (obj_field o "oru_in");
      kt_oru_out = z_of_json (obj_field o "oru_out");
      kt_res_in = z_of_json (obj_field o "res_in");
      kt_res_out = z_of_json (obj_field o "res_out");
      kt_old_supply = z_of_json (obj_field o "old_supply");
      kt_subsidy = z_of_json (obj_field o "subsidy") }

let check_res_json o =
  extracted_check_res
    { rt_coinbase = as_bool (obj_field o "coinbase");
      rt_mint_ok = as_bool (obj_field o "mint_ok");
      rt_mint_amount = z_of_json (obj_field o "mint_amount");
      rt_height = z_of_json (obj_field o "height");
      rt_burn_oru = z_field_or o "burn_oru" 0L;
      rt_dom = parse_domain (as_str (obj_field o "dom"));
      rt_res_in = z_of_json (obj_field o "res_in");
      rt_res_out = z_of_json (obj_field o "res_out");
      rt_oru_in = z_of_json (obj_field o "oru_in") }

let check_script_json o =
  extracted_check_script
    { ss_script = nat_list (obj_field o "script");
      ss_expected = parse_script_type (as_str (obj_field o "expected")) }

let check_pow_json o =
  extracted_check_pow
    { pow_digest_z = z_of_json (obj_field o "digest");
      pow_target = z_of_json (obj_field o "target");
      pow_compact_t = z_of_json (obj_field o "compact_t") }

let check_asert_json o =
  extracted_check_asert
    { asert_te = z_of_json (obj_field o "te");
      asert_tau = z_of_json (obj_field o "tau");
      asert_expected_q = z_of_json (obj_field o "expected_q") }

let decide o =
  match as_str (obj_field o "domain") with
  | "utxo" -> check_utxo_json o
  | "oru" -> check_oru_json o
  | "res" -> check_res_json o
  | "script" -> check_script_json o
  | "pow" -> check_pow_json o
  | "asert" -> check_asert_json o
  | d -> failwith ("unknown domain " ^ d)

let () =
  if Array.length Sys.argv <> 2 then begin
    prerr_endline "usage: extracted-kernel-checker <vector.json>";
    exit 2
  end;
  let path = Sys.argv.(1) in
  let ch = open_in path in
  let data = really_input_string ch (in_channel_length ch) in
  close_in ch;
  let j = parse_json data in
  if decide j then print_endline "VALID" else print_endline "INVALID"
