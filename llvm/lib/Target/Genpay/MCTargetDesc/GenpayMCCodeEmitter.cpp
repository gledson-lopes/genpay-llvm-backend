//===-- GenpayMCCodeEmitter.cpp - Genpay Instruction Emitter -*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
// Binary encoding (32-bit fixed words, little-endian in the byte stream):
//
//   31        26 25      21 20      16 15      11 10         0
//  +------------+----------+----------+----------+-----------+
//  |  opcode    |   dst    |   src1   |   src2   |  (unused) |
//  +------------+----------+----------+----------+-----------+
//
//   31        26 25      21 20      16 15                   0
//  +------------+----------+----------+----------------------+
//  |  opcode    |   dst    |   src1   |       imm16          |
//  +------------+----------+----------+----------------------+
//
// Register fields hold the hardware encoding (r0-r14 = 0-14, sp = 15,
// b0-b7 = 16-23, g0 = 24). The I-format imm16 field overlaps the R-format
// src2 field; which format applies is decided by the opcode. Branch
// displacements (imm16) are counted in 4-byte instruction words relative
// to the address of the branch itself (next instruction = 1).
//
// `li` is variable-length:
//   - immediate fits in signed 16 bits -> one 4-byte word (I-format)
//   - otherwise -> 12 bytes: word1 = opcode|dst|imm[15:0],
//                  word2 = imm[47:16], word3 = imm[63:48] (upper 16 bits
//                  of word3 are zero padding). Symbolic immediates always
//                  use the 12-byte form so relocations never change size.
//
// Opcodes (6-bit field, values must be <= 0x3f):
//   0x01 add  0x02 addi 0x03 sub 0x04 and 0x05 or 0x06 xor 0x07 mov
//   0x08 li
//   0x09 lr.s  0x0a sc.s  0x0b setgas  0x0c readgas  0x0d sha256r
//   0x10 j    0x11 jeq  0x12 jne 0x13 jz  0x14 jnz 0x15 ret
//   0x20 load.s  0x21 store.s  0x22 matk  0x23 mdcy  0x24 mrel
//   0x30 txdeposit  0x31 txrelease
//
//===----------------------------------------------------------------------===//

#include "GenpayFixupKinds.h"
#include "GenpayMCTargetDesc.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCCodeEmitter.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixup.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"

using namespace llvm;

#define DEBUG_TYPE "genpay-mc-emitter"

namespace {

class GenpayMCCodeEmitter : public MCCodeEmitter {
  MCContext &Ctx;

public:
  GenpayMCCodeEmitter(const MCInstrInfo &MCII, MCContext &Ctx)
      : Ctx(Ctx) {
    (void)MCII;
  }

  void encodeInstruction(const MCInst &MI, SmallVectorImpl<char> &CB,
                         SmallVectorImpl<MCFixup> &Fixups,
                         const MCSubtargetInfo &STI) const override;

private:
  // Variable-length constant load (see file comment).
  void encodeLI(const MCInst &MI, SmallVectorImpl<char> &CB,
                SmallVectorImpl<MCFixup> &Fixups) const;

  // Appends one 32-bit little-endian word to the code buffer.
  static void emitWord(uint32_t W, SmallVectorImpl<char> &CB) {
    CB.push_back(static_cast<char>(W & 0xff));
    CB.push_back(static_cast<char>((W >> 8) & 0xff));
    CB.push_back(static_cast<char>((W >> 16) & 0xff));
    CB.push_back(static_cast<char>((W >> 24) & 0xff));
  }

  // 5-bit hardware register field.
  unsigned regField(const MCInst &MI, unsigned OpNo) const;

  // 16-bit immediate field with range check; symbolic expressions become
  // fixup_genpay_imm16 relocations.
  unsigned immField(const MCInst &MI, unsigned OpNo,
                    SmallVectorImpl<MCFixup> &Fixups) const;

  // Branch displacement field; the label expression becomes a
  // fixup_genpay_brtarget16 relocation.
  unsigned brField(const MCInst &MI, unsigned OpNo,
                   SmallVectorImpl<MCFixup> &Fixups) const;
};

} // namespace

unsigned GenpayMCCodeEmitter::regField(const MCInst &MI, unsigned OpNo) const {
  const MCOperand &Op = MI.getOperand(OpNo);
  assert(Op.isReg() && "expected register operand");
  unsigned HW = Ctx.getRegisterInfo()->getEncodingValue(Op.getReg());
  assert(HW < 32 && "register encoding does not fit the 5-bit field");
  return HW & 0x1f;
}

unsigned GenpayMCCodeEmitter::immField(const MCInst &MI, unsigned OpNo,
                                       SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &Op = MI.getOperand(OpNo);
  if (Op.isExpr()) {
    Fixups.push_back(MCFixup::create(
        /*Offset=*/0, Op.getExpr(), MCFixupKind(Genpay::fixup_genpay_imm16)));
    return 0;
  }
  assert(Op.isImm() && "expected immediate operand");
  int64_t Imm = Op.getImm();
  if (!isInt<16>(Imm)) {
    Ctx.reportError(MI.getLoc(),
                    "immediate operand out of range [-32768, 32767]");
    return 0;
  }
  return static_cast<uint32_t>(Imm) & 0xffff;
}

unsigned GenpayMCCodeEmitter::brField(const MCInst &MI, unsigned OpNo,
                                      SmallVectorImpl<MCFixup> &Fixups) const {
  const MCOperand &Op = MI.getOperand(OpNo);
  if (Op.isExpr()) {
    Fixups.push_back(MCFixup::create(
        /*Offset=*/0, Op.getExpr(),
        MCFixupKind(Genpay::fixup_genpay_brtarget16), /*PCRel=*/true));
    return 0;
  }
  assert(Op.isImm() && "expected branch target operand");
  Ctx.reportError(MI.getLoc(), "branch target must be a label or expression");
  return 0;
}

