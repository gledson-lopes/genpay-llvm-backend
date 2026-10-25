//===-- GenpayAsmParser.cpp - Parse Genpay assembly ---------*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
// Hand-written assembly parser (no -gen-asm-matcher): the ISA has a small,
// fixed set of operand shapes, so mnemonics are dispatched through a static
// table and each operand is parsed by shape (register / immediate / branch
// target / [base, offset] memory).
//
// Accepts exactly the forms printed by GenpayInstPrinter, so the round trip
//   llc -mtriple=genpay x.ll | llvm-mc -triple=genpay
// always parses. Additionally accepts:
//   - `nop` (alias for `addi r0, r0, 0`, the canonical encoding NOP)
//   - `r15` as an alias for `sp`
//   - case-insensitive mnemonics
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/GenpayMCTargetDesc.h"
#include "TargetInfo/GenpayTargetInfo.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/ADT/Twine.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCParser/AsmLexer.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCParsedAsmOperand.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegister.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/TargetRegistry.h"

using namespace llvm;

#define DEBUG_TYPE "genpay-asm-parser"

namespace {

// ---------------------------------------------------------------------------
// Parsed operand
// ---------------------------------------------------------------------------

class GenpayOperand : public MCParsedAsmOperand {
public:
  enum KindTy {
    k_Token,     // mnemonic
    k_Register,  // r0-r14, sp, b0-b7, g0
    k_Immediate, // absolute integer
    k_Expr,      // non-absolute expression (becomes imm16 relocation)
    k_BrTarget,  // branch target (label or expression)
    k_Memory,    // [base] or [base, offset]
  };

private:
  KindTy Kind;
  SMLoc StartLoc, EndLoc;

  // Owned storage: tokens must outlive parseInstruction (the Operands vector
  // is consumed later by matchAndEmitInstruction).
  std::string TokenStorage;

  MCRegister Reg;
  int64_t Imm = 0;
  const MCExpr *Expr = nullptr;

  // Memory operand fields.
  MCRegister MemBase;
  bool MemHasOffset = false;

public:
  static std::unique_ptr<GenpayOperand> CreateToken(StringRef Tok, SMLoc S) {
    auto Op = std::make_unique<GenpayOperand>();
    Op->Kind = k_Token;
    Op->StartLoc = S;
    Op->EndLoc = S;
    Op->TokenStorage = Tok.str();
    return Op;
  }

  static std::unique_ptr<GenpayOperand> CreateReg(MCRegister Reg, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<GenpayOperand>();
    Op->Kind = k_Register;
    Op->StartLoc = S;
    Op->EndLoc = E;
    Op->Reg = Reg;
    return Op;
  }

  static std::unique_ptr<GenpayOperand> CreateImm(int64_t Imm, SMLoc S,
                                                  SMLoc E) {
    auto Op = std::make_unique<GenpayOperand>();
    Op->Kind = k_Immediate;
    Op->StartLoc = S;
    Op->EndLoc = E;
    Op->Imm = Imm;
    return Op;
  }

  static std::unique_ptr<GenpayOperand> CreateExpr(const MCExpr *E, SMLoc S,
                                                   SMLoc EL, KindTy K) {
    auto Op = std::make_unique<GenpayOperand>();
    Op->Kind = K;
    Op->StartLoc = S;
    Op->EndLoc = EL;
    Op->Expr = E;
    return Op;
  }

  static std::unique_ptr<GenpayOperand>
  CreateMem(MCRegister Base, bool HasOffset, const MCExpr *Offset, SMLoc S,
            SMLoc EL) {
    auto Op = std::make_unique<GenpayOperand>();
    Op->Kind = k_Memory;
    Op->StartLoc = S;
    Op->EndLoc = EL;
    Op->MemBase = Base;
    Op->MemHasOffset = HasOffset;
    Op->Expr = Offset;
    return Op;
  }

  // --- MCParsedAsmOperand interface ---

  bool isReg() const override { return Kind == k_Register; }
  bool isImm() const override {
    return Kind == k_Immediate || Kind == k_Expr;
  }
  bool isToken() const override { return Kind == k_Token; }
  bool isMem() const override { return Kind == k_Memory; }

