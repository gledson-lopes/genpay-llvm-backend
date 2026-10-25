# Genpay Smart-Contract ISA - symbolic operands produce ELF relocations.
#
# external_hook is undefined here, so every reference must survive into the
# object file as a relocation:
#   llvm-mc -triple=genpay -filetype=obj reloc.s -o reloc.o
#   llvm-readobj -r reloc.o   ->  R_GENPAY_IMM16 / R_GENPAY_BRTARGET16 /
#                                 R_GENPAY_32 entries
#
# NOTE: LLVM 21's generic parser no longer accepts `.word`; use
# `.long` / `.quad` / `.short` for data.

    .text
    .globl  reloc_user
reloc_user:
    li      r0, external_hook
    addi    r1, r1, external_hook
    j       external_hook
    .long   external_hook
    .long   0xdeadbeef
    ret     r0
