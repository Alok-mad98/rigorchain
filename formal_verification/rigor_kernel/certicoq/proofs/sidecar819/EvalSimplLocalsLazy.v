From Phase10 Require Import ClosedCompCertOru.
From compcert Require Import Compopts.
(** Compute everything except the debug Parameter, then inspect. *)
Timeout 600 Eval cbv -[Compopts.debug] in simpllocals_ok.
