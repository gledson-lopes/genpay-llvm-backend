//===-- GenpayFrameLowering.cpp - Frame lowering for Genpay -*- C++ -*-----===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
//===----------------------------------------------------------------------===//

#include "GenpayFrameLowering.h"

using namespace llvm;

GenpayFrameLowering::GenpayFrameLowering()
    : TargetFrameLowering(StackGrowsDown, Align(8), 0) {}
