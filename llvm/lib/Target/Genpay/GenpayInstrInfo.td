//===-- GenpayInstrInfo.td - Genpay Instruction Definitions -*- tablegen -*-===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
// Instruction groups:
//   - ALU core        : add, addi, sub, and, or, xor, mov, li
//   - Control flow    : j, jeq, jne, jz, jnz, ret
//   - ADRS memory     : load.s / store.s (tag-checked loads and stores)
//   - ADRS lifecycle  : matk, mdcy, mrel (mark / decay / release)
//   - Ledger engine   : txdeposit (escrow into ledger register b0),
//                       txrelease (release escrow back to target)
//   - Hardware atomics: lr.s / sc.s (load-reserved / store-conditional)
//   - Gas engine      : setgas / readgas (loop-breaking gas meter on g0)
//   - Crypto pipeline : sha256r (hardware SHA-256 acceleration)
//
//===----------------------------------------------------------------------===//

// Target intrinsics (llvm.genpay.*) used by the ledger, gas and crypto
// instruction groups. They are defined in
// llvm/include/llvm/IR/IntrinsicsGenpay.td, which is pulled in transitively
// via Target.td -> Intrinsics.td (installed and registered by integrate.sh).

//===----------------------------------------------------------------------===//
// SelectionDAG node types
//===----------------------------------------------------------------------===//

def SDTGenpayRet : SDTypeProfile<0, 1, [SDTCisInt<0>]>;
def GenpayRet : SDNode<"GenpayISD::RET", SDTGenpayRet,
                       [SDNPHasChain, SDNPOptInGlue]>;

//===----------------------------------------------------------------------===//
// Operands
//===----------------------------------------------------------------------===//

// 16-bit signed immediate: usable both as an instruction operand and as a
// constrained pattern fragment (multiple-inheritance idiom, cf. Mips).
def simm16 : Operand<i64>, ImmLeaf<i64, [{
  return isInt<16>(Imm);
}]>;

def brtargetAsmOperand : AsmOperandClass {
  let Name = "BrTarget";
}
def brtarget : Operand<OtherVT> {
  let ParserMatchClass = brtargetAsmOperand;
}

//===----------------------------------------------------------------------===//
// Instruction base class
//===----------------------------------------------------------------------===//

class GenpayInst<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  let Namespace = "Genpay";
  let OutOperandList = outs;
  let InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

//===----------------------------------------------------------------------===//
// ALU core
//===----------------------------------------------------------------------===//

def ADDrr : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
                       "add\t$dst, $src1, $src2",
                       [(set GPR:$dst, (add GPR:$src1, GPR:$src2))]>;

def ADDri : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, simm16:$imm),
                       "addi\t$dst, $src1, $imm",
                       [(set GPR:$dst, (add GPR:$src1, simm16:$imm))]>;

def SUBrr : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
                       "sub\t$dst, $src1, $src2",
                       [(set GPR:$dst, (sub GPR:$src1, GPR:$src2))]>;

def ANDrr : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
                       "and\t$dst, $src1, $src2",
                       [(set GPR:$dst, (and GPR:$src1, GPR:$src2))]>;

def ORrr  : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
                       "or\t$dst, $src1, $src2",
                       [(set GPR:$dst, (or GPR:$src1, GPR:$src2))]>;

def XORrr : GenpayInst<(outs GPR:$dst), (ins GPR:$src1, GPR:$src2),
                       "xor\t$dst, $src1, $src2",
                       [(set GPR:$dst, (xor GPR:$src1, GPR:$src2))]>;

// Register move. NOTE: deliberately pattern-less! A pattern like
// (set GPR:$dst, GPR:$src) acts as a catch-all that morphs ANY node
// (e.g. ISD::Constant) into a self-move. Value copies are produced by the
// CopyToReg/CopyFromReg machinery as generic COPY instructions, which the
// register allocator and GenpayMCInstLower (COPY -> MOVrr) handle.
def MOVrr : GenpayInst<(outs GPR:$dst), (ins GPR:$src),
                       "mov\t$dst, $src", []>;

// Load any 64-bit constant (imm = ISD::Constant leaf from TargetSelectionDAG.td)
def LI : GenpayInst<(outs GPR:$dst), (ins i64imm:$imm),
                    "li\t$dst, $imm",
                    [(set GPR:$dst, imm:$imm)]>;

