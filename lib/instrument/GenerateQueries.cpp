//===- QueryInterface.cpp - Interface for recording queries for DDP ------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains analysis for generating queries for signature-based
// DDP to look out for.
//
//===----------------------------------------------------------------------===//
//
#define DEBUG_TYPE "generatequeries"

#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/ADT/SCCIterator.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Support/GenericDomTree.h"
#include "ProfilerDatabase.h"
#include "EdgeProfiler.h"
#include "GenerateQueries.h"
#include <vector>
#include <string>
#include <sstream>
#include <system_error>

STATISTIC(NoMayAliasQueries, "Number of May Alias Queries recorded");
STATISTIC(NoLoads, "Number of stores recorded");
STATISTIC(NoStores, "Number of laods recorded");

using namespace llvm;
using namespace ddp;

static cl::list<unsigned int> BacktraceRefid("backtrace-refid",cl::CommaSeparated,
                          cl::desc("Create a backtrace for the given refid(s)"),
                          cl::ZeroOrMore,cl::Hidden);

static
cl::opt<bool> RefidSkippingHeuristic("heuristically-skip-refids", cl::Hidden,
                    cl::desc("Print warnings for refids that should be discarded. \
                      Implement actual discarding later."), cl::init(false));

template <class Inst>
void getInstructions(Function &F, std::vector<Inst*> &v) {
  for(Function::iterator fit=F.begin(),fend=F.end(); fit!=fend; fit++) {
      BasicBlock &BB = *fit;
      for(BasicBlock::iterator bit=BB.begin(), bend=BB.end(); bit!=bend; bit++) {
	       Instruction *I = &*bit;
	        if(Inst *i = dyn_cast<Inst>(I))
            v.push_back(i);
	    }
  }
}

template <class Inst>
void getInstructions(const std::vector<BasicBlock*> &bbs, std::vector<Inst*> &v) {
  for(size_t i=0; i<bbs.size(); i++)
    {
      BasicBlock *BB = bbs[i];
      for(BasicBlock::iterator bit=BB->begin(), bend=BB->end(); bit!=bend; bit++)
	{
	  Instruction *I = &*bit;

	  if(Inst *i = dyn_cast<Inst>(I))
	    {
	      v.push_back(i);
	    }
	}
    }
}

template <class Inst>
void getInstructions(const std::vector<BasicBlock*> &bbs,
                                      std::vector<Instruction*> &v) {
  for(size_t i=0; i<bbs.size(); i++) {
      BasicBlock *BB = bbs[i];
      for(BasicBlock::iterator bit=BB->begin(), bend=BB->end(); bit!=bend; bit++) {
	       Instruction *I = &*bit;
	        if(Inst *i = dyn_cast<Inst>(I))
	         v.push_back(i);
	    }
    }
}

static bool comesAfter(const DominatorTreeBase<BasicBlock, false> *DT,
					   const DominatorTreeBase<BasicBlock, true> *PDT,
					   const BasicBlock *a, const BasicBlock *b) {
	DomTreeNodeBase<BasicBlock> *bnode = PDT->getNode(b);
	while(bnode) {
		if( DT->dominates(bnode->getBlock(),a) )
			return true;
		bnode = bnode->getIDom();
	}
	return false;
}

