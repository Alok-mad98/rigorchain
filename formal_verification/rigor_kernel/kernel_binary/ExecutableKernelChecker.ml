(* ExecutableKernelChecker — executable-model end of the kernel chain.

   Abstract Kernel → Concrete Kernel → Executable Kernel Model
   → extracted checker (rigor-kernel-checker).

   This module names that last Coq-model layer. It is not production
   C++, not a full node, and not a compiler-correctness claim.
*)

let layer = "executable_kernel_model"

let production_cpp = false
let full_node = false
let compiler_correctness = false
