//===------------------------ BuildSignatureAPI.cpp -----------------------===//
//
//
//===----------------------------------------------------------------------===//
//
//
//
//
//
//===----------------------------------------------------------------------===//
//

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
#include "SetInstrumentFactory.h"
#include "BuildSignature.h"
#include <sstream>

using namespace llvm;

BuildSignatureAPI::BuildSignatureAPI(SImple &aBS) : BS(aBS) {}

void BuildSignatureAPI::CreateAllocateFn(Module *M) {
  std::stringstream ss;
  ss << BS.getName() << "AllocateFn";
  FunctionType *FnTy = FunctionType::get(BS.getSignatureType(), false);
  Function *F = Function::Create(
                        FnTy, GlobalValue::ExternalLinkage, ss.str(), M);
  BasicBlock *BB = BasicBlock::Create(M->getContext(), "entry",  F,  NULL);
  IRBuilder<> Builder(BB);
  Value *ret = (GlobalVariable*)BS.allocateGlobal(Builder);
  //M->getGlobalList().push_back(ret);
  PointerType *pt = dyn_cast<PointerType>(ret->getType());
  if(pt->getElementType()->isIntegerTy()) {
      Builder.CreateRet( ret );
  } else {
      Value *ind[2];
      ind[0] = Builder.getInt32(0);
      ind[1] = Builder.getInt32(0);
      ArrayRef<Value*> indices(ind);
      Builder.CreateRet(Builder.CreateGEP(ret,indices));
  }
}

void BuildSignatureAPI::CreateInsertFn(Module *M) {
  std::stringstream ss;
  ss << BS.getName() << "InsertFn";

  IRBuilder<> Builder(M->getContext());
  Type* arr[2];
  arr[0] = BS.getSignatureType();
  arr[1] = PointerType::get(Builder.getInt8Ty(),0);
  ArrayRef<Type*> args(arr);

  FunctionType *FnTy = FunctionType::get(Builder.getVoidTy(),args,false);
  Function *F = Function::Create(FnTy,GlobalValue::ExternalLinkage,ss.str(),M);
  BasicBlock *BB = BasicBlock::Create(M->getContext(),"entry",F,NULL);

  Builder.SetInsertPoint(BB);
  ++F;
  //BS.insertPointer(Builder,(Value*)&*(F->arg_begin()),(Value*)&*(++F->arg_begin()));
  BS.insertPointer(Builder,(Value*)&*(F->arg_begin()),(Value*)&*(F->arg_begin()));
  Builder.CreateRetVoid();
}

void BuildSignatureAPI::CreateMembershipFn(Module *M) {
  std::stringstream ss;
  ss << BS.getName() << "MembershipFn";
  IRBuilder<> Builder(M->getContext());
  Type* arr[2] = {BS.getSignatureType(),
                  PointerType::get(Builder.getInt8Ty(),0)};
  ArrayRef<Type*> args(arr);
  FunctionType *FnTy = FunctionType::get(Builder.getInt32Ty(),args,false);
  Function *F = Function::Create(FnTy,GlobalValue::ExternalLinkage,ss.str(),M);
  BasicBlock *BB = BasicBlock::Create(M->getContext(),"entry",F,NULL);
  Builder.SetInsertPoint(BB);
  Instruction* initRet = Builder.CreateRet(Builder.getInt32(0));
  Builder.SetInsertPoint(initRet);
  ++F;
  Instruction *ret = cast<Instruction>(BS.checkMembership(Builder,
                        (Value*)&*(F->arg_begin()), (Value*)&*(F->arg_begin())));
  Builder.SetInsertPoint(initRet);
  Builder.CreateRet(ret);
  initRet->eraseFromParent();
}
