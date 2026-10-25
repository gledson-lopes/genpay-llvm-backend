#!/usr/bin/env bash
#===-----------------------------------------------------------------------===//
# Genpay LLVM 21 Backend - smoke test runner (no lit/FileCheck required)
#
# Usage:   ./scripts/run_tests.sh /path/to/llvm-project
#
# Stages:
#   1. llc      - .ll -> text assembly, grep for ISA invariants
#   2. llvm-mc  - parse the llc output back (llc | llvm-mc round trip)
#   3. llvm-mc  - encode and emit ELF objects from the hand-written .s tests
#   4. python3  - verify relocation tables where symbols must survive
#   5. ELF identity - e_machine must be EM_GENPAY (0x4750)
#===-----------------------------------------------------------------------===//
set -uo pipefail

LLVM_DIR="${1:?usage: ./scripts/run_tests.sh /path/to/llvm-project}"
BUILD_DIR="${BUILD_DIR:-build-genpay}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LLC="$LLVM_DIR/$BUILD_DIR/bin/llc"
MC="$LLVM_DIR/$BUILD_DIR/bin/llvm-mc"

for tool in "$LLC" "$MC"; do
  if [ ! -x "$tool" ]; then
    echo "ERROR: $tool not found (build first with scripts/build.sh)" >&2
    exit 1
  fi
done

pass=0
fail=0

note() { echo "$@"; }
ok()   { pass=$((pass+1)); echo "PASS  $1"; }
bad()  { fail=$((fail+1)); echo "FAIL  $1"; }

# ---------------------------------------------------------------------------
# Stage 1+2: llc text output greps + llvm-mc round trip of the same output.
# ---------------------------------------------------------------------------
run_case() {
  local name="$1"; local file="$2"; shift 2
  local out
  if ! out=$("$LLC" -mtriple=genpay -march=genpay "$file" -o - 2>&1); then
    bad "$name  (llc returned an error)"; return
  fi
  for expect in "$@"; do
    if ! grep -qE "$expect" <<<"$out"; then
      echo "FAIL  $name  (missing pattern: $expect)"
      echo "----- emitted assembly -----"
      echo "$out"
      bad "$name"
      return
    fi
  done
  # Round trip: feed the freshly emitted .s back through llvm-mc.
  if ! grep -v '^#' <<<"$out" | "$MC" -triple=genpay - > /dev/null 2>&1; then
    bad "$name  (llvm-mc round trip failed on llc output)"
    return
  fi
  ok "$name"
}

run_case "add    (ALU add, ret r0)"        "$SCRIPT_DIR/test/add.ll" \
  'add[[:space:]]+r[0-9]+,[[:space:]]r0,[[:space:]]r1' \
  'ret[[:space:]]+r0'

run_case "const  (li r0, 42)"              "$SCRIPT_DIR/test/const.ll" \
  'li[[:space:]]+r0,[[:space:]]42$' \
  'ret[[:space:]]+r0'

run_case "args   (8 args in r0-r7)"        "$SCRIPT_DIR/test/args.ll" \
  'add[[:space:]]+r0,[[:space:]]r0,[[:space:]]r1' \
  'add[[:space:]]+r0,[[:space:]]r0,[[:space:]]r7' \
  'ret[[:space:]]+r0'

run_case "load-s (ADRS tagged load)"       "$SCRIPT_DIR/test/load-s.ll" \
  'load\.s[[:space:]]+r0,[[:space:]]\[r0\]' \
  'load\.s[[:space:]]+r0,[[:space:]]\[r0,[[:space:]]32\]' \
  'ret[[:space:]]+r0'

run_case "atomic (lr.s/sc.s CAS loop)"     "$SCRIPT_DIR/test/atomic-cas.ll" \
  'lr\.s[[:space:]]+r[0-9]+,[[:space:]]\[r0\]' \
  'jne[[:space:]]+r[0-9]+,[[:space:]]r1' \
  'sc\.s[[:space:]]+r[0-9]+,[[:space:]]r2,[[:space:]]\[r0\]' \
  'ret[[:space:]]+r0'

run_case "txdeposit (escrow via b0)"       "$SCRIPT_DIR/test/txdeposit.ll" \
  'txdeposit[[:space:]]+r[0-9]+,[[:space:]]b0,' \
  'ret[[:space:]]+r0'

run_case "crypto-gas (setgas + sha256r)"   "$SCRIPT_DIR/test/crypto-gas.ll" \
  'setgas[[:space:]]+r2' \
  'sha256r[[:space:]]+r0,[[:space:]]r0,[[:space:]]r1' \
  'ret[[:space:]]+r0'

run_case "mem-lifecycle (matk/mdcy/mrel/txrelease from IR)" \
                                           "$SCRIPT_DIR/test/mem-lifecycle.ll" \
  'matk[[:space:]]+r[0-9]+,[[:space:]]r0' \
  'mdcy[[:space:]]+r[0-9]+,[[:space:]]r0' \
  'mrel[[:space:]]+r0$' \
  'txrelease[[:space:]]+r0,[[:space:]]r1'

