//===-- GenpayRegisterInfo.cpp - Genpay Register Information -*- C++ -*---===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayRegisterInfo.h"

// The generated GenpayGenRegisterInfo.inc (GET_REGINFO_TARGET_DESC section)
// contains getFrameLowering(), which static_casts to GenpayFrameLowering.
// That requires a complete type here, before the .inc is pulled in.
#include "GenpayFrameLowering.h"
#include "GenpaySubtarget.h"
#include "MCTargetDesc/GenpayMCTargetDesc.h"

#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

#define GET_REGINFO_TARGET_DESC
#include "GenpayGenRegisterInfo.inc"

GenpayRegisterInfo::GenpayRegisterInfo()
    : GenpayGenRegisterInfo(Genpay::SP) {}

const MCPhysReg *GenpayRegisterInfo::getCalleeSavedRegs(
    const MachineFunction *) const {
  static const MCPhysReg CalleeSavedRegs[] = {0};
  return CalleeSavedRegs;
}

BitVector GenpayRegisterInfo::getReservedRegs(const MachineFunction &MF) const {
  BitVector Reserved(getNumRegs());
  // Stack pointer.
  Reserved.set(Genpay::SP);
  // Isolated ledger registers: compiler-controlled, never allocated.
  Reserved.set(Genpay::B0);
  Reserved.set(Genpay::B1);
  Reserved.set(Genpay::B2);
  Reserved.set(Genpay::B3);
  Reserved.set(Genpay::B4);
  Reserved.set(Genpay::B5);
  Reserved.set(Genpay::B6);
  Reserved.set(Genpay::B7);
  // Hardware gas counter.
  Reserved.set(Genpay::G0);
  return Reserved;
}

bool GenpayRegisterInfo::eliminateFrameIndex(
    MachineBasicBlock::iterator II, int SPAdj, unsigned FIOperandNum,
    RegScavenger *) const {
  llvm_unreachable("Genpay: frame indices are not supported yet");
}

Register GenpayRegisterInfo::getFrameRegister(const MachineFunction &) const {
  return Genpay::SP;
}