void MayAliasQueries::run(Function &F, AAResultsWrapperPass &AA,
                                       ProfileDBHelper &dbHelper) {
#define AliasResultScope  AliasResult
  std::vector<LoadInst*> nl_loads;
  std::vector<StoreInst*> nl_stores;
  std::vector< std::vector<BasicBlock*> > bbs;
  for(scc_iterator<Function*> it = scc_begin(&F); !it.isAtEnd(); ++it) {
      if (it.hasLoop()) {
	// perform cross product of loads and stores
	     const std::vector<BasicBlock*> &scc = *it;
	      std::vector<LoadInst*> loads;
	      getInstructions<LoadInst>(scc,loads);
	      std::vector<StoreInst*> stores;
	      getInstructions<StoreInst>(scc,stores);
      	NoStores += stores.size();
      	NoLoads += loads.size();

	for(size_t i=0,lsize=loads.size(); i<lsize; i++) {
	    LoadInst *LI = loads[i];
        const MemoryLocation loadLoc = MemoryLocation::get(LI);
        //AliasAnalysis::Location loadLoc = AA.getLocation(LI);
	    for(size_t j=0,ssize=stores.size(); j<ssize; j++)
	      {
		StoreInst *SI = stores[j];
        const MemoryLocation storeLoc = MemoryLocation::get(SI);
		//AliasAnalysis::Location storeLoc = AA.getLocation(SI);
		//AliasResult res = AA.alias(loadLoc,storeLoc);
        AliasResult res = AA.getAAResults().alias(loadLoc,storeLoc);
		switch (res) {
		case AliasResultScope::MayAlias:
		  NoMayAliasQueries++;
		  errs() << "NUM NO MAY ALIAS QUERIES: " << NoMayAliasQueries << "\n";
		  insertQuery(LI,SI,dbHelper.incRefId(),dbHelper.getFileId());
		  break;
		case AliasResultScope::PartialAlias:
		  break;
		case AliasResultScope::MustAlias:
		  break;
		case AliasResultScope::NoAlias:
		  break;
		}
	      }
	  }
      }
      bbs.push_back(*it);
    }

  DominatorTreeBase<BasicBlock, false> *DT =
                            new DominatorTreeBase<BasicBlock, false>();
  DominatorTreeBase<BasicBlock, true> *PDT =
                            new DominatorTreeBase<BasicBlock, true>();
  DT->recalculate(F);
  PDT->recalculate(F);

  for (size_t i=0; i<bbs.size(); i++)
    for(size_t j=i+1; j<bbs.size(); j++)
      {
	std::vector<BasicBlock*> &bi = bbs[i];
	std::vector<BasicBlock*> &bj = bbs[j];

	BasicBlock *bbi = bi[0];
	BasicBlock *bbj = bj[0];

	std::vector<BasicBlock*> *before, *after;

	if ( comesAfter(DT,PDT,bbi,bbj) ) {
	  before = &bj;
	  after = &bi;
	} else if (comesAfter(DT,PDT,bbj,bbi)) {
	  before = &bi;
	  after = &bj;
	} else {
	  continue;
	}

	std::vector<Instruction*> befores;
	//getInstructions<LoadInst>(*before,befores);
	getInstructions<StoreInst>(*before,befores);

	std::vector<Instruction*> afters;
	getInstructions<LoadInst>(*after,afters);
	//getInstructions<StoreInst>(*after,afters);

	for(size_t i=0,lsize=afters.size(); i<lsize; i++)
	  {
	    Instruction *LI = afters[i];
	    const MemoryLocation loadLoc = MemoryLocation::get(LI);
	    //AliasAnalysis::Location loadLoc = AA.getLocation(LI);
	    for(size_t j=0,ssize=befores.size(); j<ssize; j++)
	      {
		Instruction *SI = befores[j];
		 const MemoryLocation storeLoc = MemoryLocation::get(SI);
		//AliasAnalysis::Location storeLoc = AA.getLocation(SI);
		//AliasResult res = AA.alias(LI,SI);
		AliasResult res = AA.getAAResults().alias(loadLoc,storeLoc);
		switch (res) {
		case AliasResultScope::MayAlias:
		  NoMayAliasQueries++;
		  errs() << "NUM MAY ALIAS QUERIES: " << NoMayAliasQueries << "\n";
		  insertQuery(LI,SI,dbHelper.incRefId(),dbHelper.getFileId());
		  break;
		case AliasResultScope::PartialAlias:
		  break;
		case AliasResultScope::MustAlias:
		  break;
		case AliasResultScope::NoAlias:
		  break;
		}
	      }
	  }
      }



  delete DT;
  delete PDT;
}

