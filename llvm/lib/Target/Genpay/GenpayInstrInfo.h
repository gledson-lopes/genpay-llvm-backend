//===-- GenpayInstrInfo.h - Genpay Instruction Information ---*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYINSTRINFO_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYINSTRINFO_H

#include "GenpayRegisterInfo.h"
#include "llvm/CodeGen/TargetInstrInfo.h"

// The Genpay:: register/opcode enums live in GenpayMCTargetDesc.h
// (single canonical source for GET_REGINFO_ENUM / GET_INSTRINFO_ENUM).
#include "MCTargetDesc/GenpayMCTargetDesc.h"

#define GET_INSTRINFO_HEADER
#include "GenpayGenInstrInfo.inc"

namespace llvm {

class GenpayInstrInfo : public GenpayGenInstrInfo {
public:
  GenpayInstrInfo();

  void copyPhysReg(MachineBasicBlock &MBB, MachineBasicBlock::iterator MI,
                   const DebugLoc &DL, Register DestReg, Register SrcReg,
                   bool KillSrc, bool RenamableDest = false,
                   bool RenamableSrc = false) const override;
};

} // namespace llvm

#endif
