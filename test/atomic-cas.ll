; RUN: llc -mtriple=genpay -march=genpay < %s | FileCheck %s
;
; Genpay ISA test: hardware atomics. A frontend-generated atomic
; compare-and-swap must be expanded (via AtomicExpandPass) into the
; lock-free lr.s / sc.s reservation loop, so ledger updates never need
; software locks.
;
; CHECK-LABEL: test_atomic_balance_update:
; CHECK:         [[LOOP:LBB0_[0-9]+]]:
; CHECK:         lr.s [[LOADED:r[0-9]+]], [r0]
; CHECK-NEXT:    jne [[LOADED]], r1, [[EXIT:LBB0_[0-9]+]]
; CHECK:         sc.s [[ST:r[0-9]+]], r2, [r0]
; CHECK:         jnz [[ST]], [[LOOP]]
; CHECK-NOT:     sc.s
; CHECK:         mov r0, [[LOADED]]
; CHECK-NEXT:    ret r0
define i64 @test_atomic_balance_update(ptr %addr, i64 %expected, i64 %new) {
entry:
  %pair = cmpxchg ptr %addr, i64 %expected, i64 %new seq_cst seq_cst
  %val = extractvalue { i64, i1 } %pair, 0
  ret i64 %val
}