GetElementPtrInst* MayAliasQueries::traceToStructGEP(Value *val) {
   Value *startVal = val;
   Instruction *inst;
   std::map<PHINode*,int> phiSelector;
   while(1) {
      if(!isa<Instruction>(val)){
         // FIXME: Should we test for constant GEP ?
         return nullptr;
      } else {
         inst = cast<Instruction>(val);
         if(inst->getOpcode() == Instruction::GetElementPtr) {
            val = inst->getOperand(0);
            Type* ty = val->getType();
            if(ty->isPointerTy() && ty->getPointerElementType()->isStructTy())
              return cast<GetElementPtrInst>(inst);
            else
              return nullptr;
         } else if(  inst->getOpcode() == Instruction::BitCast) {
            val = inst->getOperand(0);
         } else if(  inst->getOpcode() == Instruction::PHI) {
         //FIXME: Should we try to trace through phi at all? If one side is
         // a GEP vs. other is a load or similar, maybe it is a bad idea.
            PHINode *phi = cast<PHINode>(inst);
            if(phiSelector.find(phi) == phiSelector.end()) {
               phiSelector[phi] = 0;
            } else {
               phiSelector[phi]++;
               if(phiSelector[phi] >= phi->getNumIncomingValues())
                return nullptr;
            }
            val = phi->getIncomingValue(phiSelector[phi]);
         } else {
            return nullptr;
         }
      }
   }
}

Value* MayAliasQueries::traceToAddressSource(Value *val,
                                             std::set<PHINode*> &seenPhi) {
// This function will trace the given value through instructions like GEP,
// phi and bitcasts to it's root memory source like an alloca or a function
// argument, a return from call or a function argument.
   Value *startVal = val;
   Instruction *inst;
   while(1) {
      if(!isa<Instruction>(val)){
         // FIXME: Should we test for constant GEP ?
         return val;
      } else {
         inst = cast<Instruction>(val);
         unsigned op = inst->getOpcode();
         if(Instruction::GetElementPtr == op) {
            val = inst->getOperand(0);
         } else if(Instruction::BitCast == op) {
            val = inst->getOperand(0);
         } else if(Instruction::PHI == op) {
          // Should we try to trace through phi at all? This is too complex a
          // test for a simple heuristic. Disable if it slows down code.
            PHINode *phi = cast<PHINode>(inst);
            if(seenPhi.find(phi) != seenPhi.end())
              return nullptr; // Break recursion in an infinite loop.
                              // Since this phi traced back to itself, there's
                              // nothing but GEPs and Bitcasts, which are consts,
                              // so return true.
            seenPhi.insert(phi);
            Value *ret = nullptr;
            for(int i=0; i<phi->getNumIncomingValues(); i++){
               Value *tmp;
               if(nullptr != (tmp = traceToAddressSource(
                                        phi->getIncomingValue(i), seenPhi))) {
                  if(ret == nullptr) {
                    ret = tmp;
                  } else {
                     return phi;
                  }
               }
            }
            return ret;
         } else {
            return val;
         }
      }
   }
}

bool MayAliasQueries::addressNeverTaken(Instruction *inst,
                                        std::set<PHINode*> &seenPhi) {
   for(auto user = inst->user_begin(); user != inst->user_end(); user++) {
      Instruction *userInst = cast<Instruction>(*user);
      unsigned op = userInst->getOpcode();
      if(Instruction::Store == op) {
         if(userInst->getOperand(0) == cast<Value>(inst)) return false;
         else continue;
      } else if(Instruction::PtrToInt == op) {
         return false;
      } else if(Instruction::Load == op) {
         continue;
      } else if(  Instruction::GetElementPtr == op   ||
            Instruction::BitCast == op ){
         if(addressNeverTaken(userInst,seenPhi)) continue;
         else return false;
      } else if(Instruction::PHI == op){
         PHINode *phi = cast<PHINode>(userInst);
         if(seenPhi.find(phi) != seenPhi.end()) continue;
         seenPhi.insert(phi);
         if(addressNeverTaken(userInst,seenPhi)) continue;
         else return false;
      } else if(Instruction::AtomicCmpXchg == op ||
            Instruction::AtomicRMW == op ||
            Instruction::Fence == op) {
         errs() << "DDP WARN: Not sure how to handle instruction: "
                                                << *userInst << "\n";
         return false;
      } else if(Instruction::Call == op) {
         CallInst *ci = cast<CallInst>(userInst);
         if(ci->onlyReadsMemory()) continue;
         else return false;
      }
   }
   return true;
}

