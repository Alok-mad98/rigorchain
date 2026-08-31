(* apply_delta CLI. Args: height subsidy mint converge burn ordinary authorized invalid
   Ordinary/authorized/invalid are 0/1. Prints ledger circulating res feeburn or REJECT. *)

let rec pos_of_int i =
  if i = 1 then Supply_kernel.XH
  else if i land 1 = 1 then Supply_kernel.XI (pos_of_int (i lsr 1))
  else Supply_kernel.XO (pos_of_int (i lsr 1))

let z_of_int n =
  if n = 0 then Supply_kernel.Z0
  else if n > 0 then Supply_kernel.Zpos (pos_of_int n)
  else Supply_kernel.Zneg (pos_of_int (-n))

let rec int_of_pos = function
  | Supply_kernel.XH -> 1
  | Supply_kernel.XO p -> int_of_pos p lsl 1
  | Supply_kernel.XI p -> (int_of_pos p lsl 1) + 1

let int_of_z = function
  | Supply_kernel.Z0 -> 0
  | Supply_kernel.Zpos p -> int_of_pos p
  | Supply_kernel.Zneg p -> -int_of_pos p

let bool_of_s s = s <> "0"

let () =
  if Array.length Sys.argv <> 9 then (
    prerr_endline
      "usage: supply_cli height subsidy mint converge burn ordinary authorized invalid";
    exit 2);
  let d : Supply_kernel.blockDelta =
    {
      bd_height = z_of_int (int_of_string Sys.argv.(1));
      bd_subsidy = z_of_int (int_of_string Sys.argv.(2));
      bd_valid_mint = z_of_int (int_of_string Sys.argv.(3));
      bd_converge = z_of_int (int_of_string Sys.argv.(4));
      bd_fee_burn = z_of_int (int_of_string Sys.argv.(5));
      bd_ordinary = bool_of_s Sys.argv.(6);
      bd_mint_authorized = bool_of_s Sys.argv.(7);
      bd_has_invalid = bool_of_s Sys.argv.(8);
    }
  in
  match Supply_kernel.apply_delta Supply_kernel.genesis_supply d with
  | None -> print_endline "REJECT"
  | Some s ->
      Printf.printf "%d %d %d %d\n" (int_of_z s.us_ledger_oru)
        (int_of_z s.us_circulating_oru) (int_of_z s.us_res)
        (int_of_z s.us_fee_burn_acc)
