//===- SetProfiler.cpp - Set Profiler for data dependence profiling -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is an implementation of SetProfiler class in signature-based DDP where
// all alaias analysis query generation and instrumentation start from here for
// every function.
//
//===----------------------------------------------------------------------===//
//
#define DEBUG_TYPE "SetProfiler"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
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
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "EdgeProfiler.h"
#include "Instrument.h"
#include "SetAssign.h"
#include "SetProfiler.h"
#include <vector>
#include <sstream>
#include <iostream>

STATISTIC(NumProfiledFunctions, "Number of functions that are profiled");
STATISTIC(NumHotFunctions, "Number of hot functions recorded");
STATISTIC(NumExcludedFunctions, "Number of cold functions excluded");

using namespace std;
using namespace llvm;

extern cl::opt<bool> PerfInstr;

char SetProfiler::ID = 0;
static RegisterPass<SetProfiler> SP("SetProfiler",
																		"Set profiler instruments queries",
																		false, false);

//INITIALIZE_PASS_BEGIN(SetProfiler, "SetProfiler",
    //            "Perform instrumentation of queries", true, false)
//INITIALIZE_PASS_DEPENDENCY(DominatorTree)
//INITIALIZE_PASS_DEPENDENCY(QueryInterface)
//INITIALIZE_PASS_DEPENDENCY(EdgeProbabilityReader)
//INITIALIZE_PASS_END(SetProfiler, "SetProfiler",
  //              "Perform instrumentation of queries", true, false)

#if 0
static cl::opt<bool>
BasicInterf("basicinterf", cl::Hidden,
			 			cl::desc("Basic Interference Graph Gen is enabled"),
						cl::init(false));
#endif

static cl::opt<bool>
OnlyProfHotFns("only-prof-hot-fns", cl::Hidden,
			 				 cl::desc("Only profile the hot functions - those that get \
							 executed"), cl::init(false));

static cl::opt<int>
SampleRate("sample", cl::Hidden,
			 		 cl::desc("Perform Sample-based profiling for function every N times \
					 its called (defaults to 1)"), cl::init(1));

static cl::opt<std::string>
DBTableName("db-table-name-override", cl::Hidden,
			 			cl::desc("Name of the DB table that will hold feedback data"),
						cl::init("feedback"));

static cl::opt<std::string>
dumpIRfile("dump-IR-beforeSetProfiling", cl::Hidden,
          cl::desc("Dump IR to this file, just before set profiling"),
					cl::init(""));

/*SetAssign* assign(InterfGraphGen& IG, QueryInterface& QI) {

	SetAssign* SA = new GreedySetAssign(IG, QI);
	SA->assignSets();
	return SA;
	}*/

//bool SetProfiler::runOnModule(Module &M) {
//  dbHelper = new ProfileDBHelper(M, "ddp");
//  bool changed = false;
//
//  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
//    if (MI->isDeclaration())
//      continue;
//    QI = &getAnalysis<QueryInterface>(*MI);
//    changed |= runOnFunction(*MI);
//  }
//
//  dbHelper->finishModule();
//  delete dbHelper;
//  dbHelper = NULL;
//
//  return changed;
//}

