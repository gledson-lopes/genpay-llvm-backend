# Genpay Smart-Contract ISA - full mnemonic coverage for the assembler.
#
# Exercises every instruction the parser accepts:
#   llvm-mc -triple=genpay full.s
#   llvm-mc -triple=genpay -show-encoding full.s
#   llvm-mc -triple=genpay -filetype=obj full.s -o full.o

    .text
    .globl  full_coverage
full_coverage:
# --- ALU core ---
    add     r0, r1, r2
    addi    r3, r1, 100
    addi    r3, r1, -5
    sub     r4, r2, r1
    and     r5, r1, r2
    or      r5, r1, r2
    xor     r5, r1, r2
    mov     r6, r1
    li      r7, 42
    nop

# --- ADRS tagged memory ---
    load.s  r0, [r1]
    load.s  r0, [r1, 32]
    store.s r0, [r1]
    store.s r0, [r1, -8]

# --- ADRS lifecycle ---
    matk    r0, r1
    mdcy    r0, r1
    mrel    r1

# --- Ledger engine ---
    txdeposit r0, b0, r1, r2
    txrelease r1, r2

# --- Hardware atomics ---
    lr.s    r0, [r1]
    sc.s    r0, r2, [r1]

# --- Gas engine ---
    setgas  r1
    readgas r2

# --- Crypto pipeline ---
    sha256r r0, r1, r2

    ret     r0
