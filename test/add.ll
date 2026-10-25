; Genpay smoke test 1: ALU add
define i64 @add(i64 %a, i64 %b) {
entry:
  %c = add i64 %a, %b
  ret i64 %c
}
