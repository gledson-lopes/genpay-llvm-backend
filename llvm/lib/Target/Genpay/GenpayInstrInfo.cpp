//===-- GenpayInstrInfo.cpp - Genpay Instruction Information -*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayInstrInfo.h"
#include "GenpaySubtarget.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/MC/MCRegisterInfo.h"

using namespace llvm;

#define GET_INSTRINFO_CTOR_DTOR
#include "GenpayGenInstrInfo.inc"

GenpayInstrInfo::GenpayInstrInfo()
    : GenpayGenInstrInfo(~0u, ~0u, ~0u, Genpay::RET) {}

void GenpayInstrInfo::copyPhysReg(MachineBasicBlock &MBB,
                                  MachineBasicBlock::iterator MI,
                                  const DebugLoc &DL, Register DestReg,
                                  Register SrcReg, bool KillSrc,
                                  bool RenamableDest,
                                  bool RenamableSrc) const {
  assert(Genpay::GPRRegClass.contains(DestReg) &&
         Genpay::GPRRegClass.contains(SrcReg) &&
         "Genpay: only GPR-to-GPR copies are supported");
  BuildMI(MBB, MI, DL, get(Genpay::MOVrr), DestReg)
      .addReg(SrcReg, getKillRegState(KillSrc));
}
