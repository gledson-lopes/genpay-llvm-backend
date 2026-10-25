//===-- GenpayMCInstLower.cpp - Lower MachineInstr to MCInst -*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayMCInstLower.h"
#include "GenpayMCTargetDesc.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

void GenpayMCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  if (MI->getOpcode() == TargetOpcode::COPY) {
    OutMI.setOpcode(Genpay::MOVrr);
    for (const MachineOperand &MO : MI->explicit_operands())
      OutMI.addOperand(LowerOperand(MO));
    return;
  }

  OutMI.setOpcode(MI->getOpcode());
  for (const MachineOperand &MO : MI->operands()) {
    MCOperand MCOp = LowerOperand(MO);
    if (MCOp.isValid())
      OutMI.addOperand(MCOp);
  }
}

MCOperand GenpayMCInstLower::LowerOperand(const MachineOperand &MO) const {
  switch (MO.getType()) {
  default:
    llvm_unreachable("Genpay: unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore implicit register operands.
    if (MO.isImplicit())
      return MCOperand();
    return MCOperand::createReg(MO.getReg());
  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm());
  case MachineOperand::MO_MachineBasicBlock:
    return MCOperand::createExpr(
        MCSymbolRefExpr::create(MO.getMBB()->getSymbol(), Ctx));
  case MachineOperand::MO_ExternalSymbol: {
    MCSymbol *Sym = Printer.GetExternalSymbolSymbol(MO.getSymbolName());
    return MCOperand::createExpr(MCSymbolRefExpr::create(Sym, Ctx));
  }
  }
}
