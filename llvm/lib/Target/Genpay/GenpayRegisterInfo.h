//===-- GenpayRegisterInfo.h - Genpay Register Information -*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYREGISTERINFO_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYREGISTERINFO_H

#include "llvm/CodeGen/TargetRegisterInfo.h"

#define GET_REGINFO_HEADER
#include "GenpayGenRegisterInfo.inc"

namespace llvm {

class GenpayRegisterInfo : public GenpayGenRegisterInfo {
public:
  GenpayRegisterInfo();

  const MCPhysReg *getCalleeSavedRegs(const MachineFunction *MF) const override;

  BitVector getReservedRegs(const MachineFunction &MF) const override;

  bool eliminateFrameIndex(MachineBasicBlock::iterator II, int SPAdj,
                           unsigned FIOperandNum,
                           RegScavenger *RS = nullptr) const override;

  Register getFrameRegister(const MachineFunction &MF) const override;
};

} // namespace llvm

#endif
