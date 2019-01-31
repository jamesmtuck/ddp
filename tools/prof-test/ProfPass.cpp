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

#define DEBUG_TYPE "profpassinfo"

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
#include "db/ProfileDBHelper.h"

namespace llvm {
class ProfPass : public ModulePass {
  

  //std::vector< Constant* > ddp_init; 

  ProfileDBHelper *dbHelper;

  //GlobalVariable *array;

  //void insertDtorsCall(Module &M);

  /*
  GlobalVariable *getGlobal(Module &M) {
    Twine t("ddp");
    Type *int32 = IntegerType::get(getGlobalContext(),32);
    return new GlobalVariable(M,int32,
			      false,llvm::GlobalValue::PrivateLinkage,
			      ConstantInt::get(int32,0),t);
			      }*/

public:
  ProfPass() : ModulePass(ID) {
    //    array=NULL;
  }
  static char ID;
  bool runOnModule(Module &M);
  bool runOnFunction(Function &F);
};

void initializeProfPassPass(PassRegistry&);
}

using namespace llvm;

//STATISTIC(Test1, "Number of Dependences we speculated on (double counted)");
//STATISTIC(Test2, "Total number of queries to AA");

char ProfPass::ID=0;

INITIALIZE_PASS_BEGIN(ProfPass, "ProfPass",
                "Profiler Test", false, true)

INITIALIZE_PASS_DEPENDENCY(DominatorTree)

INITIALIZE_PASS_END(ProfPass, "ProfPass",
                "Profiler Test", false, true)



bool ProfPass::runOnModule(Module &M) { 

  dbHelper = new ProfileDBHelper(M,"proftest");

    Module::iterator I;
    for(I=M.begin(); I!=M.end(); I++) {
      runOnFunction(*I);
    }
    

    /*  StructType *mystruct = M.getTypeByName("ddpref");
  Twine t("ddp_refids");
  
  ArrayRef<Constant*> aref(ddp_init);
  Constant *ca = ConstantArray::get(ArrayType::get(mystruct,ddp_init.size()),aref);
  
  array = 
    new GlobalVariable(M,ArrayType::get(mystruct,ddp_init.size()),
		       false,llvm::GlobalValue::PrivateLinkage,
		       ca,t);
  
		       insertDtorsCall(M);*/

    dbHelper->finishModule(M);
    delete dbHelper;
    dbHelper = NULL;
}


bool ProfPass::runOnFunction(Function &F) {
  Module &M = *F.getParent();
  //LLVMContext *Context = &getGlobalContext();

  Function::iterator bit;

  Type *Int32 = IntegerType::get(getGlobalContext(),32);

  /*  std::map<uint64_t, GlobalVariable*> globalMap;

  StructType *mystruct = M.getTypeByName("ddpref");
  if (mystruct==NULL) {
    Type *types[2];
    types[0] = Int32;
    types[1] = PointerType::get(Int32,0);
    ArrayRef<Type*> typeArray(types,2);
    mystruct = StructType::create(typeArray,"ddpref");
    } */   


  for(bit=F.begin(); bit!=F.end(); bit++) {

    GlobalVariable *gv = dbHelper->nextCounter();
    uint64_t ref = dbHelper->getRefId();

    BasicBlock *BB = &*bit;
    Instruction *First = BB->getFirstNonPHI();    
    IRBuilder<> builder(First);

    Value *mdVals[2];
    mdVals[0] = builder.CreateGlobalStringPtr("profpass");
    mdVals[1] = ConstantInt::get(Int32,(int)ref);
    MDNode *md = MDNode::get(getGlobalContext(),ArrayRef<Value*>(mdVals));
    First->setMetadata("ddp",md);

    LoadInst *LI = builder.CreateLoad(gv,"ddp.ld");
    Value *add = builder.CreateAdd(LI,builder.getInt32(1),"ddp.inc");
    
    //StoreInst *SI = 
    builder.CreateStore(add,gv);
  }

  dbHelper->finishFunction(F);

  return false;
}

/********
void ProfPass::insertDtorsCall(Module &M) {

  IRBuilder<> IRB(M.getContext());

  StructType *mystruct = M.getTypeByName("ddpref");

  Function::iterator bit;
  FunctionType *FnTy = FunctionType::get(IRB.getVoidTy(),false);
  
  // Function creating
  Function *prof_finish = 
    prof_finish=Function::Create(FnTy,llvm::GlobalValue::InternalLinkage,"ddp_internal_finish",&M);


  // Callee
  Type *args[3];
  args[0] = PointerType::get(IRB.getInt8Ty(),0); //IRB.CreateGlobalString("/location/of/ddp.db");
  args[1] = PointerType::get(mystruct,0);
  args[2] = IRB.getInt32Ty();
  ArrayRef<Type*> argRef(args);

  FunctionType *FnTyCallee = FunctionType::get(IRB.getVoidTy(),args,false);
  Function *callee = 
    Function::Create(FnTyCallee,llvm::GlobalValue::ExternalLinkage,"ddp_module_finish",&M);


  
  Twine entry="entry";
  BasicBlock *BB = BasicBlock::Create(M.getContext(),entry,prof_finish);

  IRB.SetInsertPoint(BB);
  
  int size = ( (ArrayType*)array->getType()->getElementType())->getNumElements();
  ConstantInt *sz = IRB.getInt32(size);
  
  Value *gepIndex[2];
  gepIndex[0] = IRB.getInt32(0);
  gepIndex[1] = IRB.getInt32(0);
  Value *gep = IRB.CreateGEP(array,ArrayRef<Value*>(gepIndex));
    //     PointerType::get(mystruct,0),0)

  Value *str = IRB.CreateGlobalString("/location/of/ddp.db");

  Value *gepIndex2[2];
  gepIndex2[0] = IRB.getInt32(0);
  gepIndex2[1] = IRB.getInt32(0);
  Value *gep1 = IRB.CreateGEP(str,ArrayRef<Value*>(gepIndex2));
  
   //CallInst *ci = 
  IRB.CreateCall3(callee,gep1,gep,sz);
  
  IRB.CreateRetVoid();

  llvm::appendToGlobalDtors(M,prof_finish,0);
}
*/
