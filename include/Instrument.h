//===- Instrument.h - Set Profiler for data dependence profiling ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is the intrumenter class. This instruments the queries in the code
// for profiler to track and analyse.
//
//===----------------------------------------------------------------------===//

#ifndef NEW_PROJECT_INSTRUMENT_H
#define NEW_PROJECT_INSTRUMENT_H

//#include "SetAssign.h"
#include "llvm/IR/Module.h"
//#include "Signature.h"
//#include "PerfectSet.h"
#include "ProfileDBHelper.h"
#include "BuildSignature.h"
#include "GenerateQueries.h"
#include <set>

#define USEBUILDER

namespace llvm {

  typedef std::set<unsigned int> IntSet;
  typedef std::set<Instruction*> InstrSet;
  typedef std::vector<ReturnInst*> RetInstVecTy;

 class SetInstrument {
 protected:
   ddp::Queries &AQ;
   Function &F;
   FunctionRegion Region;
   LLVMContext& Context;
   Module& M;
   Instruction* pos;
   ProfileDBHelper& DBHelper;

   GlobalVariable *fCount;

   typedef std::pair<Instruction*, unsigned long long> RefPair;

   class RefPairCompare {
   public:
     bool operator () (const RefPair &P1, const RefPair &P2) const {
       if ( P1.first < P2.first)
	 return true;
       else if ( P1.first==P2.first && P1.second < P2.second)
	 return true;
       else
	 return false;
     }
   };

   std::map<RefPair, AllocaInst*, RefPairCompare> queryVars;

   InstrSet inserted;
   
   //Map signature id to its population counter.
   std::map<unsigned long long, GlobalVariable *> populationCounterMap; 

 protected: // From SignatureInstrument
  typedef std::map<unsigned int, AbstractSetInstrumentHelper<SImple>*> SignMap;
  typedef std::map<unsigned int, Instruction*> InstMap;
  typedef std::map<Instruction*, InstMap> Int2InstMap;

  SignMap ProfileSets;
  Int2InstMap checked;

   std::map<unsigned int,unsigned int> StructPsetMap;
   //void insertDumpInstrumentation(Pass *P, Function &F);
   AllocaInst* allocateVariableForQuery(ddp::Query &Q, RetInstVecTy &Rets);
   Value* getPointerOperand(Instruction* I);
   void instrNoAliasQueries(RetInstVecTy &Rets);
   //void instrMustAliasQueries(std::vector<ReturnInst*> &Rets);

   void getReturnInsts(std::vector<ReturnInst*> &Rets);
   virtual void instrExits(RetInstVecTy &Rets);
   void clearChecked();

   static int traceStructSize(Value *val);
 public:
   SetInstrument(ddp::Queries&, Function&, ProfileDBHelper&);

   bool instrument(Pass *P, Function &F);

   virtual void InsertValue(Instruction *I, ddp::Query &Q);
   virtual Instruction* MembershipCheck(Instruction* I, ddp::Query &Q,
                                                    AllocaInst* QueryVar);
   virtual void finalize(RetInstVecTy&);

};

}

#endif
