//===-- GenpayTargetInfo.cpp - Genpay Target Information ----*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayTargetInfo.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

Target &llvm::getTheGenpayTarget() {
  static Target TheGenpayTarget;
  return TheGenpayTarget;
}

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGenpayTargetInfo() {
  RegisterTarget<Triple::genpay> X(getTheGenpayTarget(), "genpay",
                                   "Genpay Smart-Contract ISA Target",
                                   "Genpay");
}
