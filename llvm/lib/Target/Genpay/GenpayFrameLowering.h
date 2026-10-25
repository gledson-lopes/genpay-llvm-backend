//===-- GenpayFrameLowering.h - Frame info for Genpay -------*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYFRAMELOWERING_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYFRAMELOWERING_H

#include "llvm/CodeGen/TargetFrameLowering.h"

namespace llvm {

class GenpayFrameLowering : public TargetFrameLowering {
public:
  GenpayFrameLowering();

  void emitPrologue(MachineFunction &MF, MachineBasicBlock &MBB) const override {}
  void emitEpilogue(MachineFunction &MF, MachineBasicBlock &MBB) const override {}

  // LLVM 21: hasFP is final; targets implement hasFPImpl.
  bool hasFPImpl(const MachineFunction &MF) const override { return false; }

  bool hasReservedCallFrame(const MachineFunction &MF) const override {
    return true;
  }
};

} // namespace llvm

#endif
