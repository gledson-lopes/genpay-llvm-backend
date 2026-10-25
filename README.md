# Genpay LLVM 21 Backend — v2.3 (Smart-Contract ISA, full MC pipeline, ELF identity, complete IR surface)

Production-grade experimental LLVM backend for the **Genpay Smart-Contract ISA**,
validated end-to-end against **LLVM 21.1.6** (built and tested on x86_64 with an
assertions-enabled build; API-compatible with the aarch64 devcontainer checkout
used for v1.7, and with the macOS build used for llvm-mc testing).

v2.0 added the smart-contract hardware surface (ADRS tagged memory, ledger
registers b0–b7, hardware LL/SC atomics, gas meter g0, sha256r) on top of the
v1.7 core (r0–r14 GPRs, ALU ops, ret, the r0–r7 / r0 calling convention).

## What's new in v2.3 — ISA spec conformance: the lifecycle reaches IR

Against the ISA specification's own issue list:

1. **Incomplete memory lifecycle patterns — FIXED.** `matk`, `mdcy`, `mrel`
   (and, in the same class, `txrelease`) were defined with empty pattern
   lists and were unreachable from IR. The backend now selects them from
   three new intrinsics, following the same proven selection style as
   `lr.s` / `setgas`:
   * `matk`  ← `i64 @llvm.genpay.mem.mark(i64 %size)` (returns region base)
   * `mdcy`  ← `i64 @llvm.genpay.mem.decay(i64 %addr)` (returns pre-decay tag)
   * `mrel`  ← `void @llvm.genpay.mem.release(i64 %addr)`
   * `txrelease` ← `void @llvm.genpay.tx.release(i64 %target, i64 %escrow)`
     (the intrinsic existed but had no pattern — wired now)
   Every hardware instruction except the deliberately pattern-less `MOVrr`
   is now compiler-reachable. New lit test: `test/mem-lifecycle.ll`.
2. **load.s assembly string asymmetry — RESOLVED BY DESIGN.** The spec's
   TableGen fragment writes `"load.s\tdst,[src, $offset]"`, where `$` is a
   TableGen operand *reference*, not a printed character. All Genpay
   templates use the consistent `$dst / $addr / $off` reference style and
   print bare registers (`load.s r0, [r0, 32]`) — exactly the form the
   spec's own lit tests expect. The explicit zero-offset text form
   `load.s r0, [r0, 0]` also assembles (it selects `LOADSO`).
3. **TXDEPOSIT register-class synchronization — RESOLVED BY DESIGN.** The
   spec's `(outs LGR:$escrow)` result is unallocatable (ledger registers
   are reserved, never allocated), and its 3-operand text form hides the
   destination. The backend keeps the lossless 4-operand form
   (`txdeposit $dst, b0, $source, $amount`) with `Defs=[B0]`, so the LGR
   class stays perfectly synchronized with SelectionDAG: the register
   allocator never sees a value in the ledger file.

The four specification lit tests (`load-s.ll`, `atomic-cas.ll`,
`txdeposit.ll`, `crypto-gas.ll`) ship with hardware-accurate CHECK lines:
the ISA has no `cmp` (comparisons fold into `jne`/`jz`), `sc.s` returns the
success flag (loop via `jnz`), and `txdeposit` deposits directly into the
GPR result — so the CHECKs differ from the spec's idealized text exactly
where the spec's text did not model the hardware.

## What's new in v2.2 — Genpay ELF machine identity

`file full.o` no longer reports "no machine". Genpay objects now carry a
real architecture ID in the ELF header:

* `e_machine = EM_GENPAY = 0x4750` — ASCII "GP", an unassigned 16-bit number
  that can never collide with an officially registered architecture.
* `integrate.sh` registers the constant in `llvm/BinaryFormat/ELF.h`
  (idempotent patch `[3c]`, anchored on `EM_LOONGARCH`); `GenpayELFObjectWriter`
  passes `ELF::EM_GENPAY` to `MCELFObjectTargetWriter`, and
  `ELFObjectWriter::writeHeader` writes `getEMachine()` verbatim into the
  header.
