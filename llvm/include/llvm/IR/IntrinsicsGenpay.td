//===-- IntrinsicsGenpay.td - Genpay Target Intrinsics ------*- tablegen -*-===//
//
// Part of the Genpay Smart-Contract ISA backend for LLVM 21.
//
// Target-specific intrinsics that map directly onto the smart-contract
// hardware engines of the Genpay ISA. All of them are conservative (no
// IntrNoMem / IntrReadMem properties) so the optimizer can never reorder
// or remove them relative to each other.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "genpay" in {

/// i64 @llvm.genpay.lr.s(ptr %addr)
/// Load-reserved from an ADRS-tagged line; places a hardware reservation.
def int_genpay_lr_s : Intrinsic<[llvm_i64_ty], [llvm_ptr_ty]>;

/// i64 @llvm.genpay.sc.s(i64 %val, ptr %addr)
/// Store-conditional %val to %addr. Returns 0 on success, non-zero on
/// failure (reservation lost).
def int_genpay_sc_s : Intrinsic<[llvm_i64_ty],
                                [llvm_i64_ty, llvm_ptr_ty]>;

/// i64 @llvm.genpay.tx.deposit(i64 %source, i64 %amount)
/// Hardware escrow deposit into ledger register b0; returns the escrow
/// handle.
def int_genpay_tx_deposit : Intrinsic<[llvm_i64_ty],
                                      [llvm_i64_ty, llvm_i64_ty]>;

/// void @llvm.genpay.tx.release(i64 %target, i64 %escrow_handle)
/// Release an escrow slot back to the target wallet.
def int_genpay_tx_release : Intrinsic<[], [llvm_i64_ty, llvm_i64_ty]>;

/// void @llvm.genpay.set.gas(i64 %budget)
/// Load the loop-breaking gas meter register g0 with %budget.
def int_genpay_set_gas : Intrinsic<[], [llvm_i64_ty]>;

/// i64 @llvm.genpay.read.gas()
/// Read the current gas counter value from g0.
def int_genpay_read_gas : Intrinsic<[llvm_i64_ty], []>;

/// i64 @llvm.genpay.sha256.r(i64 %addr, i64 %len)
/// Run the hardware SHA-256 pipeline over [%addr, %addr+%len); returns the
/// handle of the digest slot.
def int_genpay_sha256_r : Intrinsic<[llvm_i64_ty],
                                    [llvm_i64_ty, llvm_i64_ty]>;

/// i64 @llvm.genpay.mem.mark(i64 %size)
/// Ask the ADRS engine to mark (materialize) a managed region of %size
/// bytes. Returns the base address of the region; its lines start in the
/// Sustain (0x3) lifecycle phase, where load.s / store.s operate.
/// Selected as: matk $dst, $size
def int_genpay_mem_mark : Intrinsic<[llvm_i64_ty], [llvm_i64_ty]>;

/// i64 @llvm.genpay.mem.decay(i64 %addr)
/// Decay the lifecycle tag of the line containing %addr: the line leaves
/// the Sustain phase, so later load.s / store.s on it trap until the line
/// is marked again. Returns the line's tag word as it was before the decay.
/// Selected as: mdcy $dst, $src
def int_genpay_mem_decay : Intrinsic<[llvm_i64_ty], [llvm_i64_ty]>;

/// void @llvm.genpay.mem.release(i64 %addr)
/// Release the line containing %addr back to the ADRS free pool.
/// Selected as: mrel $src
def int_genpay_mem_release : Intrinsic<[], [llvm_i64_ty]>;

} // end TargetPrefix = "genpay"