bool MayAliasQueries::isConstAddr(Value *val, std::set<PHINode*> &seenPhi) {
   Value *startVal = val;
   Instruction *inst;
   while(1) {
      if( isa<Constant>(val)) {
         if( isa<GlobalVariable>(val) ) {
            return cast<GlobalVariable>(val)->isConstant();
         } else if(isa<ConstantExpr>(val)) {
            ConstantExpr * constExprVal = cast<ConstantExpr>(val);
            if(constExprVal->getOpcode() == Instruction::GetElementPtr) {
               val = constExprVal->getOperand(0);
            } else if(constExprVal->getOpcode() == Instruction::BitCast) {
               val = constExprVal->getOperand(0);
            } else if(constExprVal->getOpcode() == Instruction::IntToPtr) {
            // We don't really know much about the address, even if it is a
            // const int. Probably it is constant, probably it is not.
               return false;
            } else {
               errs() << "DDP WARN: Next heuristic might not be reliable. \
                          Unknown ConstantExpr: " << *constExprVal << "\n";
               return true;
            }
         } else {
            return true;
         }
      } else if (isa<Instruction>(val)) {
         inst = cast<Instruction>(val);
         if(inst->getOpcode() == Instruction::GetElementPtr) {
            val = inst->getOperand(0);
         } else if(  inst->getOpcode() == Instruction::BitCast) {
            val = inst->getOperand(0);
         } else if(  inst->getOpcode() == Instruction::PHI) {
         //Should we try to trace through phi at all? This is too complex a test
         // for a simple heuristic. Disable if it slows down code.
            PHINode *phi = cast<PHINode>(inst);
            if(seenPhi.find(phi) != seenPhi.end())
              return true; // Break recursion in an infinite loop. Since this phi
                          // traced back to itself, there's nothing but GEPs and
                          // Bitcasts, which are consts, so return true.
            seenPhi.insert(phi);
            bool ret = true;
            for(int i=0; i<phi->getNumIncomingValues(); i++){
               ret = ret && isConstAddr(phi->getIncomingValue(i), seenPhi);
            }
            return ret;
         } else {
            return false;
         }
      } else  {
         return false;
      }
   }
}

