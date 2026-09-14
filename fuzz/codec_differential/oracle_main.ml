(* Coq side of the codec differential.

   Reads  "id \t kind \t hex"  lines on stdin.
   Writes "id \t status \t leftover \t reencoded_hex" on stdout.

   status   ok | fail | badhex | badkind
   leftover bytes the decoder did not consume ("-" when status <> ok)

   The decoder used is [decode_block_from] / [decode_transaction_from],
   which is the prefix-consuming one. Reporting leftover separately lets
   the comparison recover both semantics: the strict Coq decoder accepts
   exactly the ok-with-leftover-0 cases, the wire decoder accepts every
   ok case. Deciding that here would throw away the information the
   comparison needs. *)

module C = Codec_oracle_gen

let rec nat_of_int n = if n <= 0 then C.O else C.S (nat_of_int (n - 1))
let rec int_of_nat = function C.O -> 0 | C.S k -> 1 + int_of_nat k

exception Bad_hex

let hex_val c =
  match c with
  | '0' .. '9' -> Char.code c - Char.code '0'
  | 'a' .. 'f' -> Char.code c - Char.code 'a' + 10
  | 'A' .. 'F' -> Char.code c - Char.code 'A' + 10
  | _ -> raise Bad_hex

let bytes_of_hex s =
  let n = String.length s in
  if n mod 2 <> 0 then raise Bad_hex;
  let rec go i acc =
    if i < 0 then acc
    else go (i - 2) (nat_of_int ((hex_val s.[i] * 16) + hex_val s.[i + 1]) :: acc)
  in
  go (n - 2) []

let hex_of_bytes bs =
  let b = Buffer.create 1024 in
  List.iter (fun x -> Buffer.add_string b (Printf.sprintf "%02x" (int_of_nat x))) bs;
  Buffer.contents b

let emit id status leftover hex =
  Printf.printf "%s\t%s\t%s\t%s\n" id status leftover hex

let handle id kind hex =
  match (try Some (bytes_of_hex hex) with Bad_hex -> None) with
  | None -> emit id "badhex" "-" "-"
  | Some bs -> (
      match kind with
      | "block" -> (
          match C.decode_block_from bs with
          | None -> emit id "fail" "-" "-"
          | Some (b, rest) ->
              emit id "ok"
                (string_of_int (List.length rest))
                (hex_of_bytes (C.encode_block b)))
      | "tx" -> (
          match C.decode_transaction_from bs with
          | None -> emit id "fail" "-" "-"
          | Some (t, rest) ->
              emit id "ok"
                (string_of_int (List.length rest))
                (hex_of_bytes (C.encode_transaction t)))
      | _ -> emit id "badkind" "-" "-")

let () =
  (try
     while true do
       let line = input_line stdin in
       if String.length line > 0 then
         match String.split_on_char '\t' line with
         | [ id; kind; hex ] -> handle id kind hex
         | _ -> ()
     done
   with End_of_file -> ());
  flush stdout
