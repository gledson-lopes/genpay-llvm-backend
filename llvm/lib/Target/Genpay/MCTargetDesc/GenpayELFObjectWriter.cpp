//===-- GenpayELFObjectWriter.cpp - Genpay ELF Writer ------*- C++ -*------===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayFixupKinds.h"
#include "GenpayMCTargetDesc.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCELFObjectWriter.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCValue.h"

using namespace llvm;

#define DEBUG_TYPE "genpay-elf-writer"

namespace {

class GenpayELFObjectWriter : public MCELFObjectTargetWriter {
public:
  // e_machine = EM_GENPAY (0x4750). The constant is registered in
  // llvm/BinaryFormat/ELF.h by integrate.sh (patch [3c]). ELFObjectWriter::
  // writeHeader writes TargetObjectWriter->getEMachine() verbatim into the
  // header, so every emitted object self-identifies as Genpay.
  GenpayELFObjectWriter(uint8_t OSABI)
      : MCELFObjectTargetWriter(/*Is64Bit=*/true, OSABI,
                                /*EMachine=*/ELF::EM_GENPAY,
                                /*HasRelocationAddend=*/true) {}

  ~GenpayELFObjectWriter() override = default;

  unsigned getRelocType(const MCFixup &Fixup, const MCValue &Target,
                        bool IsPCRel) const override;
};

} // namespace

unsigned GenpayELFObjectWriter::getRelocType(const MCFixup &Fixup,
                                             const MCValue &Target,
                                             bool IsPCRel) const {
  MCFixupKind Kind = Fixup.getKind();

  // .reloc-specified kinds pass through unchanged.
  if (Kind >= FirstLiteralRelocationKind)
    return Kind;

  switch (Kind) {
  case FK_Data_1:
    return Genpay::R_GENPAY_8;
  case FK_Data_2:
    return Genpay::R_GENPAY_16;
  case FK_Data_4:
    return Genpay::R_GENPAY_32;
  case FK_Data_8:
    return Genpay::R_GENPAY_64;
  default:
    if (Kind == Genpay::fixup_genpay_brtarget16)
      return Genpay::R_GENPAY_BRTARGET16;
    if (Kind == Genpay::fixup_genpay_imm16)
      return Genpay::R_GENPAY_IMM16;
    if (Kind == Genpay::fixup_genpay_imm32_shift16)
      return Genpay::R_GENPAY_32;
    if (Kind == Genpay::fixup_genpay_imm16_shift48)
      return Genpay::R_GENPAY_16;
    return Genpay::R_GENPAY_NONE;
  }
}

std::unique_ptr<MCObjectTargetWriter>
llvm::createGenpayELFObjectWriter(uint8_t OSABI) {
  return std::make_unique<GenpayELFObjectWriter>(OSABI);
}
