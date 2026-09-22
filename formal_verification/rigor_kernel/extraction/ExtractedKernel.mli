
val negb : bool -> bool

val fst : ('a1 * 'a2) -> 'a1

val length : 'a1 list -> int

val app : 'a1 list -> 'a1 list -> 'a1 list

type comparison =
| Eq
| Lt
| Gt

val add : int -> int -> int

val mul : int -> int -> int

val sub : int -> int -> int

module Nat :
 sig
  val ltb : int -> int -> bool
 end

module Pos :
 sig
  type mask =
  | IsNul
  | IsPos of int
  | IsNeg
 end

module Coq_Pos :
 sig
  val succ : int -> int

  val add : int -> int -> int

  val add_carry : int -> int -> int

  val pred_double : int -> int

  val pred_N : int -> int

  type mask = Pos.mask =
  | IsNul
  | IsPos of int
  | IsNeg

  val succ_double_mask : mask -> mask

  val double_mask : mask -> mask

  val double_pred_mask : int -> mask

  val sub_mask : int -> int -> mask

  val sub_mask_carry : int -> int -> mask

  val mul : int -> int -> int

  val iter : ('a1 -> 'a1) -> 'a1 -> int -> 'a1

  val div2 : int -> int

  val div2_up : int -> int

  val size : int -> int

  val compare_cont : comparison -> int -> int -> comparison

  val compare : int -> int -> comparison

  val eqb : int -> int -> bool

  val testbit : int -> int -> bool
 end

module N :
 sig
  val succ_double : int -> int

  val double : int -> int

  val sub : int -> int -> int

  val compare : int -> int -> comparison

  val leb : int -> int -> bool

  val pos_div_eucl : int -> int -> int * int

  val testbit : int -> int -> bool
 end

val rev : 'a1 list -> 'a1 list

val existsb : ('a1 -> bool) -> 'a1 list -> bool

val forallb : ('a1 -> bool) -> 'a1 list -> bool

module Z :
 sig
  val double : int -> int

  val succ_double : int -> int

  val pred_double : int -> int

  val pos_sub : int -> int -> int

  val add : int -> int -> int

  val opp : int -> int

  val sub : int -> int -> int

  val mul : int -> int -> int

  val compare : int -> int -> comparison

  val leb : int -> int -> bool

  val ltb : int -> int -> bool

  val eqb : int -> int -> bool

  val of_N : int -> int

  val pos_div_eucl : int -> int -> int * int

  val div_eucl : int -> int -> int * int

  val div : int -> int -> int

  val modulo : int -> int -> int

  val quotrem : int -> int -> int * int

  val quot : int -> int -> int

  val odd : int -> bool

  val div2 : int -> int

  val log2 : int -> int

  val testbit : int -> int -> bool

  val shiftl : int -> int -> int

  val shiftr : int -> int -> int
 end

type byte = int

val split_at : int -> byte list -> (byte list * byte list) option

type amount = int

val cOIN : amount

val mAX_MONEY : amount

val money_range_b : amount -> bool

type hash32 = byte list

type outpoint = { op_txid : hash32; op_index : int }

type txout = { txo_value : int; txo_spk : byte list }

type tx = { tx_id : hash32; tx_in : outpoint list; tx_out : txout list;
            tx_coinbase : bool }

val bytes_eqb : byte list -> byte list -> bool

val outpoint_eqb : outpoint -> outpoint -> bool

val is_output_op_b : tx -> outpoint -> bool

val inputs_fresh_b : tx -> bool

val sum_values : txout list -> int

val nodupb : outpoint list -> bool

val nonemptyb : 'a1 list -> bool

val valid_tx_b : tx -> bool

type uTXOState = tx

val check_utxo : uTXOState -> bool

type input_domain =
| DomORU
| DomRES

type oru_tx = { tx_dom : input_domain; oru_in : int; oru_out : int;
                res_in : int; res_out : int }

type kernel_tx = { kt_dom : input_domain; kt_oru_in : int; kt_oru_out : 
                   int; kt_res_in : int; kt_res_out : int;
                   kt_old_supply : int; kt_subsidy : int }

val to_oru : kernel_tx -> oru_tx

type oRUState = kernel_tx

val valid_oru_tx_b : oru_tx -> bool

val check_oru : oRUState -> bool

type block_height = int

val rES_DESTROY_POLE : int

val rES_DESTROY_SHIFT : int

val rES_MAX_SUPPLY : int

val res_mint_authorized : block_height -> bool

val res_destroy_pole_sq : int -> int

type res_tx = { rt_coinbase : bool; rt_mint_ok : bool; rt_mint_amount : 
                int; rt_height : int; rt_burn_oru : int;
                rt_dom : input_domain; rt_res_in : int; rt_res_out : 
                int; rt_oru_in : int }

val coinbase : res_tx -> bool

val mint_amount : res_tx -> int

val burn_oru : res_tx -> int

type rESState = res_tx

val check_res : rESState -> bool

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

val script_type_eqb : scriptType -> scriptType -> bool

val rIGOR_PUSH_0 : byte

val rIGOR_PUSH_1 : byte

val rIGOR_PUSH_16 : byte

val rIGOR_MARKER_RETURN : byte

val rIGOR_PUSHBYTES_20 : byte

val rIGOR_PUSHDATA1 : byte

val rIGOR_PUSHDATA2 : byte

val rIGOR_PUSHDATA4 : byte

val rIGOR_MARKER_RES_MINT : byte

val rIGOR_MARKER_CONVERGE : byte

val cvg2_tag : byte list

val res2_tag : byte list

val witness_version_of : byte -> int option

val parse_witness_program : script -> (int * byte list) option

val prefix4_eq : byte list -> byte list -> bool

val is_native_res_program : byte list -> bool

val is_mint_script : script -> bool

val is_converge_script : script -> bool

val is_push_only_n : int -> byte list -> bool

val is_push_only : byte list -> bool

val is_null_data : script -> bool

val classify_script : script -> scriptType

type scriptState = { ss_script : script; ss_expected : scriptType }

val check_script : scriptState -> bool

val compact_size : int -> int

val compact_mantissa : int -> int

val compact_decode : int -> int

val raw_size : int -> int

val raw_mantissa : int -> int

val adjust_signbit : int -> int -> int * int

val compact_encode : int -> int

type powHeader = { pow_digest_z : int; pow_target : int; pow_compact_t : int }

val check_pow : powHeader -> bool

val aSERT_RADIX : int

val exp_q_raw : int -> int -> int

type aSERTInput = { asert_te : int; asert_tau : int; asert_expected_q : int }

val check_asert : aSERTInput -> bool

type consensusKernel = { check_utxo0 : (uTXOState -> bool);
                         check_oru0 : (oRUState -> bool);
                         check_res0 : (rESState -> bool);
                         check_script0 : (scriptState -> bool);
                         check_pow0 : (powHeader -> bool);
                         check_asert0 : (aSERTInput -> bool) }

val rigor_kernel : consensusKernel

type kernelValidationRequest =
| KI_UTXO of uTXOState
| KI_ORU of oRUState
| KI_RES of rESState
| KI_Script of scriptState
| KI_Pow of powHeader
| KI_ASERT of aSERTInput

val validate_kernel : kernelValidationRequest -> bool
