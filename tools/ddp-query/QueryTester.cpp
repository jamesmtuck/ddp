//===- DDPInfoInAA.cpp - DDP Info exposed through the AA interface -------===//
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
// 
//
//===----------------------------------------------------------------------===//
//

#define DEBUG_TYPE "QueryTester"

#include "llvm/PassSupport.h"
#include "llvm/Pass.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/Dominators.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Instructions.h"

#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <map>
#include <iostream>
#include <fstream>
#include <vector>

#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"

namespace llvm {
class QueryTester : public ModulePass {
  
public:
  QueryTester() : ModulePass(ID) {
    //    array=NULL;
  }
  static char ID;
  bool runOnModule(Module &M);
  bool runOnFunction(Function &F);

  void getAnalysisUsage(AnalysisUsage &AU) const
  {
    AU.setPreservesAll();
    AU.setPreservesCFG();
    AU.addRequired<AliasAnalysis>();
  }
};

void initializeQueryTesterPass(PassRegistry&);
}

using namespace llvm;

char QueryTester::ID=0;

INITIALIZE_PASS_BEGIN(QueryTester, "QueryTester",
                "Query Tester", false, true)

//INITIALIZE_PASS_DEPENDENCY(DominatorTree)
INITIALIZE_AG_DEPENDENCY(AliasAnalysis)

INITIALIZE_PASS_END(QueryTester, "QueryTester",
                "Query Test", false, true)


bool QueryTester::runOnModule(Module &M) { 
    Module::iterator I;
    for(I=M.begin(); I!=M.end(); I++) {
      runOnFunction(*I);
    }
    return false;
}

STATISTIC(NoLoads, "Number of Loads");
STATISTIC(NoStores, "Number of Stores");

STATISTIC(NoLookUps, "Number of Look ups");



STATISTIC(MustAliases, "Number of Must Aliases");
STATISTIC(PartialAliases, "Number of Partial Aliases");
STATISTIC(MayAliases, "Number of May Aliases");
STATISTIC(NoAliases, "Number of No Aliases");


bool QueryTester::runOnFunction(Function &F) {

  std::vector<LoadInst*> loads;
  std::vector<StoreInst*> stores;

  AliasAnalysis *AA = &getAnalysis<AliasAnalysis>();

  for(Function::iterator fit=F.begin(),fend=F.end(); fit!=fend; fit++)
    {
      BasicBlock &BB = *fit;      
      for(BasicBlock::iterator bit=BB.begin(), bend=BB.end(); bit!=bend; bit++)
	{
	  Instruction *I = &*bit;
	  
	  if(StoreInst *SI = dyn_cast<StoreInst>(I))
	    {
	      stores.push_back(SI);
	      NoStores++;
	    }
	  else if(LoadInst *LI = dyn_cast<LoadInst>(I))
	    {
	      loads.push_back(LI);
	      NoLoads++;
	    }
	}
    }

  for(size_t i=0,lsize=loads.size(); i<lsize; i++)
    {
      LoadInst *LI = loads[i];
      AliasAnalysis::Location loadLoc = AA->getLocation(LI);
      for(size_t j=0,ssize=stores.size(); j<ssize; j++)
	{
	  StoreInst *SI = stores[j];
	  AliasAnalysis::Location storeLoc = AA->getLocation(SI);
	  
	  NoLookUps++;

	  AliasAnalysis::AliasResult res = AA->alias(loadLoc,storeLoc);
	  switch (res) {
	  case AliasAnalysis::MayAlias:
	    MayAliases++;
	    break;
	  case AliasAnalysis::PartialAlias:
	    PartialAliases++;
	    break;
	  case AliasAnalysis::MustAlias:
	    MustAliases++;
	    break;
	  case AliasAnalysis::NoAlias:
	    NoAliases++;
	  }	  
	}
    }

  return false;
}

