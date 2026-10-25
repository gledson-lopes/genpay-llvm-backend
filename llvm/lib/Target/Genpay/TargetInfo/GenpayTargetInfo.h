//===-- GenpayTargetInfo.h - Genpay Target Info -------------*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_TARGETINFO_GENPAYTARGETINFO_H
#define LLVM_LIB_TARGET_GENPAY_TARGETINFO_GENPAYTARGETINFO_H

namespace llvm {
class Target;
Target &getTheGenpayTarget();
} // namespace llvm

#endif