  MCRegister getReg() const override {
    if (Kind == k_Register)
      return Reg;
    if (Kind == k_Memory)
      return MemBase;
    return MCRegister();
  }

  SMLoc getStartLoc() const override { return StartLoc; }
  SMLoc getEndLoc() const override { return EndLoc; }

  void print(raw_ostream &OS, const MCAsmInfo &MAI) const override {
    switch (Kind) {
    case k_Token:
      OS << "token:" << TokenStorage;
      break;
    case k_Register:
      OS << "reg";
      break;
    case k_Immediate:
      OS << "imm:" << Imm;
      break;
    case k_Expr:
      OS << "expr";
      break;
    case k_BrTarget:
      OS << "brtarget";
      break;
    case k_Memory:
      OS << "mem";
      break;
    }
  }

  // --- Accessors ---

  StringRef getToken() const {
    assert(Kind == k_Token && "not a token operand");
    return TokenStorage;
  }

  const MCExpr *getExpr() const {
    assert((Kind == k_Expr || Kind == k_BrTarget || Kind == k_Memory) &&
           "operand has no expression");
    return Expr;
  }

  bool isBrTarget() const { return Kind == k_BrTarget; }

  bool hasMemOffset() const {
    assert(Kind == k_Memory && "not a memory operand");
    return MemHasOffset;
  }

  // --- MCInst construction ---

  void addRegOperands(MCInst &Inst) const {
    Inst.addOperand(MCOperand::createReg(getReg()));
  }

