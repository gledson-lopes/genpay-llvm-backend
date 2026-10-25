//===-- GenpayFixupKinds.h - Genpay Fixup Kinds ---------------*- C++ -*--===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYFIXUPKINDS_H
#define LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYFIXUPKINDS_H

#include "llvm/MC/MCFixup.h"

namespace llvm {
namespace Genpay {

enum Fixups {
  // 16-bit PC-relative branch displacement, expressed in 4-byte instruction
  // words. Used by j / jeq / jne / jz / jnz. The displacement is measured
  // from the address of the branch instruction itself, so a branch to the
  // immediately following instruction has displacement 1.
  fixup_genpay_brtarget16 = FirstTargetFixupKind,

  // 16-bit absolute immediate field (li, addi, load.s/store.s offsets).
  // Applied against a symbol when the immediate is not a plain constant.
  fixup_genpay_imm16,

  // LI long form (12-byte variable-length li): bits [47:16] and [63:48] of
  // the 64-bit value. The short imm16 field keeps bits [15:0].
  fixup_genpay_imm32_shift16,
  fixup_genpay_imm16_shift48,

  // Marker.
  fixup_genpay_last = fixup_genpay_imm16_shift48,
};

// Number of target-specific fixup kinds (fixup_genpay_last is not counted).
enum { NumTargetFixupKinds = fixup_genpay_last - FirstTargetFixupKind + 1 };

// ELF relocation numbers used by GenpayELFObjectWriter. These are private
// numbers in the Genpay machine space (e_machine = EM_GENPAY = 0x4750,
// registered in llvm/BinaryFormat/ELF.h by integrate.sh). No public ABI
// registry assigns symbolic names for this machine, so llvm-readobj prints
// the numeric types; loaders validate objects via e_machine == 0x4750.
enum GenpayReloc {
  R_GENPAY_NONE = 0,
  R_GENPAY_BRTARGET16 = 1,
  R_GENPAY_IMM16 = 2,
  R_GENPAY_32 = 3,
  R_GENPAY_64 = 4,
  R_GENPAY_8 = 5,
  R_GENPAY_16 = 6,
};

} // namespace Genpay
} // namespace llvm

#endif
