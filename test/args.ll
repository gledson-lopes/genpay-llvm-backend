; Genpay smoke test 3: calling convention (8 register args r0-r7)
define i64 @sum8(i64 %a0, i64 %a1, i64 %a2, i64 %a3,
                 i64 %a4, i64 %a5, i64 %a6, i64 %a7) {
entry:
  %s1 = add i64 %a0, %a1
  %s2 = add i64 %s1, %a2
  %s3 = add i64 %s2, %a3
  %s4 = add i64 %s3, %a4
  %s5 = add i64 %s4, %a5
  %s6 = add i64 %s5, %a6
  %s7 = add i64 %s6, %a7
  ret i64 %s7
}
