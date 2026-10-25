//===-- GenpayTargetMachine.h - Define TargetMachine -------- -*- C++ -*---===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYTARGETMACHINE_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYTARGETMACHINE_H

#include "GenpaySubtarget.h"
#include "llvm/CodeGen/CodeGenTargetMachineImpl.h"
#include "llvm/Target/TargetMachine.h"

#include <optional>

namespace llvm {

class GenpayTargetMachine : public CodeGenTargetMachineImpl {
  GenpaySubtarget Subtarget;
  std::unique_ptr<TargetLoweringObjectFile> TLOF;

public:
  GenpayTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                      StringRef FS, const TargetOptions &Options,
                      std::optional<Reloc::Model> RM,
                      std::optional<CodeModel::Model> CM, CodeGenOptLevel OL,
                      bool JIT);

  const GenpaySubtarget *getSubtargetImpl(const Function &) const override {
    return &Subtarget;
  }

  TargetPassConfig *createPassConfig(PassManagerBase &PM) override;

  TargetLoweringObjectFile *getObjFileLowering() const override {
    return TLOF.get();
  }
};

} // namespace llvm

#endif
