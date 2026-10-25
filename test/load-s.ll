; RUN: llc -mtriple=genpay -march=genpay < %s | FileCheck %s
;
; Genpay ISA test: ADRS tagged memory safety.
; Every ordinary i64 load must be routed through the ADRS engine's
; load.s, which traps natively when the addressed line is not in the
; Sustain (0x3) lifecycle phase.

; CHECK-LABEL: test_sustain_load:
; CHECK:         load.s r0, [r0]
; CHECK-NEXT:    ret r0
define i64 @test_sustain_load(ptr %ptr) {
entry:
  %val = load i64, ptr %ptr, align 8
  ret i64 %val
}

; CHECK-LABEL: test_sustain_load_offset:
; CHECK:         load.s r0, [r0, 32]
; CHECK-NEXT:    ret r0
define i64 @test_sustain_load_offset(ptr %ptr) {
entry:
  %gep = getelementptr i64, ptr %ptr, i64 4
  %val = load i64, ptr %gep, align 8
  ret i64 %val
}
