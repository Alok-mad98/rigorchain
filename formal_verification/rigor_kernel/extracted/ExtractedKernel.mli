
val negb : bool -> bool

type nat =
| O
| S of nat

val fst : ('a1 * 'a2) -> 'a1

val length : 'a1 list -> nat

val app : 'a1 list -> 'a1 list -> 'a1 list

type comparison =
| Eq
| Lt
| Gt

val compOpp : comparison -> comparison

val add : nat -> nat -> nat

val mul : nat -> nat -> nat

val sub : nat -> nat -> nat

module Nat :
 sig
  val eqb : nat -> nat -> bool

  val leb : nat -> nat -> bool

  val ltb : nat -> nat -> bool
 end

type positive =
| XI of positive
| XO of positive
| XH

type n =
| N0
| Npos of positive

type z =
| Z0
| Zpos of positive
| Zneg of positive

module Pos :
 sig
  type mask =
  | IsNul
  | IsPos of positive
  | IsNeg
 end

module Coq_Pos :
 sig
  val succ : positive -> positive

  val add : positive -> positive -> positive

  val add_carry : positive -> positive -> positive

  val pred_double : positive -> positive

  val pred_N : positive -> n

  type mask = Pos.mask =
  | IsNul
  | IsPos of positive
  | IsNeg

  val succ_double_mask : mask -> mask

  val double_mask : mask -> mask

  val double_pred_mask : positive -> mask

  val sub_mask : positive -> positive -> mask

  val sub_mask_carry : positive -> positive -> mask

  val mul : positive -> positive -> positive

  val iter : ('a1 -> 'a1) -> 'a1 -> positive -> 'a1

  val div2 : positive -> positive

  val div2_up : positive -> positive

  val size : positive -> positive

  val compare_cont : comparison -> positive -> positive -> comparison

  val compare : positive -> positive -> comparison

  val eqb : positive -> positive -> bool

  val testbit : positive -> n -> bool
 end

module N :
 sig
  val succ_double : n -> n

  val double : n -> n

  val sub : n -> n -> n

  val compare : n -> n -> comparison

  val leb : n -> n -> bool

  val pos_div_eucl : positive -> n -> n * n

  val testbit : n -> n -> bool
 end

val rev : 'a1 list -> 'a1 list

val existsb : ('a1 -> bool) -> 'a1 list -> bool

val forallb : ('a1 -> bool) -> 'a1 list -> bool

module Z :
 sig
  val double : z -> z

  val succ_double : z -> z

  val pred_double : z -> z

  val pos_sub : positive -> positive -> z

  val add : z -> z -> z

  val opp : z -> z

  val sub : z -> z -> z

  val mul : z -> z -> z

  val compare : z -> z -> comparison

  val leb : z -> z -> bool

  val ltb : z -> z -> bool

  val eqb : z -> z -> bool

  val of_N : n -> z

  val pos_div_eucl : positive -> z -> z * z

  val div_eucl : z -> z -> z * z

  val div : z -> z -> z

  val modulo : z -> z -> z

  val quotrem : z -> z -> z * z

  val quot : z -> z -> z

  val odd : z -> bool

  val div2 : z -> z

  val log2 : z -> z

  val testbit : z -> z -> bool

  val shiftl : z -> z -> z

  val shiftr : z -> z -> z
 end

type byte = nat

val split_at : nat -> byte list -> (byte list * byte list) option

type amount = z

val cOIN : amount

val mAX_MONEY : amount

val money_range_b : amount -> bool

type hash32 = byte list

type outpoint = { op_txid : hash32; op_index : nat }

type txout = { txo_value : z; txo_spk : byte list }

type tx = { tx_id : hash32; tx_in : outpoint list; tx_out : txout list;
            tx_coinbase : bool }

val bytes_eqb : byte list -> byte list -> bool

val outpoint_eqb : outpoint -> outpoint -> bool

val is_output_op_b : tx -> outpoint -> bool

val inputs_fresh_b : tx -> bool

val sum_values : txout list -> z

val nodupb : outpoint list -> bool

val nonemptyb : 'a1 list -> bool

val valid_tx_b : tx -> bool

type uTXOState = tx

val check_utxo : uTXOState -> bool

type input_domain =
| DomORU
| DomRES

type oru_tx = { tx_dom : input_domain; oru_in : z; oru_out : z; res_in : 
                z; res_out : z }

type kernel_tx = { kt_dom : input_domain; kt_oru_in : z; kt_oru_out : 
                   z; kt_res_in : z; kt_res_out : z; kt_old_supply : 
                   z; kt_subsidy : z }

val to_oru : kernel_tx -> oru_tx

type oRUState = kernel_tx

val valid_oru_tx_b : oru_tx -> bool

val check_oru : oRUState -> bool

type block_height = z

val rES_PRICE_POLE : z

val rES_PRICE_SHIFT : z

val rES_MAX_SUPPLY : z

val res_mint_authorized : block_height -> bool

val res_destroy_pole_sq : z -> z

type res_tx = { rt_coinbase : bool; rt_mint_ok : bool; rt_mint_amount : 
                z; rt_height : z; rt_burn_oru : z; rt_dom : input_domain;
                rt_res_in : z; rt_res_out : z; rt_oru_in : z }

val coinbase : res_tx -> bool

val mint_amount : res_tx -> z

val burn_oru : res_tx -> z

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

val witness_version_of : byte -> nat option

val parse_witness_program : script -> (nat * byte list) option

val prefix4_eq : byte list -> byte list -> bool

val is_native_res_program : byte list -> bool

val is_mint_script : script -> bool

val is_converge_script : script -> bool

val is_push_only_n : nat -> byte list -> bool

val is_push_only : byte list -> bool

val is_null_data : script -> bool

val classify_script : script -> scriptType

type scriptState = { ss_script : script; ss_expected : scriptType }

val check_script : scriptState -> bool

val compact_size : z -> z

val compact_mantissa : z -> z

val compact_decode : z -> z

val raw_size : z -> z

val raw_mantissa : z -> z

val adjust_signbit : z -> z -> z * z

val compact_encode : z -> z

type powHeader = { pow_digest_z : z; pow_target : z; pow_compact_t : z }

val check_pow : powHeader -> bool

val aSERT_RADIX : z

val exp_q_raw : z -> z -> z

type aSERTInput = { asert_te : z; asert_tau : z; asert_expected_q : z }

val check_asert : aSERTInput -> bool

type kernelInput =
| KI_UTXO of uTXOState
| KI_ORU of oRUState
| KI_RES of rESState
| KI_Script of scriptState
| KI_Pow of powHeader
| KI_ASERT of aSERTInput

val validate_kernel : kernelInput -> bool

val extracted_check_utxo : uTXOState -> bool

val extracted_check_oru : oRUState -> bool

val extracted_check_res : rESState -> bool

val extracted_check_script : scriptState -> bool

val extracted_check_pow : powHeader -> bool

val extracted_check_asert : aSERTInput -> bool
