//===-- Genpay.h - Top-level interface for Genpay backend ----*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAY_H
#define LLVM_LIB_TARGET_GENPAY_GENPAY_H

#include "llvm/Target/TargetMachine.h"

namespace llvm {

class FunctionPass;
class PassRegistry;
class Target;
class GenpayTargetMachine;

Target &getTheGenpayTarget();
FunctionPass *createGenpayISelDag(GenpayTargetMachine &TM);

// Pass initialization hooks (required before INITIALIZE_PASS users run
// them through the PassRegistry; declared here so GenpayTargetMachine.cpp
// and external hosts can force-initialize).
void initializeGenpayDAGToDAGISelLegacyPass(PassRegistry &);
void initializeGenpayAsmPrinterPass(PassRegistry &);

} // namespace llvm

#endif
