//===- SetAssign.cpp - Set Profiler for data dependence profiling -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This assigns the queries to the sets for signature-based DDP to analyze.
//
//===----------------------------------------------------------------------===//
//
#define DEBUG_TYPE "setassign"

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
#include "SetAssign.h"
#include <vector>

STATISTIC(NumSets, "Number of assigned profile sets");
STATISTIC(NumQueries, "Number of query sets");
//STATISTIC(SetOverflow, "Set overflow detected");

static std::set<unsigned long long> setsUsed;

using namespace llvm;

//static cl::opt<int>
//MaxSetSize("max-set-size", cl::Hidden,
//			 cl::desc("Limit static population to this value"), cl::init(-1));

int MaxSetSize = -1;

ddp::AssignQueries::AssignQueries() {
  maxSetSize = MaxSetSize;
}

void ddp::AssignQueries::assignSets(Queries &AllQ) {
  query_iterator it;
  int count=0;
  int signCnt=0;
  printf("Normal assignSets!\n");
  if (AllQ.size() > 0)
    NumSets++;

  for(it = AllQ.begin(); it!=AllQ.end(); it++) {
      Query q = *it;
      Instruction *I = (*it).rhs;
      NumQueries++;
      if (SetAssignments.find(I)==SetAssignments.end()) {
      	if (getMaxSetSize() == -1) {
      	    SetAssignments[I] = q.pset = signCnt;
      	  } else {
      	    if (count >= getMaxSetSize()) {
      		      count = 1;
      		      SetAssignments[I] = q.pset = ++signCnt;
      		      NumSets++;
      	    } else {
      		      count++;
      		      SetAssignments[I] = q.pset = signCnt;
      	      }
	       }
      } else {
	    // use same set as before
	       q.pset = SetAssignments[I];
      }
      insertQuery(q);
    }
}

void ddp::LinearAssignQueries::assignSets(Queries &AllQ)
{
  query_iterator it, it2;
  int count=0;
  int signCnt=0;

  //if (AllQ.size() > 0)
  //  NumSets++;

  for(it = AllQ.begin(); it!=AllQ.end(); it++) {
  //optimistically give all stores a unique set
     Query q = *it;
     Instruction *I = (*it).rhs;
     if (SetAssignments.find(I)==SetAssignments.end()) {
       SetAssignments[I] = q.pset = ++signCnt;
     } else {
       q.pset = SetAssignments[I];
     }
    }

  int max = 0;
  for(it = AllQ.begin(); it!=AllQ.end(); it++) {
  //make stores that share a load use the same set
      Query q = *it;
      Instruction *I = (*it).rhs;
      for(it2 = it+1; it2!=AllQ.end(); it2++) {
    	  Query q2 = *it2;
    	  Instruction *I2 = (*it2).rhs;
    	  if (q.lhs == q2.lhs) {
    	    q2.pset = q.pset;
    	    SetAssignments[I2] = q.pset;
    	  }
    	  if (SetAssignments[I2] != q2.pset) {
    	    q2.pset = SetAssignments[I2];
    	  }
	     }
       if (q.pset > max) {
      	 max = q.pset;
      	NumSets++;
       }
       insertQuery(q);
    }
}
