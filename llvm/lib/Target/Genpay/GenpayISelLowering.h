//===-- GenpayISelLowering.h - Genpay DAG Lowering Interface -*- C++ -*----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_GENPAY_GENPAYISELLOWERING_H
#define LLVM_LIB_TARGET_GENPAY_GENPAYISELLOWERING_H

#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/AtomicOrdering.h"

namespace llvm {

class GenpaySubtarget;

namespace GenpayISD {
enum NodeType : unsigned {
  FIRST_NUMBER = ISD::BUILTIN_OP_END,
  RET, ///< Return from function.
};
} // namespace GenpayISD

class GenpayTargetLowering : public TargetLowering {
public:
  GenpayTargetLowering(const TargetMachine &TM, const GenpaySubtarget &STI);

  SDValue LowerFormalArguments(SDValue Chain, CallingConv::ID CallConv,
                               bool IsVarArg,
                               const SmallVectorImpl<ISD::InputArg> &Ins,
                               const SDLoc &DL, SelectionDAG &DAG,
                               SmallVectorImpl<SDValue> &InVals) const override;

  SDValue LowerOperation(SDValue Op, SelectionDAG &DAG) const override;

  SDValue LowerReturn(SDValue Chain, CallingConv::ID CallConv, bool IsVarArg,
                      const SmallVectorImpl<ISD::OutputArg> &Outs,
                      const SmallVectorImpl<SDValue> &OutVals, const SDLoc &DL,
                      SelectionDAG &DAG) const override;

  //===--------------------------------------------------------------------===//
  // Smart-contract hardware hooks
  //===--------------------------------------------------------------------===//

  // Route cmpxchg through AtomicExpandPass, which will build an
  // lr.s / sc.s loop using emitLoadLinked / emitStoreConditional below.
  TargetLoweringBase::AtomicExpansionKind
  shouldExpandAtomicCmpXchgInIR(AtomicCmpXchgInst *CI) const override;

  // Emits a call to @llvm.genpay.lr.s for AtomicExpandPass.
  Value *emitLoadLinked(IRBuilderBase &Builder, Type *ValueTy, Value *Addr,
                        AtomicOrdering Ord) const override;

  // Emits a call to @llvm.genpay.sc.s for AtomicExpandPass. Returns i32,
  // 0 on success, non-zero on failure.
  Value *emitStoreConditional(IRBuilderBase &Builder, Value *Val, Value *Addr,
                              AtomicOrdering Ord) const override;
};

} // namespace llvm

#endif
