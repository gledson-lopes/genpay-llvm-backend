//===-- GenpayAsmBackend.cpp - Genpay Assembler Backend -----*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayFixupKinds.h"
#include "GenpayMCTargetDesc.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/BinaryFormat/ELF.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Endian.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

#define DEBUG_TYPE "genpay-asm-backend"

namespace {

class GenpayAsmBackend : public MCAsmBackend {
public:
  GenpayAsmBackend(llvm::endianness Endian) : MCAsmBackend(Endian) {}
  ~GenpayAsmBackend() override = default;

  std::unique_ptr<MCObjectTargetWriter>
  createObjectTargetWriter() const override;

  MCFixupKindInfo getFixupKindInfo(MCFixupKind Kind) const override;

  void applyFixup(const MCFragment &F, const MCFixup &Fixup,
                  const MCValue &Target, MutableArrayRef<char> Data,
                  uint64_t Value, bool IsResolved) override;

  bool writeNopData(raw_ostream &OS, uint64_t Count,
                    const MCSubtargetInfo *STI) const override;
};

} // namespace

MCFixupKindInfo GenpayAsmBackend::getFixupKindInfo(MCFixupKind Kind) const {
  // Offsets and lengths are in bits from the fixup site. The imm16 field
  // occupies bits [15:0] of the little-endian instruction word, i.e. bytes
  // [0, 2) of the instruction.
  static const MCFixupKindInfo Infos[Genpay::NumTargetFixupKinds] = {
      {"fixup_genpay_brtarget16", 0, 16, 0},
      {"fixup_genpay_imm16", 0, 16, 0},
      {"fixup_genpay_imm32_shift16", 0, 32, 0},
      {"fixup_genpay_imm16_shift48", 0, 16, 0},
  };

  if (Kind < FirstTargetFixupKind)
    return MCAsmBackend::getFixupKindInfo(Kind);
  if (static_cast<unsigned>(Kind - FirstTargetFixupKind) >=
      Genpay::NumTargetFixupKinds)
    return MCAsmBackend::getFixupKindInfo(Kind);
  return Infos[Kind - FirstTargetFixupKind];
}

void GenpayAsmBackend::applyFixup(const MCFragment &F, const MCFixup &Fixup,
                                  const MCValue &Target,
                                  MutableArrayRef<char> Data, uint64_t Value,
                                  bool IsResolved) {
  // LLVM 21: the backend owns relocation recording for unresolved fixups.
  if (!IsResolved)
    Asm->getWriter().recordRelocation(F, Fixup, Target, Value);

  MCFixupKind Kind = Fixup.getKind();
  // Byte offset of the fixup site within the fragment data. Target fixups
  // are created at offset 0 of a 4-byte instruction; the imm16 field occupies
  // bits [15:0], i.e. bytes [Base, Base+2) of the little-endian word.
  uint32_t Base = Fixup.getOffset();

  if (Kind >= FirstTargetFixupKind) {
    int64_t Field;
    if (Kind == Genpay::fixup_genpay_brtarget16) {
      // Value is a byte displacement measured from the branch instruction
      // start (MC subtracts the fixup site address). Convert to words.
      int64_t Bytes = static_cast<int64_t>(Value);
      if (Bytes % 4 != 0) {
        getContext().reportError(Fixup.getLoc(),
                                 "branch target is not 4-byte aligned");
        return;
      }
      Field = Bytes / 4;
      if (!isInt<16>(Field)) {
        getContext().reportError(
            Fixup.getLoc(),
            "branch target out of range (+-131068 bytes from the branch)");
        return;
      }
      uint32_t Raw = static_cast<uint32_t>(Field) & 0xffff;
      Data[Base + 0] = static_cast<char>(Raw & 0xff);
      Data[Base + 1] = static_cast<char>((Raw >> 8) & 0xff);
      return;
    }

    // LI long form: slice the 64-bit value into the immediate fields.
    if (Kind == Genpay::fixup_genpay_imm32_shift16) {
      uint32_t Raw = static_cast<uint32_t>(Value >> 16);
      Data[Base + 0] = static_cast<char>(Raw & 0xff);
      Data[Base + 1] = static_cast<char>((Raw >> 8) & 0xff);
      Data[Base + 2] = static_cast<char>((Raw >> 16) & 0xff);
      Data[Base + 3] = static_cast<char>((Raw >> 24) & 0xff);
      return;
    }
    if (Kind == Genpay::fixup_genpay_imm16_shift48) {
      uint32_t Raw = static_cast<uint32_t>((Value >> 48) & 0xffff);
      Data[Base + 0] = static_cast<char>(Raw & 0xff);
      Data[Base + 1] = static_cast<char>((Raw >> 8) & 0xff);
      return;
    }

    // Plain 16-bit immediate.
    Field = static_cast<int64_t>(Value);
    if (!isInt<16>(Field)) {
      getContext().reportError(
          Fixup.getLoc(), "relocation value out of range [-32768, 32767]");
      return;
    }
    uint32_t Raw = static_cast<uint32_t>(Field) & 0xffff;
    Data[Base + 0] = static_cast<char>(Raw & 0xff);
    Data[Base + 1] = static_cast<char>((Raw >> 8) & 0xff);
    return;
  }

  // Generic data fixups, little-endian.
  switch (Kind) {
  case FK_Data_1:
    Data[Base] = static_cast<char>(Value & 0xff);
    break;
  case FK_Data_2:
    support::endian::write16le(Data.data() + Base, Value);
    break;
  case FK_Data_4:
    support::endian::write32le(Data.data() + Base, Value);
    break;
  case FK_Data_8:
    support::endian::write64le(Data.data() + Base, Value);
    break;
  default:
    llvm_unreachable("GenpayAsmBackend: unsupported fixup kind");
  }
}

bool GenpayAsmBackend::writeNopData(raw_ostream &OS, uint64_t Count,
                                    const MCSubtargetInfo *STI) const {
  // Canonical NOP: addi r0, r0, 0  ==  0x02000000.
  const uint32_t NopWord = 0x02000000u;

  while (Count >= 4) {
    support::endian::write(OS, NopWord, llvm::endianness::little);
    Count -= 4;
  }
  // Pad any sub-word remainder with zero bytes (undefined opcodes); this can
  // only come from odd .align directives.
  while (Count > 0) {
    OS << '\0';
    --Count;
  }
  return true;
}

std::unique_ptr<MCObjectTargetWriter>
GenpayAsmBackend::createObjectTargetWriter() const {
  return createGenpayELFObjectWriter(ELF::ELFOSABI_NONE);
}

MCAsmBackend *llvm::createGenpayMCAsmBackend(const Target &T,
                                             const MCSubtargetInfo &STI,
                                             const MCRegisterInfo &MRI,
                                             const MCTargetOptions &Options) {
  return new GenpayAsmBackend(llvm::endianness::little);
}
