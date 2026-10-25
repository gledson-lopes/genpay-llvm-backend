//===-- GenpayMCAsmInfo.h - Genpay Asm Info -------------------*- C++ -*-===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCASMINFO_H
#define LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCASMINFO_H

#include "llvm/MC/MCAsmInfo.h"
#include "llvm/TargetParser/Triple.h"

namespace llvm {

class GenpayMCAsmInfo : public MCAsmInfo {
public:
  explicit GenpayMCAsmInfo(const Triple &TT) {
    CommentString = "#";
    PrivateGlobalPrefix = ".L";
  }
};

} // namespace llvm

#endif