* `scripts/run_tests.sh` gained a stage 5 check: every run asserts
  `e_machine == 0x4750` on an emitted object.

Tool behavior after the fix: `llvm-readelf -h full.o` shows `Machine: 0x4750`;
`file full.o` prints `*unknown arch 0x4750*` instead of `no machine`
(libmagic has no "Genpay" name entry — the identity is the numeric value,
which is exactly what a Genpay-aware loader or linker validates).

## What's new in v2.1 — the assembler pipeline

The backend now owns the **complete MC layer**, so `llvm-mc -triple=genpay`
works for the first time:

```
.ll  --llc-->  .s  --llvm-mc-->  text  (round trip)
                       |--show-encoding--> binary encodings
                       |--filetype=obj--->  ELF object files (.o)
```

New components:

| File | Role |
|---|---|
| `AsmParser/GenpayAsmParser.cpp` | hand-written target parser (all 27 operand forms, `nop` alias, case-insensitive mnemonics, precise diagnostics) |
| `MCTargetDesc/GenpayMCCodeEmitter.cpp` | fixed 32-bit encoding + variable-length `li`, fixup creation |
| `MCTargetDesc/GenpayAsmBackend.cpp` | fixup application, relocation recording, NOP padding |
| `MCTargetDesc/GenpayELFObjectWriter.cpp` | ELF relocation types (RELA) |
| `MCTargetDesc/GenpayFixupKinds.h` | fixup kinds + experimental reloc numbers |

### Binary encoding (32-bit fixed words, little-endian)

```
 R-format:  31    26 25    21 20    16 15    11 10     0
            | op   |  dst   |  src1  |  src2  | unused |
 I-format:  31    26 25    21 20    16 15              0
            | op   |  dst   |  src1  |      imm16     |
```

Register fields hold the hardware encoding: r0–r14 = 0–14, sp = 15,
b0–b7 = 16–23, g0 = 24 (5-bit fields). The imm16 field overlaps the src2
field; the opcode decides the format. Branch displacements are counted in
4-byte instruction words relative to the branch itself (next instruction = 1),
giving a ±131068-byte branch range.

Opcodes (6-bit field):

```
0x01 add   0x02 addi  0x03 sub    0x04 and    0x05 or     0x06 xor
0x07 mov   0x08 li    0x09 lr.s   0x0a sc.s   0x0b setgas 0x0c readgas
0x0d sha256r
0x10 j     0x11 jeq   0x12 jne    0x13 jz     0x14 jnz    0x15 ret
0x20 load.s 0x21 store.s 0x22 matk 0x23 mdcy   0x24 mrel
0x30 txdeposit  0x31 txrelease
```

**`li` is variable-length**: 4 bytes when the immediate fits signed 16 bits;
otherwise 12 bytes (word1 = `op|dst|imm[15:0]`, word2 = `imm[47:16]`,
word3 = `imm[63:48]`). Symbolic immediates (`li r0, some_symbol`) always use
the 12-byte form so relocations never change instruction size. The canonical
NOP is `addi r0, r0, 0` (`0x02000000`); the `nop` alias parses to it.

### Relocations (experimental, RELA)

| Reloc | Fixup | Meaning |
|---|---|---|
| R_GENPAY_IMM16 = 1 | `fixup_genpay_imm16` | bits [15:0] of S+A |
| R_GENPAY_BRTARGET16 = 2 | `fixup_genpay_brtarget16` | (S+A−P)/4, PC-relative |
| R_GENPAY_32 = 3 | `FK_Data_4`, `fixup_genpay_imm32_shift16` | bits [31:0] resp. [47:16] of S+A |
| R_GENPAY_64 = 4 | `FK_Data_8` | S+A |
| R_GENPAY_16 = 6 | `FK_Data_2`, `fixup_genpay_imm16_shift48` | bits [15:0] resp. [63:48] of S+A |

