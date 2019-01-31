//===- SetAssign.h - Set Profiler for data dependence profiling ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef NEW_PROJECT_SETASSIGN_H
#define NEW_PROJECT_SETASSIGN_H

#include "llvm/PassSupport.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <map>
#include <iostream>
#include <fstream>
#include "GenerateQueries.h"

typedef std::map<unsigned int, unsigned int> IntMapType;

namespace ddp {

  class AssignQueries : public Queries {
  public:
    AssignQueries();
    std::map<llvm::Instruction*,unsigned int> SetAssignments;

    int maxSetSize;
  public:
    virtual void assignSets(Queries &CQ);

    int getMaxSetSize() {  return maxSetSize; }
    void setMaxSetSize(int s) { maxSetSize = s; }
  };

  class LinearAssignQueries : public AssignQueries {
  public:
    virtual void assignSets(Queries &CQ);
  };

}

#endif
