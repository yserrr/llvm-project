//===- VecUtils.cpp -------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Vectorize/SandboxVectorizer/VecUtils.h"

#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/SandboxIR/Instruction.h"
#include "llvm/Transforms/Vectorize/SandboxVectorizer/InstrMaps.h"

namespace llvm::sandboxir {

SmallVector<BundleTy> VecUtils::getNextUserBundles(ArrayRef<Value *> Bndl,
                                                   const InstrMaps &IMaps) {
  SmallVector<BundleTy> Bundles;
  if (Bndl.empty())
    return Bundles;
  Value *V0 = Bndl[0];
  DenseSet<User *> SeenUsers;
  for (User *U0 : V0->users()) {
    if (!SeenUsers.insert(U0).second)
      continue;
    BundleTy NextUserBndl = getNextUserBundle(Bndl, U0, V0, IMaps);
    if (NextUserBndl.size() == Bndl.size())
      Bundles.emplace_back(std::move(NextUserBndl));
  }
  return Bundles;
}

BundleTy VecUtils::getNextUserBundle(ArrayRef<Value *> Bndl, User *U0,
                                     Value *V0, const InstrMaps &IMaps) {
  auto *UI0 = dyn_cast<Instruction>(U0);
  if (!UI0 || IMaps.isVectorized(UI0))
    return {};

  // Find the operand index at which U0 uses lane 0 (V0).
  auto GetOpIdxVec = [](Value *V, User *U) -> SmallVector<unsigned> {
    SmallVector<unsigned, 2> OpIdxVec;
    for (unsigned Idx : seq<unsigned>(U->getNumOperands())) {
      if (U->getOperand(Idx) == V) {
        OpIdxVec.emplace_back(Idx);
      }
    }
    return OpIdxVec;
  };

  // Find a distinct matching user for each of the remaining lanes.
  BundleTy NextUserBndl;
  NextUserBndl.push_back(UI0);
  SmallPtrSet<Instruction *, 4> Claimed;
  Claimed.insert(UI0);
  for (Value *V : drop_begin(Bndl)) {
    Instruction *Match = nullptr;
    for (User *U : V->users()) {
      auto *UI = dyn_cast<Instruction>(U);
      if (!UI || IMaps.isVectorized(UI) || Claimed.contains(UI))
        continue;
      if (UI->getOpcode() != UI0->getOpcode() ||
          UI->getType() != UI0->getType())
        continue;
      if (UI->getParent() != UI0->getParent())
        continue;

      SmallVector<unsigned> OpIdxVec = GetOpIdxVec(V, UI);
      if (OpIdxVec.size() == 1) {
        if (UI->getOperand(OpIdxVec[0]) != V)
          continue;
      } else {
        /// TODO: What to do if multiple operands of UI are V?
      }

      Match = UI;
      break;
    }
    if (!Match)
      return {};
    Claimed.insert(Match);
    NextUserBndl.push_back(Match);
  }
  return NextUserBndl;
}

unsigned VecUtils::getFloorPowerOf2(unsigned Num) {
  if (Num == 0)
    return Num;
  unsigned Mask = Num;
  Mask >>= 1;
  for (unsigned ShiftBy = 1; ShiftBy < sizeof(Num) * 8; ShiftBy <<= 1)
    Mask |= Mask >> ShiftBy;
  return Num & ~Mask;
}

#ifndef NDEBUG
template <typename T> static void dumpImpl(ArrayRef<T *> Bndl) {
  for (auto [Idx, V] : enumerate(Bndl))
    dbgs() << Idx << "." << *V << "\n";
}
void VecUtils::dump(ArrayRef<Value *> Bndl) { dumpImpl(Bndl); }
void VecUtils::dump(ArrayRef<Instruction *> Bndl) { dumpImpl(Bndl); }
#endif // NDEBUG

} // namespace llvm::sandboxir
