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
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "ProfilerDatabase.h"
#include "ProfileDBHelper.h"

using namespace llvm;

static cl::opt<std::string>
DumpProfToFile("dump-profile-to-filename",
		           cl::desc("Do not dump to database, dump to specified filename"),
		           cl::init("prof.out"));

GlobalVariable *ProfileDBHelper::buildArray(Module &M) {
  StructType *mystruct = getProfStructType();
  Twine t("profiler_refids");
  ArrayRef<Constant*> aref(ddp_init);
  Constant *ca = ConstantArray::get(ArrayType::get(mystruct,ddp_init.size()),aref);
  GlobalVariable* array =
    new GlobalVariable(M, ArrayType::get(mystruct, ddp_init.size()),
                       false, llvm::GlobalValue::PrivateLinkage, ca, t);
  ddp_init.clear(); // clear this out so they aren't added to the dtors list
  return array;
}

/*
  insertDtorsCall inserts code equivalent to this:

   tool_profstruct array[N] = { ... };
   void toolname_dtor() {
       toolname_module_finish("/path/to/db",array,N);
   }

 */

Function* ProfileDBHelper::insertDtorsCall(Module &M) {
  GlobalVariable *array = buildArray(M);
  IRBuilder<> IRB(M.getContext());
  StructType *mystruct = getProfStructType();

  // Destructors must be void type functions with no argument
  // Create such a FunctionType
  Function::iterator bit;
  FunctionType *FnTy = FunctionType::get(IRB.getVoidTy(),false);

  // Make the destructor declaration
  std::string dtor_name = toolname+"_dtor";
  Function *prof_finish = Function::Create(FnTy,
                              llvm::GlobalValue::InternalLinkage, dtor_name, &M);

  // Inside the dtor, we'll call another function that does take arguments, but they
  // are all derived from globals.  So, we can easily generate them from
  // within the dtor.
  // This function is implemented in the profiling library. So, all we need to do
  // is call it, we don't need to define it.
  // Create the type for the callee
  Type *args[7];
  args[0] = PointerType::get(IRB.getInt8Ty(),0); //IRB.CreateGlobalString("/location/of/ddp.db");
  args[1] = PointerType::get(IRB.getInt8Ty(),0); //IRB.CreateGlobalString("/location/of/ddp.db");
  args[2] = PointerType::get(IRB.getInt8Ty(),0); //IRB.CreateGlobalString("/location/of/ddp.db");
  args[3] = PointerType::get(IRB.getInt8Ty(),0); //IRB.CreateGlobalString("/location/of/ddp.db");
  args[4] = IRB.getInt32Ty();
  args[5] = PointerType::get(mystruct,0);
  args[6] = IRB.getInt32Ty();

  // Declare the callee, must match the ProfStructType
  std::string tool_finish_name;

  //if (isConnected() && (DumpProfToFile.getNumOccurrences()==0))
    //tool_finish_name = "profiler_update_db";
  //else
    tool_finish_name = "profiler_update_file";
  FunctionType *FnTyCallee = FunctionType::get(IRB.getVoidTy(),args,false);
#ifdef DDP_LLVM_VERSION_3_7
  Constant *calleeConst = M.getOrInsertFunction(tool_finish_name, FnTyCallee);
  Function *callee = checkSanitizerInterfaceFunction(calleeConst);
#else
  Function *callee = (Function*)M.getOrInsertFunction(tool_finish_name, FnTyCallee);
#endif

  Twine entry="entry";
  BasicBlock *BB = BasicBlock::Create(M.getContext(),entry,prof_finish);

  // Insert the instructions inside toolname_dtor
  IRB.SetInsertPoint(BB);

  int size = ( (ArrayType*)array->getType()->getElementType())->getNumElements();
  ConstantInt *sz = IRB.getInt32(size);
  ConstantInt *fileid = IRB.getInt32(db->getFileID());
  Value *gepIndex0[2];
  Value *str2;
  //if (DumpProfToFile.getNumOccurrences())
    //str2 = IRB.CreateGlobalString("");
  //else
  str2 = IRB.CreateGlobalString(DBFileManager::getSingleton().getPrefix());
  gepIndex0[0] = IRB.getInt32(0);
  gepIndex0[1] = IRB.getInt32(0);
  Value *gep0 = IRB.CreateGEP(str2,ArrayRef<Value*>(gepIndex0));
  Value *str;
  if (isConnected() && (DumpProfToFile.getNumOccurrences()==0))
    str = IRB.CreateGlobalString(toolname+".db");
  else {
    if (DumpProfToFile.getNumOccurrences())
      str = IRB.CreateGlobalString(DumpProfToFile);
    else
      str = IRB.CreateGlobalString(toolname+".out");
  }

  Value *gepIndex1[2];
  gepIndex1[0] = IRB.getInt32(0);
  gepIndex1[1] = IRB.getInt32(0);
  Value *gep1 = IRB.CreateGEP(str,ArrayRef<Value*>(gepIndex1));

  Value *gepIndex2[2];
  gepIndex2[0] = IRB.getInt32(0);
  gepIndex2[1] = IRB.getInt32(0);
  Value *gep2 = IRB.CreateGEP(
                  IRB.CreateGlobalString(tableName),ArrayRef<Value*>(gepIndex2));

  Value *gepIndex3[2];
  gepIndex3[0] = IRB.getInt32(0);
  gepIndex3[1] = IRB.getInt32(0);
  Value *gep3 = IRB.CreateGEP(IRB.CreateGlobalString(
        DBFileManager::getSingleton().getOrigin()),ArrayRef<Value*>(gepIndex2));

  Value *gepIndex4[2];
  gepIndex4[0] = IRB.getInt32(0);
  gepIndex4[1] = IRB.getInt32(0);
  Value *gep4 = IRB.CreateGEP(array,ArrayRef<Value*>(gepIndex4));

  std::vector<Value*> cargs;
  cargs.push_back(gep0);
  cargs.push_back(gep1);
  cargs.push_back(gep2);
  cargs.push_back(gep3);
  cargs.push_back(fileid);
  cargs.push_back(gep4);
  cargs.push_back(sz);
  ArrayRef<Value*> cArgs(cargs);

   //CallInst *ci =
  IRB.CreateCall(callee,cArgs);
  IRB.CreateRetVoid();
  llvm::appendToGlobalDtors(M,prof_finish,0);
  return callee;
}

