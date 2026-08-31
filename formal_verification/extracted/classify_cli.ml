(* Stdin hex lines -> ORU|RES|NONE|INVALID. Coq AssetClassification.classify.

   Empty scriptPubKey is the line "-" (never a blank line: blanks are skipped).
   The same encoding is used by cpp_classify_cli.cpp and classify_triple_diff.py.
*)

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

let asset_tag = function
  | Classify_kernel.AssetORU -> "ORU"
  | Classify_kernel.AssetRES -> "RES"
  | Classify_kernel.AssetNone -> "NONE"
  | Classify_kernel.AssetInvalid -> "INVALID"

let asset_byte = function
  | Classify_kernel.AssetORU -> '\x00'
  | Classify_kernel.AssetRES -> '\x01'
  | Classify_kernel.AssetNone -> '\x02'
  | Classify_kernel.AssetInvalid -> '\x04'

let rec bytes_to_hex = function
  | [] -> ""
  | n :: rest -> Printf.sprintf "%02x" n ^ bytes_to_hex rest

let classify_hex hex =
  let bytes = if hex = "-" then [] else hex_to_bytes hex in
  asset_tag (Classify_kernel.classify bytes)

let () =
  if Array.length Sys.argv >= 2 && Sys.argv.(1) = "one" then (
    let hex = if Array.length Sys.argv > 2 then Sys.argv.(2) else "" in
    print_endline (classify_hex hex))
  else
    try
      while true do
        let line = String.trim (input_line stdin) in
        if line <> "" && line.[0] <> '#' then print_endline (classify_hex line)
      done
    with End_of_file -> ()
