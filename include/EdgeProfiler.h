#ifndef EDGEPROFILER_H
#define EDGEPROFILER_H

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

//#include "InterfGraphGen.h"
//#include "SetAssign.h"

namespace llvm {

void initializeEdgeProfilerPass(PassRegistry&);
void initializeEdgeProbabilityPass(PassRegistry&);
void initializeEdgeProbabilityReaderPass(PassRegistry&);

class EdgeProbabilityReader : public ImmutablePass {  

public:
  EdgeProbabilityReader() : ImmutablePass(ID) {}
  static char ID;

  int64_t getExecutionCount(BasicBlock*);
  int64_t getEdgeExecutionCount(BasicBlock*from,BasicBlock*to);
  double   getNormalizedExecutionFrequency(BasicBlock*);

  bool checkValidity(BasicBlock *);

  // Todo: Add conditional probability function?
};

}

#endif