void GenpayMCCodeEmitter::encodeLI(const MCInst &MI,
                                   SmallVectorImpl<char> &CB,
                                   SmallVectorImpl<MCFixup> &Fixups) const {
  uint32_t Word = (0x08u << 26) | (regField(MI, 0) << 21);
  const MCOperand &Op = MI.getOperand(1);

  if (Op.isExpr()) {
    // Symbolic immediates always use the 12-byte long form so that the
    // relocation set never changes the instruction size.
    Fixups.push_back(MCFixup::create(
        /*Offset=*/0, Op.getExpr(), MCFixupKind(Genpay::fixup_genpay_imm16)));
    Fixups.push_back(MCFixup::create(
        /*Offset=*/4, Op.getExpr(),
        MCFixupKind(Genpay::fixup_genpay_imm32_shift16)));
    Fixups.push_back(MCFixup::create(
        /*Offset=*/8, Op.getExpr(),
        MCFixupKind(Genpay::fixup_genpay_imm16_shift48)));
    emitWord(Word, CB);
    emitWord(0, CB);
    emitWord(0, CB);
    return;
  }

  assert(Op.isImm() && "expected immediate operand");
  uint64_t U = static_cast<uint64_t>(Op.getImm());
  emitWord(Word | (static_cast<uint32_t>(U) & 0xffff), CB);
  if (!isInt<16>(Op.getImm())) {
    // 12-byte long form: imm[47:16], then imm[63:48] (plus 16 zero bits).
    emitWord(static_cast<uint32_t>(U >> 16), CB);
    emitWord(static_cast<uint32_t>((U >> 48) & 0xffff), CB);
  }
}

void GenpayMCCodeEmitter::encodeInstruction(
    const MCInst &MI, SmallVectorImpl<char> &CB,
    SmallVectorImpl<MCFixup> &Fixups, const MCSubtargetInfo &STI) const {
  if (MI.getOpcode() == Genpay::LI)
    return encodeLI(MI, CB, Fixups);

  uint32_t Word = 0;

  switch (MI.getOpcode()) {
  // ---- ALU core ---------------------------------------------------------
  // NOTE: opcodes are hardcoded per case; the generated Genpay:: enum order
  // is NOT guaranteed to follow the .td definition order.
  case Genpay::ADDrr:
    Word = (0x01u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::SUBrr:
    Word = (0x03u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::ANDrr:
    Word = (0x04u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::ORrr:
    Word = (0x05u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::XORrr:
    Word = (0x06u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::ADDri:
    Word = (0x02u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           immField(MI, 2, Fixups);
    break;

  case Genpay::MOVrr:
    Word = (0x07u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  // ---- Control flow ------------------------------------------------------
  case Genpay::J:
    Word = (0x10u << 26) | brField(MI, 0, Fixups);
    break;

  case Genpay::JEQ:
    Word = (0x11u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           brField(MI, 2, Fixups);
    break;

  case Genpay::JNE:
    Word = (0x12u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           brField(MI, 2, Fixups);
    break;

  case Genpay::JZ:
    Word = (0x13u << 26) | (regField(MI, 0) << 21) | brField(MI, 1, Fixups);
    break;

  case Genpay::JNZ:
    Word = (0x14u << 26) | (regField(MI, 0) << 21) | brField(MI, 1, Fixups);
    break;

  case Genpay::RET:
    Word = (0x15u << 26) | (regField(MI, 0) << 16);
    break;

  // ---- ADRS tagged memory / atomics ---------------------------------------
  case Genpay::LOADS:
  case Genpay::LRS:
    Word = ((MI.getOpcode() == Genpay::LOADS ? 0x20u : 0x09u) << 26) |
           (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  case Genpay::LOADSO:
    Word = (0x20u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           immField(MI, 2, Fixups);
    break;

  case Genpay::STORES:
    Word = (0x21u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  case Genpay::STORESO:
    Word = (0x21u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           immField(MI, 2, Fixups);
    break;

  // ---- ADRS lifecycle -------------------------------------------------------
  case Genpay::MATK:
    Word = (0x22u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  case Genpay::MDCY:
    Word = (0x23u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  case Genpay::MREL:
    Word = (0x24u << 26) | (regField(MI, 0) << 16);
    break;

  // ---- Ledger engine ----------------------------------------------------------
  case Genpay::TXDEPOSIT:
    Word = (0x30u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  case Genpay::TXRELEASE:
    Word = (0x31u << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16);
    break;

  // ---- Hardware atomics ---------------------------------------------------------
  case Genpay::SCS:
    Word = (0x0au << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  // ---- Gas engine ------------------------------------------------------------------
  case Genpay::SETGAS:
    Word = (0x0bu << 26) | (regField(MI, 0) << 16);
    break;

  case Genpay::READGAS:
    Word = (0x0cu << 26) | (regField(MI, 0) << 21);
    break;

  // ---- Crypto pipeline ----------------------------------------------------------------
  case Genpay::SHA256R:
    Word = (0x0du << 26) | (regField(MI, 0) << 21) | (regField(MI, 1) << 16) |
           (regField(MI, 2) << 11);
    break;

  default:
    llvm_unreachable("GenpayMCCodeEmitter: unexpected opcode");
  }

  // Fixed little-endian 4-byte words.
  emitWord(Word, CB);
}

MCCodeEmitter *llvm::createGenpayMCCodeEmitter(const MCInstrInfo &MCII,
                                               MCContext &Ctx) {
  return new GenpayMCCodeEmitter(MCII, Ctx);
}
