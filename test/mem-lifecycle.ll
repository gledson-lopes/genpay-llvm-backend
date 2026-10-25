; RUN: llc -mtriple=genpay -march=genpay < %s | FileCheck %s
;
; Genpay ISA test: ADRS memory lifecycle management. The spec defines
; matk / mdcy / mrel with empty pattern lists (assembler-only); the backend
; completes the surface with the @llvm.genpay.mem.* intrinsics so the full
; lifecycle (mark -> sustain -> decay / release) is reachable from IR.
; txrelease completes the ledger surface the same way, selecting from the
; pre-existing llvm.genpay.tx.release intrinsic. All functions return i64
; (ret void is not modeled by the Genpay RET pattern yet).
;
; NOTE: source operands are the incoming argument registers (r0, r1) and
; are stable; destinations are wildcarded because the register allocator
; may assign any free GPR to a fresh value.

declare i64 @llvm.genpay.mem.mark(i64)
declare i64 @llvm.genpay.mem.decay(i64)
declare void @llvm.genpay.mem.release(i64)
declare void @llvm.genpay.tx.release(i64, i64)

; Mark a region: ADRS allocates the lines and returns the region base,
; which is born in the Sustain (0x3) lifecycle phase.
;
; CHECK-LABEL: open_region:
; CHECK:         matk [[DST:r[0-9]+]], r0
; CHECK-NEXT:    ret [[DST]]
define i64 @open_region(i64 %size) {
entry:
  %base = call i64 @llvm.genpay.mem.mark(i64 %size)
  ret i64 %base
}

; Decay a line: it leaves the Sustain phase and the instruction returns
; the tag word as it was before the decay.
;
; CHECK-LABEL: decay_line:
; CHECK:         mdcy [[DST:r[0-9]+]], r0
; CHECK-NEXT:    ret [[DST]]
define i64 @decay_line(i64 %addr) {
entry:
  %tag = call i64 @llvm.genpay.mem.decay(i64 %addr)
  ret i64 %tag
}

; Release a line back to the ADRS free pool.
;
; CHECK-LABEL: release_line:
; CHECK:         mrel r0
; CHECK-NEXT:    li r0, 0
; CHECK-NEXT:    ret r0
define i64 @release_line(i64 %addr) {
entry:
  call void @llvm.genpay.mem.release(i64 %addr)
  ret i64 0
}

; Settle an escrow: release the escrow handle back to the target wallet.
;
; CHECK-LABEL: settle_escrow:
; CHECK:         txrelease r0, r1
; CHECK-NEXT:    ret r0
define i64 @settle_escrow(i64 %target, i64 %escrow) {
entry:
  call void @llvm.genpay.tx.release(i64 %target, i64 %escrow)
  ret i64 %target
}