//===----------------------------------------------------------------------===//
// Control flow
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1 in {
  def J : GenpayInst<(outs), (ins brtarget:$dst),
                     "j\t$dst",
                     [(br bb:$dst)]> {
    let isBarrier = 1;
  }

  // NOTE: these match ISD::BR_CC directly. LegalizeDAG rewrites every
  // brcond(setcc) into BR_CC (LegalizeDAG.cpp, case ISD::BRCOND), so
  // patterns on brcond(setcc) would never fire.
  def JEQ : GenpayInst<(outs), (ins GPR:$lhs, GPR:$rhs, brtarget:$dst),
                       "jeq\t$lhs, $rhs, $dst",
                       [(brcc SETEQ, GPR:$lhs, GPR:$rhs, bb:$dst)]>;

  def JNE : GenpayInst<(outs), (ins GPR:$lhs, GPR:$rhs, brtarget:$dst),
                       "jne\t$lhs, $rhs, $dst",
                       [(brcc SETNE, GPR:$lhs, GPR:$rhs, bb:$dst)]>;

  def JZ : GenpayInst<(outs), (ins GPR:$lhs, brtarget:$dst),
                      "jz\t$lhs, $dst",
                      [(brcc SETEQ, GPR:$lhs, 0, bb:$dst)]>;

  def JNZ : GenpayInst<(outs), (ins GPR:$lhs, brtarget:$dst),
                       "jnz\t$lhs, $dst",
                       [(brcc SETNE, GPR:$lhs, 0, bb:$dst)]>;
}

def RET : GenpayInst<(outs), (ins GPR:$src),
                     "ret\t$src",
                     [(GenpayRet GPR:$src)]> {
  let isReturn = 1;
  let isTerminator = 1;
  let hasCtrlDep = 1;
  let isBarrier = 1;
}

//===----------------------------------------------------------------------===//
// ADRS engine: tagged memory (load.s / store.s)
//
// Every ordinary memory access is routed through the ADRS tag engine.
// load.s traps natively if the addressed line is not in the Sustain (0x3)
// lifecycle phase. Offsets are 16-bit signed byte offsets.
//
// Format note (ISA spec conformance): the spec's TableGen fragment writes
// the LOADS template as "load.s\tdst,[src, $offset]". The '$' there is a
// TableGen operand REFERENCE, not a printed character; all Genpay templates
// use the consistent "$dst / $addr / $off" reference style and print bare
// register names ("load.s r0, [r0, 32]"), exactly as in the spec's own lit
// tests. The explicit zero-offset text form "load.s r0, [r0, 0]" also
// assembles (it selects LOADSO); the compiler prints the short "[r0]" form.
//===----------------------------------------------------------------------===//

def LOADS : GenpayInst<(outs GPR:$dst), (ins GPR:$addr),
                       "load.s\t$dst, [$addr]",
                       [(set GPR:$dst, (load GPR:$addr))]>;

def LOADSO : GenpayInst<(outs GPR:$dst), (ins GPR:$addr, simm16:$off),
                        "load.s\t$dst, [$addr, $off]",
                        [(set GPR:$dst,
                          (load (add GPR:$addr, simm16:$off)))]>;

def STORES : GenpayInst<(outs), (ins GPR:$src, GPR:$addr),
                        "store.s\t$src, [$addr]",
                        [(store GPR:$src, GPR:$addr)]>;

def STORESO : GenpayInst<(outs), (ins GPR:$src, GPR:$addr, simm16:$off),
                         "store.s\t$src, [$addr, $off]",
                         [(store GPR:$src,
                           (add GPR:$addr, simm16:$off))]>;

//===----------------------------------------------------------------------===//
// ADRS engine: lifecycle management
//
// The ISA spec defines these three instructions with empty pattern lists
// (assembler-only). The backend completes the surface with the
// @llvm.genpay.mem.* intrinsics so the whole lifecycle is reachable from
// IR, following the same selection style as lr.s/sc.s/setgas:
//   matk <- i64  @llvm.genpay.mem.mark(i64 %size)   ; returns region base
//   mdcy <- i64  @llvm.genpay.mem.decay(i64 %addr)  ; returns pre-decay tag
//   mrel <- void @llvm.genpay.mem.release(i64 %addr)
//===----------------------------------------------------------------------===//