  void addImmOperands(MCInst &Inst) const {
    if (Kind == k_Immediate)
      Inst.addOperand(MCOperand::createImm(Imm));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addBrTargetOperands(MCInst &Inst) const {
    assert(Kind == k_BrTarget);
    Inst.addOperand(MCOperand::createExpr(Expr));
  }

  void addMemOperands(MCInst &Inst) const {
    assert(Kind == k_Memory);
    // Memory operands contribute (base, offset) MCInst operands in order.
    Inst.addOperand(MCOperand::createReg(MemBase));
    if (!MemHasOffset || !Expr) {
      Inst.addOperand(MCOperand::createImm(0));
      return;
    }
    int64_t Res;
    if (Expr->evaluateAsAbsolute(Res))
      Inst.addOperand(MCOperand::createImm(Res));
    else
      Inst.addOperand(MCOperand::createExpr(Expr));
  }
};

// ---------------------------------------------------------------------------
// Mnemonic table
// ---------------------------------------------------------------------------

enum class OpShape { Reg, Imm, BrTarget, Mem };

struct MnemonicRow {
  const char *Name;
  unsigned Opcode;
  OpShape Shapes[3];
  unsigned NumShapes; // number of parsed operands (0 for the nop alias)
};

const MnemonicRow Mnemonics[] = {
    // ALU core
    {"add", Genpay::ADDrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"addi", Genpay::ADDri, {OpShape::Reg, OpShape::Reg, OpShape::Imm}, 3},
    {"sub", Genpay::SUBrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"and", Genpay::ANDrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"or", Genpay::ORrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"xor", Genpay::XORrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"mov", Genpay::MOVrr, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 2},
    {"li", Genpay::LI, {OpShape::Reg, OpShape::Imm, OpShape::Reg}, 2},

    // Control flow
    {"j", Genpay::J, {OpShape::BrTarget, OpShape::Reg, OpShape::Reg}, 1},
    {"jeq", Genpay::JEQ, {OpShape::Reg, OpShape::Reg, OpShape::BrTarget}, 3},
    {"jne", Genpay::JNE, {OpShape::Reg, OpShape::Reg, OpShape::BrTarget}, 3},
    {"jz", Genpay::JZ, {OpShape::Reg, OpShape::BrTarget, OpShape::Reg}, 2},
    {"jnz", Genpay::JNZ, {OpShape::Reg, OpShape::BrTarget, OpShape::Reg}, 2},
    {"ret", Genpay::RET, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 1},

    // ADRS tagged memory (base/offset forms resolved at match time)
    {"load.s", Genpay::LOADS, {OpShape::Reg, OpShape::Mem, OpShape::Reg}, 2},
    {"store.s", Genpay::STORES, {OpShape::Reg, OpShape::Mem, OpShape::Reg}, 2},

    // ADRS lifecycle
    {"matk", Genpay::MATK, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 2},
    {"mdcy", Genpay::MDCY, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 2},
    {"mrel", Genpay::MREL, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 1},

    // Ledger engine (txdeposit: dst, b0, source, amount - the b0 literal is
    // validated and consumed by parseInstruction, never pushed as operand)
    {"txdeposit", Genpay::TXDEPOSIT,
     {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},
    {"txrelease", Genpay::TXRELEASE,
     {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 2},

    // Hardware atomics (base-addressing only; offsets rejected at match time)
    {"lr.s", Genpay::LRS, {OpShape::Reg, OpShape::Mem, OpShape::Reg}, 2},
    {"sc.s", Genpay::SCS, {OpShape::Reg, OpShape::Reg, OpShape::Mem}, 3},

    // Gas engine
    {"setgas", Genpay::SETGAS, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 1},
    {"readgas", Genpay::READGAS,
     {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 1},

    // Crypto pipeline
    {"sha256r", Genpay::SHA256R, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 3},

    // Alias (no operands): addi r0, r0, 0
    {"nop", Genpay::ADDri, {OpShape::Reg, OpShape::Reg, OpShape::Reg}, 0},
};

const MnemonicRow *findMnemonic(StringRef Name) {
  for (const MnemonicRow &Row : Mnemonics)
    if (Name == Row.Name)
      return &Row;
  return nullptr;
}

// Ledger / gas registers are never valid GPR operands.
bool isLedgerOrGasRegister(MCRegister Reg) {
  switch (Reg.id()) {
  case Genpay::B0:
  case Genpay::B1:
  case Genpay::B2:
  case Genpay::B3:
  case Genpay::B4:
  case Genpay::B5:
  case Genpay::B6:
  case Genpay::B7:
  case Genpay::G0:
    return true;
  default:
    return false;
  }
}

// ---------------------------------------------------------------------------
// Parser
// ---------------------------------------------------------------------------

class GenpayAsmParser : public MCTargetAsmParser {
public:
  GenpayAsmParser(const MCSubtargetInfo &STI, MCAsmParser &P,
                  const MCInstrInfo &MII, const MCTargetOptions &Options)
      : MCTargetAsmParser(Options, STI, MII) {}

  bool parseInstruction(ParseInstructionInfo &Info, StringRef Name,
                        SMLoc NameLoc, OperandVector &Operands) override;

  bool matchAndEmitInstruction(SMLoc IDLoc, unsigned &Opcode,
                               OperandVector &Operands, MCStreamer &Out,
                               uint64_t &ErrorInfo,
                               bool MatchingInlineAsm) override;

  bool parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                     SMLoc &EndLoc) override;

  ParseStatus tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                               SMLoc &EndLoc) override;

  void convertToMapAndConstraints(unsigned Kind,
                                  const OperandVector &Operands) override {
    // Genpay does not support MS-style inline assembly.
  }

private:
  bool Error(SMLoc L, const Twine &Msg) { return getParser().Error(L, Msg); }

  static MCRegister matchRegisterName(StringRef Name);

  // Parses one register into a k_Register operand.
  bool parseRegOperand(OperandVector &Operands);

  // Parses an expression into k_Immediate (absolute) or k_Expr.
  bool parseImmOperand(OperandVector &Operands);

  // Parses a branch target (label or expression) into k_BrTarget.
  bool parseBrTargetOperand(OperandVector &Operands);

  // Parses "[ base ]" or "[ base , offset ]" into k_Memory.
  bool parseMemOperand(OperandVector &Operands);
};

MCRegister GenpayAsmParser::matchRegisterName(StringRef Name) {
  return StringSwitch<MCRegister>(Name)
      .Case("r0", Genpay::R0)
      .Case("r1", Genpay::R1)
      .Case("r2", Genpay::R2)
      .Case("r3", Genpay::R3)
      .Case("r4", Genpay::R4)
      .Case("r5", Genpay::R5)
      .Case("r6", Genpay::R6)
      .Case("r7", Genpay::R7)
      .Case("r8", Genpay::R8)
      .Case("r9", Genpay::R9)
      .Case("r10", Genpay::R10)
      .Case("r11", Genpay::R11)
      .Case("r12", Genpay::R12)
      .Case("r13", Genpay::R13)
      .Case("r14", Genpay::R14)
      .Case("sp", Genpay::SP)
      .Case("r15", Genpay::SP) // alias
      .Case("b0", Genpay::B0)
      .Case("b1", Genpay::B1)
      .Case("b2", Genpay::B2)
      .Case("b3", Genpay::B3)
      .Case("b4", Genpay::B4)
      .Case("b5", Genpay::B5)
      .Case("b6", Genpay::B6)
      .Case("b7", Genpay::B7)
      .Case("g0", Genpay::G0)
      .Default(MCRegister());
}

bool GenpayAsmParser::parseRegOperand(OperandVector &Operands) {
  AsmLexer &L = getLexer();
  SMLoc S = L.getLoc();
  if (L.isNot(AsmToken::Identifier))
    return Error(S, "expected register operand");
  StringRef Name = L.getTok().getIdentifier();
  MCRegister Reg = matchRegisterName(Name);
  if (!Reg) {
    Error(S, "unknown register '" + Name + "'");
    return true;
  }
  SMLoc E = L.getTok().getEndLoc();
  L.Lex();
  Operands.push_back(GenpayOperand::CreateReg(Reg, S, E));
  return false;
}

bool GenpayAsmParser::parseImmOperand(OperandVector &Operands) {
  AsmLexer &L = getLexer();
  SMLoc S = L.getLoc();
  const MCExpr *E = nullptr;
  if (getParser().parseExpression(E))
    return true;
  SMLoc EL = L.getLoc();
  int64_t Res = 0;
  if (E->evaluateAsAbsolute(Res))
    Operands.push_back(GenpayOperand::CreateImm(Res, S, EL));
  else
    Operands.push_back(
        GenpayOperand::CreateExpr(E, S, EL, GenpayOperand::k_Expr));
  return false;
}

bool GenpayAsmParser::parseBrTargetOperand(OperandVector &Operands) {
  AsmLexer &L = getLexer();
  SMLoc S = L.getLoc();
  const MCExpr *E = nullptr;
  if (getParser().parseExpression(E))
    return true;
  SMLoc EL = L.getLoc();
  Operands.push_back(
      GenpayOperand::CreateExpr(E, S, EL, GenpayOperand::k_BrTarget));
  return false;
}

bool GenpayAsmParser::parseMemOperand(OperandVector &Operands) {
  AsmLexer &L = getLexer();
  SMLoc S = L.getLoc();
  if (L.isNot(AsmToken::LBrac))
    return Error(S, "expected '['");
  L.Lex();

  if (L.isNot(AsmToken::Identifier))
    return Error(L.getLoc(), "expected base register");
  StringRef Name = L.getTok().getIdentifier();
  MCRegister Base = matchRegisterName(Name);
  if (!Base)
    return Error(L.getLoc(), "unknown register '" + Name + "'");
  L.Lex();

  bool HasOffset = false;
  const MCExpr *Offset = nullptr;
  if (L.is(AsmToken::Comma)) {
    L.Lex();
    if (getParser().parseExpression(Offset))
      return true;
    HasOffset = true;
  }

  if (L.isNot(AsmToken::RBrac))
    return Error(L.getLoc(), "expected ']'");
  SMLoc E = L.getTok().getEndLoc();
  L.Lex();

  Operands.push_back(GenpayOperand::CreateMem(Base, HasOffset, Offset, S, E));
  return false;
}

bool GenpayAsmParser::parseInstruction(ParseInstructionInfo &Info,
                                       StringRef Name, SMLoc NameLoc,
                                       OperandVector &Operands) {
  std::string Lower = Name.lower();
  const MnemonicRow *Row = findMnemonic(Lower);
  if (!Row)
    return Error(NameLoc, "unknown instruction mnemonic '" + Name + "'");

  Operands.push_back(GenpayOperand::CreateToken(Lower, NameLoc));

  AsmLexer &L = getLexer();

  // Special form: txdeposit dst, b0, source, amount  (literal ledger token).
  if (Lower == "txdeposit") {
    if (parseRegOperand(Operands)) // dst
      return true;
    if (L.isNot(AsmToken::Comma))
      return Error(L.getLoc(), "expected ','");
    L.Lex();
    SMLoc TokLoc = L.getLoc();
    if (L.isNot(AsmToken::Identifier) || L.getTok().getIdentifier() != "b0")
      return Error(TokLoc, "expected escrow ledger register 'b0'");
    L.Lex();
    if (L.isNot(AsmToken::Comma))
      return Error(L.getLoc(), "expected ','");
    L.Lex();
    if (parseRegOperand(Operands)) // source
      return true;
    if (L.isNot(AsmToken::Comma))
      return Error(L.getLoc(), "expected ','");
    L.Lex();
    if (parseRegOperand(Operands)) // amount
      return true;
    if (L.isNot(AsmToken::EndOfStatement))
      return Error(L.getLoc(), "unexpected extra text after operand");
    getParser().Lex(); // consume EndOfStatement
    return false;
  }

  for (unsigned i = 0; i < Row->NumShapes; ++i) {
    if (i > 0) {
      if (L.isNot(AsmToken::Comma))
        return Error(L.getLoc(), "expected ','");
      L.Lex();
    }
    switch (Row->Shapes[i]) {
    case OpShape::Reg:
      if (parseRegOperand(Operands))
        return true;
      break;
    case OpShape::Imm:
      if (parseImmOperand(Operands))
        return true;
      break;
    case OpShape::BrTarget:
      if (parseBrTargetOperand(Operands))
        return true;
      break;
    case OpShape::Mem:
      if (parseMemOperand(Operands))
        return true;
      break;
    }
  }

  if (L.isNot(AsmToken::EndOfStatement))
    return Error(L.getLoc(), "unexpected extra text after operand");
  getParser().Lex(); // consume EndOfStatement
  return false;
}

bool GenpayAsmParser::matchAndEmitInstruction(
    SMLoc IDLoc, unsigned &Opcode, OperandVector &Operands, MCStreamer &Out,
    uint64_t &ErrorInfo, bool MatchingInlineAsm) {
  if (Operands.empty()) {
    Error(IDLoc, "expected instruction");
    return true;
  }

  const GenpayOperand &MnemonicOp =
      *static_cast<const GenpayOperand *>(Operands[0].get());
  if (!MnemonicOp.isToken()) {
    Error(IDLoc, "expected instruction mnemonic");
    return true;
  }

  const MnemonicRow *Row = findMnemonic(MnemonicOp.getToken());
  if (!Row) {
    Error(IDLoc, "unknown instruction mnemonic");
    return true;
  }

  // Operand count check (Operands[0] is the mnemonic token; the txdeposit
  // b0 literal is not represented as an operand).
  unsigned NumParsed = Operands.size() - 1;
  unsigned NumExpected = Row->NumShapes;
  if (NumParsed != NumExpected) {
    Error(IDLoc, Twine("wrong number of operands for '") + Row->Name +
                     "': expected " + Twine(NumExpected) + ", got " +
                     Twine(NumParsed));
    return true;
  }

  unsigned OpcodeOut = Row->Opcode;

  // Memory variant resolution and offset legality.
  if (Row->Opcode == Genpay::LOADS || Row->Opcode == Genpay::STORES) {
    const GenpayOperand &MemOp =
        *static_cast<const GenpayOperand *>(Operands[2].get());
    if (!MemOp.isMem()) {
      Error(MemOp.getStartLoc(), "expected '[base]' or '[base, offset]'");
      return true;
    }
    bool IsLoad = Row->Opcode == Genpay::LOADS;
    OpcodeOut = MemOp.hasMemOffset()
                    ? (IsLoad ? Genpay::LOADSO : Genpay::STORESO)
                    : (IsLoad ? Genpay::LOADS : Genpay::STORES);
  } else if (Row->Opcode == Genpay::LRS || Row->Opcode == Genpay::SCS) {
    unsigned MemIdx = Row->Opcode == Genpay::LRS ? 2 : 3;
    const GenpayOperand &MemOp =
        *static_cast<const GenpayOperand *>(Operands[MemIdx].get());
    if (!MemOp.isMem()) {
      Error(MemOp.getStartLoc(), "expected '[base]'");
      return true;
    }
    if (MemOp.hasMemOffset()) {
      Error(MemOp.getStartLoc(),
            "lr.s / sc.s do not support offsets; address must be [base]");
      return true;
    }
  }

  MCInst Inst;
  Inst.setOpcode(OpcodeOut);

  unsigned OpIdx = 1;
  for (unsigned i = 0; i < Row->NumShapes; ++i) {
    const GenpayOperand &Op =
        *static_cast<const GenpayOperand *>(Operands[OpIdx++].get());
    switch (Row->Shapes[i]) {
    case OpShape::Reg:
      if (!Op.isReg()) {
        Error(Op.getStartLoc(), "expected register operand");
        return true;
      }
      if (isLedgerOrGasRegister(Op.getReg())) {
        Error(Op.getStartLoc(),
              "ledger (b0-b7) and gas (g0) registers are not addressable by "
              "this operand; use a general purpose register r0-r14 or sp");
        return true;
      }
      Op.addRegOperands(Inst);
      break;
    case OpShape::Imm:
      if (!Op.isImm()) {
        Error(Op.getStartLoc(), "expected immediate operand");
        return true;
      }
      Op.addImmOperands(Inst);
      break;
    case OpShape::BrTarget:
      if (!Op.isBrTarget()) {
        Error(Op.getStartLoc(), "expected branch target");
        return true;
      }
      Op.addBrTargetOperands(Inst);
      break;
    case OpShape::Mem:
      if (!Op.isMem()) {
        Error(Op.getStartLoc(), "expected memory operand");
        return true;
      }
      Op.addMemOperands(Inst);
      break;
    }
  }

  // The nop alias expands to the canonical encoding NOP.
  if (Row->NumShapes == 0 && MnemonicOp.getToken() == "nop") {
    Inst = MCInst();
    Inst.setOpcode(Genpay::ADDri);
    Inst.addOperand(MCOperand::createReg(Genpay::R0));
    Inst.addOperand(MCOperand::createReg(Genpay::R0));
    Inst.addOperand(MCOperand::createImm(0));
  }

  Inst.setLoc(IDLoc);
  Opcode = Inst.getOpcode();
  Out.emitInstruction(Inst, *STI);
  return false;
}

bool GenpayAsmParser::parseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                    SMLoc &EndLoc) {
  AsmLexer &L = getLexer();
  StartLoc = L.getLoc();
  if (L.isNot(AsmToken::Identifier)) {
    Error(StartLoc, "expected register");
    return true;
  }
  MCRegister R = matchRegisterName(L.getTok().getIdentifier());
  if (!R) {
    Error(StartLoc, "unknown register '" + L.getTok().getIdentifier() + "'");
    return true;
  }
  EndLoc = L.getTok().getEndLoc();
  L.Lex();
  Reg = R;
  return false;
}

ParseStatus GenpayAsmParser::tryParseRegister(MCRegister &Reg, SMLoc &StartLoc,
                                              SMLoc &EndLoc) {
  AsmLexer &L = getLexer();
  StartLoc = L.getLoc();
  if (L.isNot(AsmToken::Identifier))
    return ParseStatus::NoMatch;
  MCRegister R = matchRegisterName(L.getTok().getIdentifier());
  if (!R)
    return ParseStatus::NoMatch;
  EndLoc = L.getTok().getEndLoc();
  L.Lex();
  Reg = R;
  return ParseStatus::Success;
}

} // namespace

extern "C" LLVM_EXTERNAL_VISIBILITY void LLVMInitializeGenpayAsmParser() {
  RegisterMCAsmParser<GenpayAsmParser> X(getTheGenpayTarget());
}
