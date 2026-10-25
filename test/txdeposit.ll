; RUN: llc -mtriple=genpay -march=genpay < %s | FileCheck %s
;
; Genpay ISA test: token escrow & balance management. The target-specific
; intrinsic llvm.genpay.tx.deposit bypasses generic ALU pipelines and
; compiles straight into the isolated ledger register file instruction
; txdeposit, which uses ledger register b0 as the hardware escrow slot.
;
; CHECK-LABEL: execute_bob_to_alice_escrow:
; CHECK:         txdeposit {{r[0-9]+}}, b0, r0, r1
; CHECK-NEXT:    ret r0
define i64 @execute_bob_to_alice_escrow(i64 %wallet_ptr, i64 %amount) {
entry:
  %escrow_handle = call i64 @llvm.genpay.tx.deposit(i64 %wallet_ptr, i64 %amount)
  ret i64 %escrow_handle
}

declare i64 @llvm.genpay.tx.deposit(i64, i64)
