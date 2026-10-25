//===-- GenpaySubtarget.h - Define Subtarget for Genpay -----*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYSUBTARGET_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYSUBTARGET_H

#include "GenpayFrameLowering.h"
#include "GenpayISelLowering.h"
#include "GenpayInstrInfo.h"
#include "GenpayRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGTargetInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class GenpaySubtarget : public TargetSubtargetInfo {
  GenpayRegisterInfo RegInfo;
  GenpayInstrInfo InstrInfo;
  GenpayTargetLowering TLInfo;
  GenpayFrameLowering FrameLowering;
  SelectionDAGTargetInfo SDAGInfo;

public:
  GenpaySubtarget(const Triple &TT, StringRef CPU, StringRef FS,
                  const TargetMachine &TM, const TargetOptions &Options,
                  CodeModel::Model CM, CodeGenOptLevel OL);

  const GenpayRegisterInfo *getRegisterInfo() const override {
    return &RegInfo;
  }
  const GenpayInstrInfo *getInstrInfo() const override { return &InstrInfo; }
  const GenpayTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const TargetFrameLowering *getFrameLowering() const override {
    return &FrameLowering;
  }
  // NOTE: must return a valid object - SelectionDAG::verifyNode (assertions
  // builds) dereferences it for every target-specific node.
  const SelectionDAGTargetInfo *getSelectionDAGInfo() const override {
    return &SDAGInfo;
  }
};

} // namespace llvm

#endif
