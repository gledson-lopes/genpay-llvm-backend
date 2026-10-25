# Genpay Smart-Contract ISA - branches, labels and loop forms.
#
# All branch targets are in-section labels; they must resolve to
# fixup_genpay_brtarget16 displacements without relocations:
#   llvm-mc -triple=genpay -filetype=obj branch.s -o branch.o

    .text
    .globl  branchy
branchy:
    li      r0, 0
loop:
    addi    r0, r0, 1
    jeq     r0, r1, done
    jne     r0, r2, other
    jz      r3, loop
    jnz     r3, loop
    j       loop
other:
    sub     r0, r0, r4
    j       done
    mov     r0, r5
done:
    ret     r0
