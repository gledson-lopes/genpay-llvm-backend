; RUN: llc -mtriple=genpay -march=genpay < %s | FileCheck %s
;
; Genpay ISA test: gas constraints & cryptographic pipelines. The
; loop-breaking gas meter (register g0) and the hardware SHA-256
; accelerator must be emitted as atomic hardware-level actions via the
; setgas and sha256r instructions.
;
; CHECK-LABEL: secure_gas_and_hash_pipeline:
; CHECK:         setgas r2
; CHECK-NEXT:    sha256r r0, r0, r1
; CHECK-NEXT:    ret r0
define i64 @secure_gas_and_hash_pipeline(i64 %buf_ptr, i64 %len, i64 %budget) {
entry:
  call void @llvm.genpay.set.gas(i64 %budget)
  %hash_handle = call i64 @llvm.genpay.sha256.r(i64 %buf_ptr, i64 %len)
  ret i64 %hash_handle
}

declare void @llvm.genpay.set.gas(i64)
declare i64 @llvm.genpay.sha256.r(i64, i64)
