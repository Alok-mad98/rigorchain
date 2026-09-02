(* Test-only driver for extracted MakeKernelInput.v.
   Not production. Not a second consensus authority.
   Walks Coq's extracted [all_raw] (same nest as MakeKernelInput.v / C++). *)

module K = Makekernelinput_oracle

let rec count_true = function
  | [] -> 0
  | true :: xs -> 1 + count_true xs
  | false :: xs -> count_true xs

let rec agree rs bs =
  match rs, bs with
  | [], [] -> ()
  | r :: rs, b :: bs ->
      if K.make_crypto_accept r <> b then failwith "domain_crypto_accept disagrees with make_crypto_accept";
      if b && not (K.sighash_path_consistent r) then failwith "accepting raw is path-inconsistent";
      agree rs bs
  | _ -> failwith "all_raw / domain_crypto_accept length mismatch"

let () =
  if K.extracted_kernel_production then failwith "extracted_kernel_production must be false";
  if K.extracted_kernel_linked then failwith "extracted_kernel_linked must be false";
  if K.compcert_binary_verified_kernel then failwith "compcert_binary_verified_kernel must be false";
  if not K.compcert_vst_deferred then failwith "compcert_vst_deferred must be true";
  if not K.vst_scope_makekernelinput then failwith "vst_scope_makekernelinput must be true";
  if K.vst_scope_full_kernel then failwith "vst_scope_full_kernel must be false";
  if not (K.make_crypto_accept K.pin_v0_accept) then failwith "pin_v0_accept";
  if K.make_crypto_accept K.pin_v0_no_witness_flag then failwith "pin_v0_no_witness_flag";
  if not (K.make_crypto_accept K.pin_mint_accept) then failwith "pin_mint_accept";
  if not (K.sighash_path_consistent K.pin_v0_accept) then failwith "pin_v0 path tag";
  if not (K.sighash_path_consistent K.pin_mint_accept) then failwith "pin_mint path tag";
  List.iter (fun r ->
      if K.make_crypto_accept r then failwith "tag mismatch accepted";
      if K.sighash_path_consistent r then failwith "tag mismatch marked consistent")
    K.tag_mismatch_pins;
  if List.length K.tag_mismatch_pins < 3 then failwith "tag_mismatch_pins too small";
  let all = K.all_raw in
  let mapped = K.domain_crypto_accept in
  let n = List.length all in
  if n <> 9216 then failwith (Printf.sprintf "domain size %d want 9216" n);
  if List.length mapped <> n then failwith "domain_crypto_accept length";
  agree all mapped;
  let trues = count_true mapped in
  if trues <> 300 then failwith (Printf.sprintf "crypto true %d want 300" trues);
  print_endline "EXTRACTED_KERNEL_PRODUCTION=FALSE";
  print_endline "EXTRACTED_KERNEL_LINKED=FALSE";
  print_endline "EXTRACTED_KERNEL_TEST_ORACLE=PASS";
  Printf.printf "EXTRACTED_ORACLE_DOMAIN=%d\n" n;
  Printf.printf "EXTRACTED_ORACLE_CRYPTO_TRUE=%d\n" trues;
  print_endline "EXTRACTED_ORACLE_USES_ALL_RAW=PASS";
  print_endline "COMPCERT_VST_DEFERRED=TRUE";
  print_endline "COMPCERT_VST_CANDIDATE=MakeKernelInput";
  print_endline "COMPCERT_BINARY_VERIFIED_KERNEL=FALSE"
