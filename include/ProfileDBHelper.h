#ifndef PROFILER_DB_HELPER_H
#define PROFILER_DB_HELPER_H

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
#include <string>
#include <iostream>
#include "ProfilerDatabase.h"
#include "SQLite3Helper.h"
#include "UniqueRefID.h"
#include <map>

namespace llvm {

  class ProfileDBHelper {
  private:
    std::vector<Constant*> ddp_init;
    std::map<unsigned long, GlobalVariable*> globalMap;
    std::string toolname;
    std::string tableName;

  protected:
    ProfilerDatabase *db;
    Module &M;

    Function* insertDtorsCall(Module &M);
    GlobalVariable *buildArray(Module &M);

    virtual StructType *getProfStructType() {
      Type *Int32 = IntegerType::get(M.getContext(), 32);
      std::string name = "profstruct" + toolname;
      StructType *mystruct = M.getTypeByName(name);
      if (mystruct == NULL) {
        Type *types[6];
        types[0] = Int32;
        types[1] = PointerType::get(Int32,0);
        types[2] = Int32;
        types[3] = PointerType::get(Int32,0);
        types[4] = PointerType::get(Int32,0);
        types[5] = PointerType::get(Int32,0);
        ArrayRef<Type*> typeArray(types,6);
        mystruct = StructType::create(typeArray,name);
      }
      return mystruct;
    }

    GlobalVariable *getGlobalVar(Module &M) {
      std::string t=(toolname+"_cntr");
      Type *int32 = IntegerType::get(M.getContext(), 32);
      return new GlobalVariable(M, int32, false,
                                llvm::GlobalValue::PrivateLinkage,
                                ConstantInt::get(int32,0),t);
    }

    bool isConnected() {
      return db && db->isConnected();
    }

  public:
    ProfileDBHelper(Module &aM, std::string name);
    virtual ~ProfileDBHelper();

    void setTableName(std::string s) {  tableName = s; }

    ProfilerDatabase *getProfilerDatabase() { return db; }

    virtual void finishFunction(Function &F) {} // Remove
    virtual void finishModule(Module &M);

    GlobalVariable *nextCounter(unsigned int total=0);
    GlobalVariable *getCounter(unsigned int number, unsigned int total=0);

    void addDBRecord(unsigned int refid, GlobalVariable *count, unsigned int total=0,
		     GlobalVariable *totcnt=NULL,GlobalVariable *extra=NULL, GlobalVariable *population=NULL);

    unsigned long long incRefId() { return db->inc(); }
    unsigned long long getRefId() { return db->get(); }
    unsigned long long getFileId() { return db->getFileID(); }

    unsigned long long feedbackValue(unsigned long long refID) {
      return db->feedbackValue(toolname,refID);
    }
  };
}

#endif //PROFILER_DB_HELPER_H