# ---------------------------------------------------------------------------
# Stage 3: hand-written .s tests through the full MC pipeline.
# ---------------------------------------------------------------------------
ASM_DIR="$SCRIPT_DIR/test/asm"
tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT

if "$MC" -triple=genpay "$ASM_DIR/full.s" -o "$tmpdir/full.echo" 2>"$tmpdir/full.err"; then
  # nop must canonicalize to the encoding NOP after the round trip.
  if grep -qE 'addi[[:space:]]+r0,[[:space:]]r0,[[:space:]]0$' "$tmpdir/full.echo"; then
    ok "asm/full.s  (parse + print, nop alias)"
  else
    bad "asm/full.s  (nop alias missing after round trip)"
  fi
else
  bad "asm/full.s  (llvm-mc parse failed)"
  cat "$tmpdir/full.err"
fi

if "$MC" -triple=genpay -filetype=obj "$ASM_DIR/branch.s" \
     -o "$tmpdir/branch.o" 2>"$tmpdir/branch.err"; then
  # jeq r0, r1, done is the 3rd instruction; done is 8 words away -> 0x44010008
  word=$(python3 - "$tmpdir/branch.o" <<'PYEOF'
import struct, sys
data = open(sys.argv[1], 'rb').read()
e_shoff, = struct.unpack_from('<Q', data, 0x28)
entsz, num, strndx = struct.unpack_from('<HHH', data, 0x3a)
for i in range(num):
    off = e_shoff + i * entsz
    name, typ, flags, addr, offset, size, link, info, align, entsize = \
        struct.unpack_from('<IIQQQQIIQQ', data, off)
    if typ == 1:  # SHT_PROGBITS -> .text
        print(format(struct.unpack_from('<I', data, offset + 8)[0], 'x'))
        break
PYEOF
)
  if [ "$word" = "44010008" ]; then
    ok "asm/branch.s  (obj emission + branch displacement)"
  else
    bad "asm/branch.s  (jeq displacement mismatch: 0x$word)"
  fi
else
  bad "asm/branch.s  (obj emission failed)"
  cat "$tmpdir/branch.err"
fi

if "$MC" -triple=genpay -filetype=obj "$ASM_DIR/reloc.s" \
     -o "$tmpdir/reloc.o" 2>"$tmpdir/reloc.err"; then
  nrelocs=$(python3 - "$tmpdir/reloc.o" <<'PYEOF'
import struct, sys
data = open(sys.argv[1], 'rb').read()
e_shoff, = struct.unpack_from('<Q', data, 0x28)
entsz, num, strndx = struct.unpack_from('<HHH', data, 0x3a)
count = 0
for i in range(num):
    off = e_shoff + i * entsz
    name, typ, flags, addr, offset, size, link, info, align, entsize = \
        struct.unpack_from('<IIQQQQIIQQ', data, off)
    if typ in (9, 4):
        count += size // 24
print(count)
PYEOF
)
  if [ "$nrelocs" -ge 4 ]; then
    ok "asm/reloc.s  (obj emission + $nrelocs relocations recorded)"
  else
    bad "asm/reloc.s  (expected >=4 relocations, got $nrelocs)"
  fi
else
  bad "asm/reloc.s  (obj emission failed)"
  cat "$tmpdir/reloc.err"
fi

# li long form: a 64-bit constant must assemble to the 12-byte encoding.
printf 'li r0, 4294967295\n' > "$tmpdir/bigli.s"
if "$MC" -triple=genpay -filetype=obj "$tmpdir/bigli.s" \
     -o "$tmpdir/bigli.o" 2>/dev/null; then
  tsize=$(python3 - "$tmpdir/bigli.o" <<'PYEOF'
import struct, sys
data = open(sys.argv[1], 'rb').read()
e_shoff, = struct.unpack_from('<Q', data, 0x28)
entsz, num, strndx = struct.unpack_from('<HHH', data, 0x3a)
for i in range(num):
    off = e_shoff + i * entsz
    name, typ, flags, addr, offset, size, link, info, align, entsize = \
        struct.unpack_from('<IIQQQQIIQQ', data, off)
    if typ == 1:  # SHT_PROGBITS -> .text
        print(size)
        break
PYEOF
)
  if [ "$tsize" = "12" ]; then
    ok "li long form  (64-bit immediate encodes to 12 bytes)"
  else
    bad "li long form  (.text size $tsize, expected 12)"
  fi
else
  bad "li long form  (64-bit immediate failed to encode)"
fi

# ---------------------------------------------------------------------------
# Stage 5: ELF identity - the header must self-identify as Genpay.
# ---------------------------------------------------------------------------
if [ -f "$tmpdir/branch.o" ]; then
  em=$(python3 -c 'import struct,sys; print(hex(struct.unpack_from("<H", open(sys.argv[1], "rb").read(), 0x12)[0]))' "$tmpdir/branch.o")
  if [ "$em" = "0x4750" ]; then
    ok "elf e_machine  (EM_GENPAY 0x4750 stamped in the ELF header)"
  else
    bad "elf e_machine  (expected 0x4750, got $em)"
  fi
else
  bad "elf e_machine  (no object emitted to inspect)"
fi

echo ""
echo "Result: $pass passed, $fail failed"
[ "$fail" -eq 0 ]