void MayAliasQueries::insertQuery(Instruction *lhs, Instruction *rhs,
                                  unsigned long long id/*=0*/,
                                  unsigned long long fileid/*=0*/,
                                  unsigned long long pset/*=0*/) {
   //lhs = Load Instruction
   //rhs = Store Instruction
   //id = refid

   //Print if type is matching
   //if(RefidSkippingHeuristic.getValue()) {
//      if(lhs->getType() != rhs->getOperand(0)->getType()) {
//         errs()<<"DDP WARN: Type Mismatch FileID="<<fileid<<" ID="<<id<<" LoadType="<<*lhs->getType()<<" StoreType="<<*rhs->getOperand(0)->getType()<<"\n";
//      }

      //Detect mismatching GEP ==> Compare base pointer type and constant offsets. Display if mismatching.
      GetElementPtrInst *LoadGEP = traceToStructGEP(lhs->getOperand(0));
      GetElementPtrInst *StoreGEP = traceToStructGEP(rhs->getOperand(1));
      if(LoadGEP != nullptr && StoreGEP != nullptr) {
         // This is the code to detect if structures are unions, guessing by the name. But somehow the getName function is not returning proper value.
         //StructType *loadStructType = cast<StructType>(LoadGEP->getOperand(0)->getType());
         //StructType *storeStructType = cast<StructType>(StoreGEP->getOperand(0)->getType());
         //bool unionFound = false;
         //if( (!loadStructType->isLiteral()) && loadStructType->hasName()) {
         //   //errs()<<"##### Type name = size="<<loadStructType->getName().size()<<" val="<<loadStructType->getName()<<"\n";
         //   unionFound = unionFound && (loadStructType->getName().find("union.") != StringRef::npos);
         //}
         //if( (!storeStructType->isLiteral()) && storeStructType->hasName()) {
         //   //errs()<<"##### Type name = size="<<storeStructType->getName().size()<<" val="<<storeStructType->getName()<<"\n";
         //   unionFound = unionFound && (storeStructType->getName().find("union.") != StringRef::npos);
         //}
         if(LoadGEP->getOperand(0)->getType() != StoreGEP->getOperand(0)->getType()) {
            errs()<<"DDP WARN: GEP Base Mismatch FileID="<<fileid<<" ID="<<id<<" LoadGEP="<<*LoadGEP<<" StoreGEP="<<*StoreGEP<<"\n";
            return;
         } else {
            bool constIndices=true,mismatch = false;
            int LoadNumOperands = LoadGEP->getNumOperands();
            int StoreNumOperands = StoreGEP->getNumOperands();
            int smallerNumOperands = LoadNumOperands < StoreNumOperands ? LoadNumOperands : StoreNumOperands;
            //Skip the 0th operand since it is the base, whose type we've checked. Skip 1st operand as it is the stride into an array of such structs, and needn't be constant or match.
            for(int i=2; i<smallerNumOperands && constIndices; i++){
                  Constant *Lidx = dyn_cast<Constant>(LoadGEP->getOperand(i));
                  Constant *Sidx = dyn_cast<Constant>(StoreGEP->getOperand(i));
                  if(Lidx == nullptr || Sidx == nullptr) constIndices = false;
                  else if(Lidx != Sidx) mismatch=true;
            }
            //Only one of the below two loops will actually run, that too only if numOperands don't match.
            //The logic here is, if there are excess operands but all of them are 0, then the address is going to be the same. Otherwise it will definitely be different.
            for(int i=smallerNumOperands; i<StoreNumOperands && constIndices; i++) {
               Constant *idx = dyn_cast<Constant>(StoreGEP->getOperand(i));
               if(idx == nullptr) constIndices = false;
               else if(!idx->isZeroValue()) mismatch = true;
            }
            for(int i=smallerNumOperands; i<LoadNumOperands && constIndices; i++) {
               Constant *idx = dyn_cast<Constant>(LoadGEP->getOperand(i));
               if(idx == nullptr) constIndices = false;
               else if(!idx->isZeroValue()) mismatch = true;
            }

            //If the indices are constant till now and a mismatch between them
            // has been detected, we should heuristically skip this Load-Store pair.
            if(constIndices && mismatch){
               errs()<<"DDP WARN: GEP Indices Mismatch FileID="<<fileid<<" ID="
                  <<id<<" LoadGEP="<<*LoadGEP<<"      StoreGEP="<<*StoreGEP<<"\n";
               return;
            }
         }
      }

      {
      //Check for constness of load address. Then it can never conflict with a store.
         std::set<PHINode*> seenPhi;
         if( isConstAddr(lhs->getOperand(0),seenPhi) ) {
            errs() << "DDP WARN: Constant Load Address FileID="<< fileid
            	   <<" ID=" << id << " LoadAddr=" << *lhs->getOperand(0) << "\n";
            return;
         }
      }

      {
       //Check if one of the address is coming from alloca, while other is
       // through a GEP. These can never clash as an explicit alloca won't be
       // inside anything.
         Value *loadAddr = lhs->getOperand(0);
         Value *storeAddr = rhs->getOperand(1);
         if(isa<BitCastInst>(loadAddr)) {
           loadAddr = cast<BitCastInst>(loadAddr)->getOperand(0);
         }
         if(isa<BitCastInst>(storeAddr)) {
           storeAddr = cast<BitCastInst>(storeAddr)->getOperand(0);
         }
         bool nonZeroConstIdx = false;
         bool nonConstIdx = false;
         bool nonAggAlloca = false;
         if(isa<GetElementPtrInst>(loadAddr) && isa<AllocaInst>(storeAddr)) {
            AllocaInst *aloc = cast<AllocaInst>(storeAddr);
            GetElementPtrInst *gep = cast<GetElementPtrInst>(loadAddr);
            for(int i=1; i<gep->getNumOperands(); i++){
               Constant *idx = dyn_cast<Constant>(gep->getOperand(i));
               if(idx && !idx->isZeroValue()) nonZeroConstIdx = true;
               if(idx == nullptr) nonConstIdx = true;
            }
            if(! aloc->getAllocatedType()->isAggregateType()) nonAggAlloca = true;
         } else if(isa<GetElementPtrInst>(storeAddr) && isa<AllocaInst>(loadAddr)) {
            AllocaInst *aloc = cast<AllocaInst>(loadAddr);
            GetElementPtrInst *gep = cast<GetElementPtrInst>(storeAddr);
            for(int i=1; i<gep->getNumOperands(); i++){
               Constant *idx = dyn_cast<Constant>(gep->getOperand(i));
               if(idx && !idx->isZeroValue()) nonZeroConstIdx = true;
               if(idx == nullptr) nonConstIdx = true;
            }
            if(! aloc->getAllocatedType()->isAggregateType()) nonAggAlloca = true;
         }
         if(nonAggAlloca || nonZeroConstIdx) {
            errs()<<"DDP WARN: Alloca vs GEP FileID="<<fileid<<" ID="<<id<<" LoadAddr="<<*loadAddr<<"      StoreAddr="<<*storeAddr<<"\n";
            return;
         } else if ( nonConstIdx ) {
            errs()<<"DDP WARN: Alloca vs GEP with nonConstIndices FileID="<<fileid<<" ID="<<id<<" LoadAddr="<<*loadAddr<<"      StoreAddr="<<*storeAddr<<"\n";
         }
      }

      {
         Value *loadAddr = lhs->getOperand(0);
         Value *storeAddr = rhs->getOperand(1);
         std::set<PHINode*> seenPhi;
         Value *loadAddrSource = traceToAddressSource(loadAddr,seenPhi);
         seenPhi.clear();
         Value *storeAddrSource = traceToAddressSource(storeAddr,seenPhi);
         if(loadAddrSource != storeAddrSource) {
            AllocaInst *allocaSource = nullptr;
            Value *otherAddr = nullptr;
            Value *otherSource = nullptr;
            if(loadAddrSource && isa<AllocaInst>(loadAddrSource)) {
               allocaSource = cast<AllocaInst>(loadAddrSource);
               otherAddr = storeAddr;
               otherSource = storeAddrSource;
            } else if (storeAddrSource && isa<AllocaInst>(storeAddrSource)) {
               allocaSource = cast<AllocaInst>(storeAddrSource);
               otherAddr = loadAddr;
               otherSource = loadAddrSource;
            }
            if(allocaSource) {
               seenPhi.clear();
               if(addressNeverTaken(allocaSource,seenPhi)) {
                  errs() << "DDP WARN: Traced Alloca address never taken FileID="
                         << fileid << " ID=" <<id<< " Alloca="
                         << *allocaSource <<"\n";
                  return;
               } else if (isa<Argument>(otherSource)) {
                  errs() << "DDP WARN: Traced alloca won't conflict with traced"
                            " argument FileID=" << fileid << " ID=" << id
                          << " Alloca=" << *allocaSource << " Argument="
                          << *otherSource << "\n";
                  return;
               }
            }
         }
      }
   //}

   //Create backtraces as per user input.
   //for(const auto &refid : BacktraceRefid) {
     // if(id== refid) {
       //  printBacktrace("backtrace_refid" + std::to_string(id) + "_store.txt",rhs->getOperand(1));
        // printBacktrace("backtrace_refid" + std::to_string(id) + "_load.txt",lhs->getOperand(0));
     // }
  // }

   //Now actually insert the query
     errs() << "INSERT QUERIES\n";
   Queries::insertQuery(lhs,rhs,id,pset);
}

void ddp::printBacktrace(const std::string &filename, Value *val) {
   std::error_code EC;

   raw_fd_ostream out(filename,EC,sys::fs::F_Text);
   if(EC){
      std::cerr << "Unable to open file for backtracing instruction : "
                << filename << "Error no.=" <<EC.value() << "Error msg="
                << EC.message() << std::endl;
      return;
   }
   Instruction *inst;
   while(1) {
      if(!isa<Instruction>(val)){
         out<<*val<<"\n";
         break;
      } else {
         inst = cast<Instruction>(val);
         out<<inst->getParent()->getName()<<":   "<<*inst<<"\n";
      }
      if(  inst->getOpcode() == Instruction::Load ||
           inst->getOpcode() == Instruction::GetElementPtr ||
           inst->getOpcode() == Instruction::BitCast ){
         val = inst->getOperand(0);
      } else {
         break;
      }
   }
   if(isa<Instruction>(val)){
      out << "Function Name : "
          << cast<Instruction>(val)->getParent()->getParent()->getName() << "\n";
   }
   out.close();
   if (out.has_error())
    std::cerr << "Unable to write to file" << filename << std::endl;
}