Objects are stamped with `e_machine = EM_GENPAY` (0x4750, ASCII "GP",
registered in `llvm/BinaryFormat/ELF.h` by `integrate.sh`). No public ABI
registry assigns symbolic names for this machine, so `llvm-readobj -r`
lists the numeric relocation types.

### Text-format changes in v2.1 (breaking)

* **`txdeposit` now prints its destination register** (lossless round trip):
  `txdeposit r0, b0, r0, r1`. The v2.0 form `txdeposit b0, r0, r1` could not
  be re-assembled because the destination register was not visible.
* LLVM 21's generic parser no longer accepts `.word`; data directives in
  Genpay assembly use `.long` / `.quad` / `.short`.

## What's in v2.0 (unchanged in v2.1)

| Hardware engine | Registers | Instructions | Compiler entry |
|---|---|---|---|
| ADRS lifecycle tagging | r0–r14 (tag bits [63:60]) | `load.s`, `store.s`, `matk`, `mdcy`, `mrel` | every plain i64 load/store auto-routes through `load.s`/`store.s` |
| Isolated ledger file | b0–b7 (encodings 16–23) | `txdeposit`, `txrelease` | `@llvm.genpay.tx.deposit` intrinsic |
| Hardware atomics (LL/SC) | — | `lr.s`, `sc.s` | `cmpxchg` is auto-expanded by AtomicExpandPass into an lr.s/sc.s loop |
| Loop-breaking gas meter | g0 (encoding 24) | `setgas`, `readgas` | `@llvm.genpay.set.gas` / `@llvm.genpay.read.gas` intrinsics |
| Crypto pipeline | — | `sha256r` | `@llvm.genpay.sha256.r` intrinsic |
| Control flow | — | `j`, `jeq`, `jne`, `jz`, `jnz` | `br`/`icmp`/`br` lowering |

Engineering notes carried over from v2.0: target intrinsics registered in
LLVM core (installed idempotently by `integrate.sh`); AtomicExpandPass
integration (`shouldExpandAtomicCmpXchgInIR` → LLSC,
`setMaxAtomicSizeInBitsSupported(64)`, direct `ISD::BR_CC` matching);
`copyPhysReg` (COPY → `mov`); SelectionDAGInfo fix for the historical ISel
segfault; pattern-less `MOVrr`.

## Directory layout

```
Genpay-LLVM-Backend-LLVM21/
├── integrate.sh                  # one-shot, idempotent integration
├── scripts/
│   ├── build.sh                  # cmake+ninja build of llc + llvm-mc (GENPAY_JOBS-aware)
│   └── run_tests.sh              # smoke suite: llc greps + llvm-mc round trip
│                                 #   + obj emission + relocation checks
├── test/
│   ├── add.ll  const.ll  args.ll         # v1.7 baselines (unchanged output)
│   ├── load-s.ll  atomic-cas.ll          # v2.0 lit tests (RUN: + FileCheck)
│   ├── txdeposit.ll  crypto-gas.ll
│   ├── mem-lifecycle.ll                   # v2.3 ADRS lifecycle + txrelease
│   └── asm/
│       ├── full.s                        # every mnemonic through the parser
│       ├── branch.s                      # labels + in-section branch fixups
│       └── reloc.s                       # symbolic operands -> ELF relocations
└── llvm/
    ├── include/llvm/IR/IntrinsicsGenpay.td
    └── lib/Target/Genpay/         # CodeGen + AsmParser + MCTargetDesc + TargetInfo
```

## Integration & build

```bash
# 1. Integrate into your llvm-project checkout (idempotent, safe to re-run)
./integrate.sh /workspaces/workspace/llvm-project

# 2. Build llc AND llvm-mc (use -j2 on the aarch64 devcontainer to avoid OOM)
GENPAY_JOBS=2 ./scripts/build.sh /workspaces/workspace/llvm-project
cmake --build /workspaces/workspace/llvm-project/build-genpay \
      --target llvm-mc -j2

# 3. Run the smoke suite (llc + llvm-mc + object emission checks)
./scripts/run_tests.sh /workspaces/workspace/llvm-project
```

