#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ddp/llvm/Version.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/CommandFlags.h"

#if !defined(DDP_LLVM_VERSION_3_6) && !defined(DDP_LLVM_VERSION_3_7)
#include "llvm/DebugInfo.h"
#endif

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"

#if (LLVM_VERSION_MAJOR==3) && (LLVM_VERSION_MINOR>4)
#define LLVM_AFTER_34
#define TRUE_ARG true
#include "llvm/IR/IRPrintingPasses.h" 
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/IR/LegacyPassNameParser.h"
#include "llvm/IR/Verifier.h"
#include "llvm/InitializePasses.h"
#else
#define TRUE_ARG 
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Support/PassNameParser.h"
#include "llvm/Support/Host.h"
#endif
#include "llvm/LinkAllIR.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/MC/SubtargetFeature.h"
#ifdef DDP_LLVM_VERSION_3_7
#include "ddp/llvm/PassManager_3.6.2.h"
#else
#include "llvm/PassManager.h"
#endif
#include "llvm/Support/Debug.h"
#ifdef LLVM_AFTER_34
#include "llvm/Support/FileSystem.h"
#endif
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#ifdef DDP_LLVM_VERSION_3_7
#include "llvm/Analysis/TargetLibraryInfo.h"
#else
#include "llvm/Target/TargetLibraryInfo.h"
#endif
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <algorithm>
#include <memory>

#include "BuildSignature.h"
#include "db/ProfilerDatabase.h"

using namespace llvm;

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

#if 0
void GenSignatureCode(Module *M)
{
  IRBuilder<> Builder(getGlobalContext());  
  FunctionType *IntFnTy = FunctionType::get(Builder.getInt32Ty(),false);
  Function *F = Function::Create(IntFnTy,GlobalValue::InternalLinkage,"tester",M);
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(),"entry",F,NULL);

  std::vector< Value* > vec;

  BuildSpecialHashIndex<3,0x3F> Hash;
  SimpleSignature SS(64,Hash);

  Builder.SetInsertPoint(BB);
  SS.allocateLocal(Builder);

  for(int i=0; i<50; i++)
    {
      Value *v;
      vec.push_back(v=Builder.CreateAlloca(Builder.getInt32Ty(),Builder.getInt32(1)));

      if (i%5==0)
	SS.insertPointer(Builder,v);
    }
  
  Builder.CreateRet(  Builder.CreateTrunc(SS.checkMembership(Builder,vec[6]), Builder.getInt32Ty() ));
}
#endif

int
main (int argc, char ** argv)
{
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();
  cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");

#ifdef LLVM_AFTER_34
  std::unique_ptr<tool_output_file> Out;
#else
  OwningPtr<tool_output_file> Out;
#endif
  // Default to standard output.
  if (OutputFilename.empty())
    OutputFilename = "-";
  
#ifdef LLVM_AFTER_34
#if defined(DDP_LLVM_VERSION_3_6) || defined(DDP_LLVM_VERSION_3_7)
   std::error_code EC;
    Out.reset(new tool_output_file(OutputFilename, EC, sys::fs::F_None));
    if (EC) {
      errs() << EC.message() << '\n';
      return 1;
    }
#else
    std::string ErrorInfo;
    Out.reset(new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                                   sys::fs::F_None));
    if (!ErrorInfo.empty()) {
      errs() << ErrorInfo << '\n';
      return 1;
    }

#endif
#else
    std::string ErrorInfo;
    Out.reset(new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                                   sys::fs::F_Binary));
    if (!ErrorInfo.empty()) {
      errs() << ErrorInfo << '\n';
      return 1;
    }
#endif

  SMDiagnostic Err;

  // Build new module
  Module *M  = new Module("instr-tester",getGlobalContext());

#ifdef LLVM_AFTER_34
  M->setTargetTriple(LLVMGetDefaultTargetTriple());
#else
  M->setTargetTriple(sys::getDefaultTargetTriple());
#endif

#define DeclareSimple(Size,Mask)  	   \
  {                                        \
    SimpleSignature SS(Size,HashBuilderFactory::CreateShiftMaskIndex(Size,Mask));         \
    BuildSignatureAPI B(SS);               \
    B.CreateAPI(M);                        \
  }

#define DeclareSimpleKnuth(Size,Mask)  	   \
  {                                        \
    SimpleSignature SS(Size,HashBuilderFactory::CreateKnuthIndex(Size,Mask));         \
    BuildSignatureAPI B(SS);               \
    B.CreateAPI(M);                        \
  }

  {
    //DeclareSimple(32,0x1F)

  SimpleSignature SS64(64,HashBuilderFactory::CreateShiftMaskIndex(0,0x3F));
  BuildSignatureAPI B64(SS64);
  B64.CreateAPI(M);

  SimpleSignature SS128(128,HashBuilderFactory::CreateShiftMaskIndex(0,0x7F));
  BuildSignatureAPI B128(SS128);
  B128.CreateAPI(M);

  SimpleSignature SS256(256,HashBuilderFactory::CreateShiftMaskIndex(0,0xFF));
  BuildSignatureAPI B256(SS256);
  B256.CreateAPI(M);

  }

  {
    DeclareSimpleKnuth(32,0x1F)
    DeclareSimple(1024,0x3FF)
  }

  {
    // BuildSpecialHashIndex<2,0x3FF> Hash5;
    ArraySignature A1024(32,32,HashBuilderFactory::CreateXorIndex(2,0x3FF));
    BuildSignatureAPI BA1024(A1024);
    BA1024.CreateAPI(M);
  }

  {
    //BuildDynHashIndex Hash6(2,0xFFF);
    ArraySignature A4096(32,128,HashBuilderFactory::CreateShiftMaskIndex(2,0xFFF));
    BuildSignatureAPI BA4096(A4096);
    BA4096.CreateAPI(M);
  }

  {
    BankedSignature B(3,32,32);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }
  {
    BankedSignature B(2,32,32);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(3,32,16);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(2,32,16);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(2,32,32*4);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(4,32,8);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(3,32,64);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(2,32,32*8);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    BankedSignature B(2,32,32*8);
    DumpSet DS(&B,1);
    BuildSignatureAPI BA(DS);
    BA.CreateAPI(M);
  }

  {
    RangeAndBankedSignature B(2,32,16);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    RangeAndBankedSignature B(2,32,32);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    RangeAndBankedSignature B(3,32,32);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    RangeAndBankedSignature B(2,32,32*2);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    RangeAndBankedSignature B(2,32,32*4);
    BuildSignatureAPI BA(B);
    BA.CreateAPI(M);
  }

  {
    PerfectSet P;
    BuildSignatureAPI PAPI(P);
    PAPI.CreateAPI(M);
  }

  {
    HashTableSet P;
    BuildSignatureAPI PAPI(P);
    PAPI.CreateAPI(M);
  }

  //GenSignatureCode(M);

  // Dump function to bitcode
  WriteBitcodeToFile(M,Out->os());

  Out->keep();

  return 0;
}


