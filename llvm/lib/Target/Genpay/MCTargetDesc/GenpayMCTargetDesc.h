//===-- GenpayMCTargetDesc.h - Genpay Target Descriptions ---*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCTARGETDESC_H
#define LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCTARGETDESC_H

#include <memory>

#include "llvm/MC/MCRegisterInfo.h"

#define GET_REGINFO_ENUM
#include "GenpayGenRegisterInfo.inc"

#define GET_INSTRINFO_ENUM
#include "GenpayGenInstrInfo.inc"

namespace llvm {
class MCAsmBackend;
class MCCodeEmitter;
class MCContext;
class MCInstrInfo;
class MCObjectTargetWriter;
class MCSubtargetInfo;
class MCTargetOptions;
class Target;

Target &getTheGenpayTarget();

MCCodeEmitter *createGenpayMCCodeEmitter(const MCInstrInfo &MCII,
                                         MCContext &Ctx);

MCAsmBackend *createGenpayMCAsmBackend(const Target &T,
                                       const MCSubtargetInfo &STI,
                                       const MCRegisterInfo &MRI,
                                       const MCTargetOptions &Options);

std::unique_ptr<MCObjectTargetWriter>
createGenpayELFObjectWriter(uint8_t OSABI);
} // namespace llvm

#endif