## Using llvm-mc

```bash
MC=./build-genpay/bin/llvm-mc

# Parse and print (what previously failed with
# "this target does not support assembly parsing")
$MC -triple=genpay test/atomic-cas.s

# Round trip straight from the compiler
llc -mtriple=genpay test/atomic-cas.ll -o - | $MC -triple=genpay -

# Binary encodings
$MC -triple=genpay -show-encoding test/asm/full.s

# ELF object files (with relocations for undefined symbols)
$MC -triple=genpay -filetype=obj test/asm/reloc.s -o reloc.o
llvm-readobj -r reloc.o

# MC-level diagnostics for hand-written assembly
$MC -triple=genpay test/asm/full.s -o -
```

Parser notes: mnemonics are case-insensitive; `r15` is accepted as an alias
for `sp`; `nop` assembles as `addi r0, r0, 0`; ledger (b0–b7) and gas (g0)
registers are rejected in general-purpose operand slots with a targeted
error; `txdeposit` requires the literal `b0` escrow token in position two.

## ISA quick reference (compiler-visible subset)

```asm
add   rD, rA, rB        addi  rD, rA, imm16     sub/and/or/xor  rD, rA, rB
mov   rD, rA            li    rD, imm64
load.s  rD, [rA]        load.s  rD, [rA, off16]
store.s rS, [rA]        store.s rS, [rA, off16]
lr.s  rD, [rA]          sc.s  rD, rV, [rA]     ; rD = 0 on success
txdeposit rD, b0, rS, rA  txrelease rT, rE
setgas  rS              readgas rD
sha256r rD, rA, rL      matk/mdcy/mrel
j     label             jeq/jne rA, rB, label      jz/jnz rA, label
ret   rA
```

Calling convention (unchanged from v1.7): first 8 i64 args in r0–r7, return
value in r0. r15 = sp, b0–b7 and g0 are reserved (never allocated).

## Test expectations (validated outputs)

```
add.ll    -> add r0, r0, r1 ; ret r0
const.ll  -> li r0, 42 ; ret r0
args.ll   -> add r0, r0, r1 ... add r0, r0, r7 ; ret r0
load-s.ll -> load.s r0, [r0] / load.s r0, [r0, 32] ; ret r0
atomic-cas.ll -> lr.s r3, [r0] ; jne r3, r1, exit ; sc.s r5, r2, [r0] ;
                 jnz r5, loop ; mov r0, r3 ; ret r0
txdeposit.ll  -> txdeposit r0, b0, r0, r1 ; ret r0     (v2.1 text form)
crypto-gas.ll -> setgas r2 ; sha256r r0, r0, r1 ; ret r0
mem-lifecycle.ll -> matk rX, r0 ; mdcy rX, r0 ; mrel r0 ;
                    txrelease r0, r1   (X = allocated GPR)
asm/full.s    -> parses + reprints all 27 forms; nop -> addi r0, r0, 0
asm/branch.s  -> obj with resolved in-section displacements (jeq = 0x44010008)
asm/reloc.s   -> obj with R_GENPAY_IMM16 / _BRTARGET16 / _32 relocations
```

## Known limitations

* Stack frames / FrameIndex elimination not implemented (leaf, register-only
  functions; >8 args or spills will abort).
* Calls (LowerCall), 64-bit multiply/divide (libcall path), and
  select/shifts are unimplemented.
* Only `jeq`/`jne`-class comparisons are pattern-matched; other condcodes
  fall through the i64 setcc path and require `cmp`-style materialization.
* No disassembler yet (`llvm-mc -disassemble` unsupported); add a
  GenpayDisassembler component if objdump-style decoding is needed.
* Relocation numbers are private R_GENPAY_* types in the Genpay machine
  space (`e_machine = 0x4750`); consumers validate objects by comparing the
  numeric e_machine and reloc types (name recognition in `file`/readelf is
  cosmetic and not required).