/*
Function* SetProfiler::CloneAndInsertSampledFunctionCall(Function &F)
{
  ValueToValueMapTy VMap;
  //Function *clone = CloneFunction(&F,VMap,true,NULL);
  Function *clone = CloneFunction(&F,VMap);
  F.getParent()->getFunctionList().push_back(clone);

  BasicBlock &Entry = F.getEntryBlock();
#ifdef DDP_LLVM_VERSION_3_7
  LoopInfo *LI = NULL;
  DominatorTree *DT = NULL;
  if(DominatorTreeWrapperPass *DTWP =
			getAnalysisIfAvailable<DominatorTreeWrapperPass>()) DT=&DTWP->getDomTree();
  if(LoopInfoWrapperPass *LIWP = getAnalysisIfAvailable<LoopInfoWrapperPass>())
		LI=&LIWP->getLoopInfo();
  BasicBlock *split = SplitBlock(&Entry, &*Entry.begin(), DT, LI);
#else
  //BasicBlock *split = SplitBlock(&Entry,&*Entry.begin(),this);
	LoopInfo *LI = NULL;
	DominatorTree *DT = NULL;
	if(DominatorTreeWrapperPass *DTWP =
												getAnalysisIfAvailable<DominatorTreeWrapperPass>())
			 	DT=&DTWP->getDomTree();
	if(LoopInfoWrapperPass *LIWP = getAnalysisIfAvailable<LoopInfoWrapperPass>())
		LI=&LIWP->getLoopInfo();
	BasicBlock *split = SplitBlock(&Entry, &*Entry.begin(), DT, LI);
#endif
  BasicBlock *newBB = BasicBlock::Create(*context,"",&F,split);

  Entry.getTerminator()->eraseFromParent();
  std::string t=("ddp_sample_number_cntr");
  Type *int32 = IntegerType::get(*context,32);

  IRBuilder<> B(&Entry);
  GlobalVariable *gv = new GlobalVariable(*F.getParent(),int32,
					 false,llvm::GlobalValue::PrivateLinkage,
					 B.getInt32(0),t);

  {
    /*  Make entry block look like
	if ( --ddp_sample_number_cntr < 0 )
            goto newBB
        else
	    goto split
     *

    IRBuilder<> Builder(&Entry);

    LoadInst *LI = Builder.CreateLoad(gv,false,"ddp_sample_no");
    Builder.CreateStore(Builder.CreateSub(LI,Builder.getInt32(1)),gv,false);

    Value *Cmp = Builder.CreateICmpSLE(LI,Builder.getInt32(0));

    Builder.CreateCondBr(Cmp,newBB,split);
  }

  {
    /* Make newBB look like this:
       ddp_sample_number_cntr=sample_rate
       r = clone(...)
       ret r
     *
    IRBuilder<> Builder(newBB);
    Builder.CreateStore(Builder.getInt32(SampleRate),gv,false);

    std::vector<Value*> args;
    for(Function::arg_iterator it=F.arg_begin(),end=F.arg_end(); it!=end; it++)
      {
	args.push_back(&*it);
      }
    ArrayRef<Value*> Args(args);
    Value *ret = Builder.CreateCall(clone,Args);
    if (ret->getType()->isVoidTy())
      Builder.CreateRetVoid();
    else
      Builder.CreateRet(ret);
  }

  fList.insert(clone);
  return clone;
}
*/

bool SetProfiler::runOnFunction(Function *F) {
  errs() << "RUN ON FUNCTION: " << F->getName() << "\n";
  DEBUG_WITH_TYPE("ddp", outs() << "Function: " << F->getName() << "\n");
  if(!F->size()) {
	  errs() << "FUNCTION IS EMPTY\n";
	  return false;
  }

  /*
  EdgeProbabilityReader &EPR = getAnalysis<EdgeProbabilityReader>();

  // Yes, it looks weird and is seemingly inefficient,
  // but label all dependences first, so that we have a consistent
  // marking
  BasicBlock *BB = &F->getEntryBlock();

  if(!EPR.checkValidity(BB)) {
    assert(0 && "edge profiler annotations are broken!");
  }
  int64_t count = EPR.getExecutionCount(BB);
  if(SampleRate>1) {
    // clone function and evaluate clone
    if(!(OnlyProfHotFns && count==0))
      {
	Function *f = CloneAndInsertSampledFunctionCall(*F);
	F = f;
      }
    // getAnalysis<GCMQueries>(*F);
    //QI = &getAnalysis<QueryInterface>(*F);
  } else {
    //getAnalysis<GCMQueries>(*F);
    //QI = &getAnalysis<QueryInterface>(*F);
  }

  // Break if we only want ot instrument hot functions and this one is cold
  if(OnlyProfHotFns) {
    // Its not the main function
    if(count==0) {
      if(strcmp(F->getName().data(), "main") == 0) {
	assert(0 && "main should execute more than 0 times!");
      }
      // Function is not executed
      DEBUG_WITH_TYPE("ddp",outs() << "OnlyProfileHotFns: Function: " << F->getName()
		      << " -- execCount: " << EPR.getExecutionCount(BB) << " -- Not profiled\n");
      NumExcludedFunctions++;
      return false;
    } else {
      NumHotFunctions++;
    }
  }
  */

  ddp::MayAliasQueries AliasQueries;

  ddp::AssignQueries AAQ;
  ddp::LinearAssignQueries LAQ;
  ddp::AssignQueries *AQ = &AAQ;

  //Queries.run(*F,getAnalysis<AliasAnalysis>(),*dbHelper);
  errs() << "---ALIAS QUERIES\n";
  //AAResultsWrapperPass *aaresults = new AAResultsWrapperPass();
  //aaresults->runOnFunction(*F);
  //errs() << "WORKING\n";
  //AliasAnalysis *aliasAnalysis = &(getAnalysis<AAResultsWrapperPass>().getAAResults());
  //errs() << "ALIAS ANALYSIS COMPLETE: " << aliasAnalysis << "\n";
  AliasQueries.run(*F, getAnalysis<AAResultsWrapperPass>(*F),*dbHelper);
  errs() << "ALIAS QUERIES\n";
  /*
  ddp::Queries::query_iterator qi,qend=AliasQueries.end();
  for(qi=AliasQueries.begin(); qi!=qend; qi++)
  {
    EPR.checkValidity((*qi).lhs->getParent());

    // set total count to that of the enclosing function
    (*qi).total = count; //EPR.getExecutionCount((*qi).lhs->getParent());
  }

  if (PerfInstr)
    {
      printf("PerfectInstrumentation!\n");
      AQ = &AAQ;
      // This must be set this way, or we'll get weird results
      // Unfortunately, this also means much more overhead
      AQ->setMaxSetSize(1);
    }
  /*else if(Queries.size() > 10000)
    {
      // It will be very expensive
      // to instrument, so select cheap instrumentation strategy
      AQ = &AAQ;
      AQ->setMaxSetSize(-1);
      }*/

  AQ->assignSets(AliasQueries);
  errs() << "SETS ASSIGNED\n";

  // Must be here!!!
  if(AliasQueries.size()>0) {
	errs() << "ALIAS QUERIES NON ZERO\n";
    NumProfiledFunctions++;
    DEBUG_WITH_TYPE("ddp", outs() << "Instrumenting Function: "
																	<< F->getName() << "\n");
  }

  SetInstrument Instr(*AQ, *F, *dbHelper);
  errs() << "START INSTRUMENTATION\n";
  return Instr.instrument(this, *F);
  //return true;
}

