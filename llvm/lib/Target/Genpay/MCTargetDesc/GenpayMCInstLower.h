//===-- GenpayMCInstLower.h - Lower MachineInstr to MCInst ---*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCINSTLOWER_H
#define LLVM_LIB_TARGET_GENPAY_MCTARGETDESC_GENPAYMCINSTLOWER_H

namespace llvm {

class AsmPrinter;
class MCContext;
class MCInst;
class MCOperand;
class MachineInstr;
class MachineOperand;

class GenpayMCInstLower {
  MCContext &Ctx;
  AsmPrinter &Printer;

public:
  GenpayMCInstLower(MCContext &ctx, AsmPrinter &printer)
      : Ctx(ctx), Printer(printer) {}

  void Lower(const MachineInstr *MI, MCInst &OutMI) const;
  MCOperand LowerOperand(const MachineOperand &MO) const;
};

} // namespace llvm

#endif