ProfileDBHelper::ProfileDBHelper(Module &aM, std::string name)
  :toolname(name),tableName("feedback"),M(aM) {
  db = ProfilerDatabase::CreateOrFind(name);
}

ProfileDBHelper::~ProfileDBHelper() {
  delete db;
}

void ProfileDBHelper::finishModule(Module &M) {
  // register all of the counters to be dumped to the database when the
  // program ends
  insertDtorsCall(M);
}

GlobalVariable *ProfileDBHelper::nextCounter(unsigned int total) {
  unsigned int ref = (unsigned int) incRefId();
  return getCounter(ref,total);
}

GlobalVariable *ProfileDBHelper::getCounter(unsigned int number, unsigned int total) {
  if (globalMap.find(number)==globalMap.end()) {
    GlobalVariable *gv = getGlobalVar(M);
    globalMap[number] = gv; // insert into hash for later lookup
    Type *Int32 = IntegerType::get(M.getContext(), 32);
    Constant *inits[6];
    inits[0] = ConstantInt::get(Int32,(int)number);
    inits[1] = gv;
    inits[2] = ConstantInt::get(Int32,(int)total);
    inits[3] = ConstantPointerNull::get(PointerType::get(Int32, 0));
    inits[4] = ConstantPointerNull::get(PointerType::get(Int32, 0));
    inits[5] = ConstantPointerNull::get(PointerType::get(Int32, 0));
    ArrayRef<Constant*> cref(inits, 6);
    StructType *mystruct = getProfStructType();
    Constant *cs =  ConstantStruct::get(mystruct, cref);
    ddp_init.push_back(cs);
  }
  return globalMap[number];
}

void ProfileDBHelper::addDBRecord(unsigned int refid, GlobalVariable *count,
				                          unsigned int total, GlobalVariable *totcnt,
                                  GlobalVariable *extra,
                                  GlobalVariable *population/*=NULL*/) {
  assert(globalMap.find(refid)==globalMap.end()
              && "Overwriting previous entry may lead to unexpected behavior.");
  globalMap[refid] = count; // insert into hash for later lookup
  Type *Int32 = IntegerType::get(M.getContext(), 32);
  Constant *inits[6];
  inits[0] = ConstantInt::get(Int32,(int)refid);
  inits[1] = count;
  inits[2] = ConstantInt::get(Int32,(int)total);
  inits[3] = totcnt ? (Constant*)totcnt :
                (Constant*)ConstantPointerNull::get(PointerType::get(Int32, 0));
  inits[4] = extra ? (Constant*)extra :
                (Constant*)ConstantPointerNull::get(PointerType::get(Int32, 0));
  inits[5] = population ? (Constant*)population :
                (Constant*)ConstantPointerNull::get(PointerType::get(Int32, 0));
  ArrayRef<Constant*> cref(inits, 6);
  StructType *mystruct = getProfStructType();
  Constant *cs = ConstantStruct::get(mystruct, cref);
  ddp_init.push_back(cs);
}
