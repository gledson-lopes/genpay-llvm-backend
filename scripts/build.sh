#!/usr/bin/env bash
#===-----------------------------------------------------------------------===//
# Genpay LLVM 21 Backend - build helper
#
# Usage:   ./scripts/build.sh /path/to/llvm-project [extra cmake args...]
#
# Environment:
#   GENPAY_JOBS  - number of parallel build jobs (default: nproc)
#                  NOTE: on aarch64 devcontainers with OOM history, use -j2!
#   BUILD_DIR    - build directory name (default: build-genpay)
#===-----------------------------------------------------------------------===//
set -euo pipefail

LLVM_DIR="${1:?usage: ./scripts/build.sh /path/to/llvm-project [extra cmake args...]}"
shift || true
BUILD_DIR="${BUILD_DIR:-build-genpay}"
JOBS="${GENPAY_JOBS:-$(nproc)}"

cd "$LLVM_DIR"

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
  echo "==> Configuring Genpay build in $BUILD_DIR"
  cmake -S llvm -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLVM_OPTIMIZED_TABLEGEN=ON \
    -DLLVM_TARGETS_TO_BUILD=X86 \
    -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=Genpay \
    "$@"
else
  echo "==> Reusing existing configure in $BUILD_DIR (cmake re-run)"
  cmake -S llvm -B "$BUILD_DIR" "$@"
fi

echo "==> Building llc and llvm-mc with -j$JOBS"
cmake --build "$BUILD_DIR" --target llc llvm-mc -j"$JOBS"

echo ""
echo "Build OK: $LLVM_DIR/$BUILD_DIR/bin/llc"
echo "Build OK: $LLVM_DIR/$BUILD_DIR/bin/llvm-mc"
