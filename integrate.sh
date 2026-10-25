#!/usr/bin/env bash
#===-----------------------------------------------------------------------===//
# Genpay LLVM 21 Backend - v2.3 (Smart-Contract ISA + MC + full IR surface) - One-shot integration
#
# Usage:   ./integrate.sh /path/to/llvm-project
#
# What it does (all steps idempotent):
#   1. Installs llvm/lib/Target/Genpay/        (backend incl. AsmParser + MC)
#   2. Installs llvm/include/llvm/IR/IntrinsicsGenpay.td
#   3. Patches llvm/include/llvm/IR/Intrinsics.td   (registers the intrinsics)
#  3b. Patches llvm/include/llvm/IR/CMakeLists.txt  (IntrinsicsGenpay.h enums)
#  3c. Patches llvm/include/llvm/BinaryFormat/ELF.h (EM_GENPAY = 0x4750)
#   4. Patches llvm/include/llvm/TargetParser/Triple.h   (Triple::genpay)
#   5. Patches llvm/lib/TargetParser/Triple.cpp       (triple parsing hooks)
#   6. Installs lit test cases into llvm/test/CodeGen/Genpay/
#===-----------------------------------------------------------------------===//
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LLVM_DIR="${1:?usage: ./integrate.sh /path/to/llvm-project}"

if [ ! -f "$LLVM_DIR/llvm/CMakeLists.txt" ]; then
  echo "ERROR: $LLVM_DIR does not look like an llvm-project checkout (missing llvm/CMakeLists.txt)" >&2
  exit 1
fi

echo "==> [1/6] Installing target sources into llvm/lib/Target/Genpay/"
rm -rf "$LLVM_DIR/llvm/lib/Target/Genpay"
cp -r "$SCRIPT_DIR/llvm/lib/Target/Genpay" "$LLVM_DIR/llvm/lib/Target/Genpay"

echo "==> [2/6] Installing llvm/include/llvm/IR/IntrinsicsGenpay.td"
cp "$SCRIPT_DIR/llvm/include/llvm/IR/IntrinsicsGenpay.td" \
   "$LLVM_DIR/llvm/include/llvm/IR/IntrinsicsGenpay.td"

python3 - "$LLVM_DIR" <<'PYEOF'
import sys, os

LLVM = sys.argv[1]

def patch(path, needle, insertion, description, before=False):
    with open(path) as f:
        src = f.read()
    if insertion.strip() in src:
        print(f"    {description}: already applied")
        return
    if needle not in src:
        print(f"    WARNING: could not find anchor for {description} in {path}")
        sys.exit(1)
    if before:
        src = src.replace(needle, insertion + needle, 1)
    else:
        src = src.replace(needle, needle + insertion, 1)
    with open(path, "w") as f:
        f.write(src)
    print(f"    {description}: patched")

# --- [3/6] Intrinsics.td: register the Genpay intrinsics -------------------
patch(
    os.path.join(LLVM, "llvm/include/llvm/IR/Intrinsics.td"),
    'include "llvm/IR/IntrinsicsLoongArch.td"\n',
    'include "llvm/IR/IntrinsicsGenpay.td"\n',
    "[3/6] Intrinsics.td",
)

# [3b] llvm/IR/CMakeLists.txt: generate IntrinsicsGenpay.h (the per-target
# enum header that makes Intrinsic::genpay_* visible in C++).
patch(
    os.path.join(LLVM, "llvm/include/llvm/IR/CMakeLists.txt"),
    "tablegen(LLVM IntrinsicsVE.h -gen-intrinsic-enums -intrinsic-prefix=ve)\n",
    "tablegen(LLVM IntrinsicsGenpay.h -gen-intrinsic-enums -intrinsic-prefix=genpay)\n",
    "[3b] llvm/IR/CMakeLists.txt",
)

# --- [3c] BinaryFormat/ELF.h: give Genpay objects a real e_machine ---------
# 0x4750 is ASCII "GP" (Genpay): an unassigned 16-bit number, so it can never
# collide with an officially registered architecture. ELFObjectWriter::
# writeHeader writes TargetObjectWriter->getEMachine() verbatim, so without
# this every Genpay object carries e_machine = EM_NONE ("no machine").
patch(
    os.path.join(LLVM, "llvm/include/llvm/BinaryFormat/ELF.h"),
    "  EM_LOONGARCH = 258,     // LoongArch\n",
    '  EM_GENPAY = 0x4750,     // Genpay Smart-Contract ISA (unofficial, ASCII "GP")\n',
    "[3c] ELF.h e_machine",
)

# --- [4/6] Triple.h: add Triple::genpay to ArchType ------------------------
patch(
    os.path.join(LLVM, "llvm/include/llvm/TargetParser/Triple.h"),
    "    LastArchType = ve\n",
    "    genpay,         // Genpay: Genpay Smart-Contract ISA\n",
    "[4/6] Triple.h ArchType",
    before=True,
)

# --- [5/6] Triple.cpp: parse the genpay triple ------------------------------
patch(
    os.path.join(LLVM, "llvm/lib/TargetParser/Triple.cpp"),
    '          .Case("lanai", Triple::lanai)\n',
    '          .Case("genpay", Triple::genpay)\n',
    "[5/6] Triple.cpp parse",
)
patch(
    os.path.join(LLVM, "llvm/lib/TargetParser/Triple.cpp"),
    '  case hexagon:        return "hexagon";\n',
    '  case genpay:         return "genpay";\n',
    "[5/6] Triple.cpp name",
)
patch(
    os.path.join(LLVM, "llvm/lib/TargetParser/Triple.cpp"),
    "  case llvm::Triple::hsail64:\n",
    "  case llvm::Triple::genpay:\n",
    "[5/6] Triple.cpp pointer width",
)
# getDefaultFormat: genpay always uses ELF object files.
patch(
    os.path.join(LLVM, "llvm/lib/TargetParser/Triple.cpp"),
    "  case Triple::aarch64_be:\n  case Triple::amdgcn:\n",
    "  case Triple::genpay:\n",
    "[5/6] Triple.cpp object format",
)

print("    Triple.cpp: all hooks present")
PYEOF

echo "==> [6/6] Installing lit tests into llvm/test/CodeGen/Genpay/"
mkdir -p "$LLVM_DIR/llvm/test/CodeGen/Genpay"
cp "$SCRIPT_DIR"/test/*.ll "$LLVM_DIR/llvm/test/CodeGen/Genpay/"

echo ""
echo "Integration complete. Now build llc:"
echo "  ./scripts/build.sh $LLVM_DIR"
