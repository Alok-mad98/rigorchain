(* Driver for the extracted Coq SHA-256 / SHA-256d kernel.
   Usage: hash256_cli sha256|sha256d <hex-payload>
   Empty payload: pass the empty string as the second argument.
   This binary is the Coq kernel compiled to OCaml. It is not C++ CHash256. *)

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

let () =
  if Array.length Sys.argv < 2 then (
    prerr_endline "usage: hash256_cli sha256|sha256d [hex]";
    exit 2);
  let mode = Sys.argv.(1) in
  let hex = if Array.length Sys.argv > 2 then Sys.argv.(2) else "" in
  let msg = hex_to_bytes hex in
  let out =
    match mode with
    | "sha256" -> Hash256_kernel.sha256_bytes msg
    | "sha256d" | "hash256" -> Hash256_kernel.hash256_bytes msg
    | _ -> failwith "mode must be sha256 or sha256d"
  in
  print_endline (bytes_to_hex out)
