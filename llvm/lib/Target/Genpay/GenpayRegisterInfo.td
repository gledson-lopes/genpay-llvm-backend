//===-- GenpayRegisterInfo.td - Genpay Register Definitions -*- tablegen -*-===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
// Hardware register files:
//   GPR : r0-r14   general purpose registers (ADRS-tagged: top bits [63:60]
//                  carry the ADRS lifecycle tag managed by load.s/matk/mdcy)
//   SPR : sp (r15) stack pointer, never allocated by the register allocator
//   LGR : b0-b7    isolated financial ledger registers, decoupled from the
//                  generic ALU; only ledger instructions (txdeposit,
//                  txrelease) and the gas engine touch them
//   GSR : g0       dedicated hardware gas counter register
//
//===----------------------------------------------------------------------===//

class GenpayReg<bits<16> num, string n> : Register<n> {
  let HWEncoding = num;
  let Namespace = "Genpay";
}

// General Purpose Registers (r0 - r14)
foreach i = 0-14 in {
  def R#i : GenpayReg<i, "r"#i>;
}

// Stack Pointer (r15)
def SP : GenpayReg<15, "sp">;

// Isolated Financial Ledger Registers (b0 - b7, encodings 16-23)
foreach i = 0-7 in {
  def B#i : GenpayReg<!add(16, i), "b"#i>;
}

// Dedicated Hardware Gas Counter Register (g0, encoding 24)
def G0 : GenpayReg<24, "g0">;

// Register Classes for instruction selection matching
def GPR : RegisterClass<"Genpay", [i64], 64, (add
  R0, R1, R2, R3, R4, R5, R6, R7,
  R8, R9, R10, R11, R12, R13, R14)>;

def SPR : RegisterClass<"Genpay", [i64], 64, (add SP)>;
def LGR : RegisterClass<"Genpay", [i64], 64, (add
  B0, B1, B2, B3, B4, B5, B6, B7)>;
def GSR : RegisterClass<"Genpay", [i64], 64, (add G0)>;