def MATK : GenpayInst<(outs GPR:$dst), (ins GPR:$size),
                      "matk\t$dst, $size",
                      [(set GPR:$dst,
                        (int_genpay_mem_mark GPR:$size))]>;

def MDCY : GenpayInst<(outs GPR:$dst), (ins GPR:$src),
                      "mdcy\t$dst, $src",
                      [(set GPR:$dst,
                        (int_genpay_mem_decay GPR:$src))]>;

def MREL : GenpayInst<(outs), (ins GPR:$src),
                      "mrel\t$src",
                      [(int_genpay_mem_release GPR:$src)]>;

//===----------------------------------------------------------------------===//
// Ledger engine: isolated transactional instructions
//
// txdeposit atomically debits %source by %amount into the hardware escrow
// slot of ledger register b0 and returns the escrow handle in %dst. The
// handle is simultaneously visible in b0 to other agents.
//===----------------------------------------------------------------------===//

// txdeposit note (ISA spec conformance): the spec writes the definition as
// (outs LGR:$escrow) with the 3-operand text form "txdeposit b0, r0, r1".
// Ledger registers are reserved (never allocated), so an LGR result cannot
// be the allocated destination of the SelectionDAG value, and a text form
// without a visible destination cannot be re-assembled. The backend prints
// the lossless 4-operand form with the GPR result first and the literal b0
// escrow token second; the hardware effect on b0 is recorded via Defs=[B0],
// which keeps the LGR class perfectly synchronized with SelectionDAG (the
// register allocator never sees a value in the ledger file).
def TXDEPOSIT : GenpayInst<(outs GPR:$dst),
                           (ins GPR:$source, GPR:$amount),
                           "txdeposit\t$dst, b0, $source, $amount",
                           [(set GPR:$dst,
                             (int_genpay_tx_deposit GPR:$source,
                                                    GPR:$amount))]> {
  let Defs = [B0];
}

def TXRELEASE : GenpayInst<(outs), (ins GPR:$target, GPR:$escrow),
                           "txrelease\t$target, $escrow",
                           [(int_genpay_tx_release GPR:$target,
                                                   GPR:$escrow)]>;

//===----------------------------------------------------------------------===//
// Hardware atomics: load-reserved / store-conditional
//
// lr.s places a reservation on the addressed line; sc.s completes the
// store only if the reservation is still valid and writes 0 into $dst on
// success (non-zero on failure). cmpxchg is expanded to an lr.s/sc.s loop
// by AtomicExpandPass.
//===----------------------------------------------------------------------===//

def LRS : GenpayInst<(outs GPR:$dst), (ins GPR:$addr),
                     "lr.s\t$dst, [$addr]",
                     [(set GPR:$dst, (int_genpay_lr_s GPR:$addr))]> {
  let mayLoad = 1;
}

def SCS : GenpayInst<(outs GPR:$dst), (ins GPR:$val, GPR:$addr),
                     "sc.s\t$dst, $val, [$addr]",
                     [(set GPR:$dst,
                       (int_genpay_sc_s GPR:$val, GPR:$addr))]> {
  let mayStore = 1;
}

//===----------------------------------------------------------------------===//
// Gas engine: loop-breaking meter (register g0)
//===----------------------------------------------------------------------===//

def SETGAS : GenpayInst<(outs), (ins GPR:$src),
                        "setgas\t$src",
                        [(int_genpay_set_gas GPR:$src)]> {
  let Defs = [G0];
}

def READGAS : GenpayInst<(outs GPR:$dst), (ins),
                         "readgas\t$dst",
                         [(set GPR:$dst, (int_genpay_read_gas))]> {
  let Uses = [G0];
}

//===----------------------------------------------------------------------===//
// Crypto pipeline: hardware SHA-256 accelerator
//===----------------------------------------------------------------------===//

def SHA256R : GenpayInst<(outs GPR:$dst), (ins GPR:$addr, GPR:$len),
                         "sha256r\t$dst, $addr, $len",
                         [(set GPR:$dst,
                           (int_genpay_sha256_r GPR:$addr, GPR:$len))]> {
  let mayLoad = 1;
}