bool SetProfiler::doInitialization(Module &M) {
  dbHelper = new ProfileDBHelper(M, "ddp");

  // Override default table name if this flag is set
  // This will allow comparing various design alternatives
  if( DBTableName.getNumOccurrences() > 0 )
    {
      dbHelper->setTableName(DBTableName);
    }
  else
    {
      std::string tableName = "feedback";
#if 0
      // Remove this additional logic and just call it feedback
      // That way the name is only changed if the user requests it
      if (PerfInstr)
	tableName = "perf_" + tableName;
      if (HTInstr)
	tableName = "ht_" + tableName;
      std::stringstream ss;
      ss << SampleRate;
      if (SampleRate > 1)
	tableName = tableName + "_sample_" + ss.str();
#endif

      dbHelper->setTableName(tableName);
    }

  return false;
}

bool SetProfiler::doFinalization(Module &M) {
	/*
  // Changes to module in doFinalization() don't seem to take effect,
  // even with return true
  //dbHelper->finishModule();
  if (dbHelper)
    {
      dbHelper->finishModule(M);
      delete dbHelper;
      dbHelper = NULL;
    }
    */
  return false;
}


bool SetProfiler::runOnModule(Module &M)
{
  bool ret = false;

  errs() << "RUN ON MODULE\n";
  /*
  if(dumpIRfile != "") {
     std::error_code EC;
     raw_fd_ostream out(dumpIRfile, EC, sys::fs::F_Text);
     if (EC) {
        std::cerr<<"Unable to open file : "<<dumpIRfile<<"Error no.="
								 <<EC.value()<<"Error msg="<<EC.message()<<std::endl;
     } else {
        M.print(out, nullptr);
        out.close();
        if (out.has_error())
					std::cerr<<"Unable to write to file"<<dumpIRfile<<std::endl;
     }
  }
  */

  errs() << "RUNNING\n";
  M.print(llvm::errs(), nullptr);
  // Is there a better way??!!
  std::vector<Function*> list;
  for(Module::iterator it=M.begin(); it!=M.end(); it++)
      list.push_back(&*it);

  for(size_t i=0,size=list.size(); i<size; i++) {
      Function *F = list[i];

      // Do not profile a cloned function that's already been
      // instrumented
      // This check is only needed in the case of sampling
      if (SampleRate > 1 && fList.find(F)!=fList.end())
      	continue;

      if (F->begin()!=F->end()) {
    	  AliasAnalysis *aliasAnalysis =
											&(getAnalysis<AAResultsWrapperPass>(*F).getAAResults());
    	  errs() << "ALIAS ANALYSIS COMPLETE: " << aliasAnalysis << "\n";
	  		ret = runOnFunction(F) || ret;
			}
  }
  return ret;
}
