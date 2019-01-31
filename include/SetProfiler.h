//===- SetProfiler.h - Set Profiler for data dependence profiling ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is an important class in signature-based DDP since all alaias analysis,
// query generation and instrumentation start from here.
//
//===----------------------------------------------------------------------===//

#ifndef DDP_SETPROFILER_H
#define DDP_SETPROFILER_H

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
#include "llvm/PassSupport.h"
#include "llvm/Pass.h"
#include "llvm/PassAnalysisSupport.h"
#include "ProfileDBHelper.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include <set>

namespace llvm {

void initializeSetProfilerPass(PassRegistry&);

class SetProfiler : public ModulePass {
 private:
  std::set<Function*> fList;
  Function *CloneAndInsertSampledFunctionCall(Function &F);

 public:
  static char ID;
  //QueryInterface *QI;
  ProfileDBHelper *dbHelper;

 SetProfiler() : ModulePass(ID), dbHelper(NULL) { }

 void getAnalysisUsage(AnalysisUsage &AU) const override {
 	AU.addRequired<AAResultsWrapperPass>();
   //AU.addRequired<EdgeProbabilityReader>();
 	 AU.setPreservesAll();
 }

  virtual bool runOnModule(Module &M);

  //virtual bool runOnModule(Module &M);

  virtual bool runOnFunction(Function *F);
  virtual bool doInitialization(Module &M);
  virtual bool doFinalization(Module &M);
};

}

#endif
