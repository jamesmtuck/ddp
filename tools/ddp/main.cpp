//===- opt.cpp - The LLVM Modular Optimizer -------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Optimizations may be specified an arbitrary number of times on the command
// line, They are run in the order specified.
//
//===----------------------------------------------------------------------===//

#include "ddp/llvm/Version.h"
#if (LLVM_VERSION_MAJOR==3) && (LLVM_VERSION_MINOR>4)
#include "PassPrinters.h"
#endif
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
using namespace llvm;

//#include "DDPInfoInAA.h"
//#include "QueryInterface.h"
//#include "GCMQueries.h"
//#include "MustAliasQueries.h"
#include "AddressProfiler.h"
#include "SetProfiler.h"
#include "EdgeProfiler.h"
//#include "FunctionSpecialization.h"
//#include "FunctionVersioning.h"
//#include "QueryAnalysis.h"
//#include "feedback.h"
//#include "opts.h"

#include <unistd.h>

using namespace llvm;

// The OptimizationList is automatically populated with registered Passes by the
// PassNameParser.
//
static cl::list<const PassInfo*, bool, PassNameParser>
PassList(cl::desc("Optimizations available:"));

// Other command line options...
//
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode file>"),
    cl::init("-"), cl::value_desc("filename"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Override output filename"),
               cl::value_desc("filename"));

static cl::opt<bool>
Force("f", cl::desc("Enable binary output on terminals"));

static cl::opt<bool>
PrintEachXForm("p", cl::desc("Print module after each transformation"));

static cl::opt<bool>
NoOutput("disable-output",
         cl::desc("Do not write result bitcode file"), cl::Hidden);

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

static cl::opt<bool>
NoVerify("disable-verify", cl::desc("Do not verify result module"), cl::Hidden);

static cl::opt<bool>
VerifyEach("verify-each", cl::desc("Verify after each transform"));

static cl::opt<bool>
StripDebug("strip-debug",
           cl::desc("Strip debugger symbol info from translation unit"));

static cl::opt<bool>
DisableInline("disable-inlining", cl::desc("Do not run the inliner pass"));

static cl::opt<bool>
DisableOptimizations("disable-opt",
                     cl::desc("Do not run any optimization passes"));

static cl::opt<bool>
DisableInternalize("disable-internalize",
                   cl::desc("Do not mark all symbols as internal"));

static cl::opt<bool>
StandardCompileOpts("std-compile-opts",
                   cl::desc("Include the standard compile time optimizations"));

static cl::opt<bool>
StandardLinkOpts("std-link-opts",
                 cl::desc("Include the standard link time optimizations"));

static cl::opt<bool>
OptLevelO1("O1",
           cl::desc("Optimization level 1. Similar to llvm-gcc -O1"));

static cl::opt<bool>
OptLevelO2("O2",
           cl::desc("Optimization level 2. Similar to llvm-gcc -O2"));

static cl::opt<bool>
OptLevelOs("Os",
           cl::desc("Like -O2 with extra optimizations for size. Similar to clang -Os"));

static cl::opt<bool>
OptLevelOz("Oz",
           cl::desc("Like -Os but reduces code size further. Similar to clang -Oz"));


static cl::opt<bool>
OptLevelO3("O3",
           cl::desc("Optimization level 3. Similar to llvm-gcc -O3"));

static cl::opt<std::string>
TargetTriple("mtriple", cl::desc("Override target triple for module"));

static cl::opt<bool>
UnitAtATime("funit-at-a-time",
            cl::desc("Enable IPO. This is same as llvm-gcc's -funit-at-a-time"),
            cl::init(true));

static cl::opt<bool>
DisableLoopUnrolling("disable-loop-unrolling",
                     cl::desc("Disable loop unrolling in all relevant passes"),
                     cl::init(false));
static cl::opt<bool>
DisableLoopVectorization("disable-loop-vectorization",
                     cl::desc("Disable the loop vectorization pass"),
                     cl::init(false));

static cl::opt<bool>
DisableSLPVectorization("disable-slp-vectorization",
                        cl::desc("Disable the slp vectorization pass"),
                        cl::init(false));

static cl::opt<bool>
DisableSimplifyLibCalls("disable-simplify-libcalls",
                        cl::desc("Disable simplify-libcalls"));

static cl::opt<bool>
Quiet("q", cl::desc("Obsolete option"), cl::Hidden);

static cl::alias
QuietA("quiet", cl::desc("Alias for -q"), cl::aliasopt(Quiet));

static cl::opt<bool>
AnalyzeOnly("analyze", cl::desc("Only perform analysis, no optimization"));

static cl::opt<bool>
PrintBreakpoints("print-breakpoints-for-testing",
                 cl::desc("Print select breakpoints location for testing"));

static cl::opt<std::string>
DefaultDataLayout("default-data-layout",
          cl::desc("data layout string to use if not specified by module"),
          cl::value_desc("layout-string"), cl::init(""));

static cl::opt<std::string>
OriginalFilename("origin",cl::desc("The original C/C++ file from which this code is compiled"),cl::init(" "));

static cl::opt<std::string>
ApplicationName("appname",cl::desc("The name of the application this is a part of"),cl::init(" "));


static cl::opt<std::string>
ProfileDBPrefix("profile-db-prefix",cl::desc("The location of the profiling database"),cl::init(""));

// ---------- Define Printers for module and function passes ------------
namespace {

struct CallGraphSCCPassPrinter : public CallGraphSCCPass {
  static char ID;
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  std::string PassName;

  CallGraphSCCPassPrinter(const PassInfo *PI, raw_ostream &out) :
    CallGraphSCCPass(ID), PassToPrint(PI), Out(out) {
      std::string PassToPrintName =  PassToPrint->getPassName();
      PassName = "CallGraphSCCPass Printer: " + PassToPrintName;
    }

  virtual bool runOnSCC(CallGraphSCC &SCC) {
    if (!Quiet)
      Out << "Printing analysis '" << PassToPrint->getPassName() << "':\n";

    // Get and print pass...
    for (CallGraphSCC::iterator I = SCC.begin(), E = SCC.end(); I != E; ++I) {
      Function *F = (*I)->getFunction();
      if (F)
        getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out,
                                                              F->getParent());
    }
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};

char CallGraphSCCPassPrinter::ID = 0;

struct ModulePassPrinter : public ModulePass {
  static char ID;
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  std::string PassName;

  ModulePassPrinter(const PassInfo *PI, raw_ostream &out)
    : ModulePass(ID), PassToPrint(PI), Out(out) {
      std::string PassToPrintName =  PassToPrint->getPassName();
      PassName = "ModulePass Printer: " + PassToPrintName;
    }

  virtual bool runOnModule(Module &M) {
    if (!Quiet)
      Out << "Printing analysis '" << PassToPrint->getPassName() << "':\n";

    // Get and print pass...
    getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out, &M);
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};

char ModulePassPrinter::ID = 0;
struct FunctionPassPrinter : public FunctionPass {
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  static char ID;
  std::string PassName;

  FunctionPassPrinter(const PassInfo *PI, raw_ostream &out)
    : FunctionPass(ID), PassToPrint(PI), Out(out) {
      std::string PassToPrintName =  PassToPrint->getPassName();
      PassName = "FunctionPass Printer: " + PassToPrintName;
    }

  virtual bool runOnFunction(Function &F) {
    if (!Quiet)
      Out << "Printing analysis '" << PassToPrint->getPassName()
          << "' for function '" << F.getName() << "':\n";

    // Get and print pass...
    getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out,
            F.getParent());
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};

char FunctionPassPrinter::ID = 0;

struct LoopPassPrinter : public LoopPass {
  static char ID;
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  std::string PassName;

  LoopPassPrinter(const PassInfo *PI, raw_ostream &out) :
    LoopPass(ID), PassToPrint(PI), Out(out) {
      std::string PassToPrintName =  PassToPrint->getPassName();
      PassName = "LoopPass Printer: " + PassToPrintName;
    }


  virtual bool runOnLoop(Loop *L, LPPassManager &LPM) {
    if (!Quiet)
      Out << "Printing analysis '" << PassToPrint->getPassName() << "':\n";

    // Get and print pass...
    getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out,
                        L->getHeader()->getParent()->getParent());
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};

char LoopPassPrinter::ID = 0;

struct RegionPassPrinter : public RegionPass {
  static char ID;
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  std::string PassName;

  RegionPassPrinter(const PassInfo *PI, raw_ostream &out) : RegionPass(ID),
    PassToPrint(PI), Out(out) {
    std::string PassToPrintName =  PassToPrint->getPassName();
    PassName = "RegionPass Printer: " + PassToPrintName;
  }

  virtual bool runOnRegion(Region *R, RGPassManager &RGM) {
    if (!Quiet) {
      Out << "Printing analysis '" << PassToPrint->getPassName() << "' for "
          << "region: '" << R->getNameStr() << "' in function '"
          << R->getEntry()->getParent()->getName() << "':\n";
    }
    // Get and print pass...
   getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out,
                       R->getEntry()->getParent()->getParent());
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};

char RegionPassPrinter::ID = 0;

struct BasicBlockPassPrinter : public BasicBlockPass {
  const PassInfo *PassToPrint;
  raw_ostream &Out;
  static char ID;
  std::string PassName;

  BasicBlockPassPrinter(const PassInfo *PI, raw_ostream &out)
    : BasicBlockPass(ID), PassToPrint(PI), Out(out) {
      std::string PassToPrintName =  PassToPrint->getPassName();
      PassName = "BasicBlockPass Printer: " + PassToPrintName;
    }

  virtual bool runOnBasicBlock(BasicBlock &BB) {
    if (!Quiet)
      Out << "Printing Analysis info for BasicBlock '" << BB.getName()
          << "': Pass " << PassToPrint->getPassName() << ":\n";

    // Get and print pass...
    getAnalysisID<Pass>(PassToPrint->getTypeInfo()).print(Out,
            BB.getParent()->getParent());
    return false;
  }

  virtual const char *getPassName() const { return PassName.c_str(); }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequiredID(PassToPrint->getTypeInfo());
    AU.setPreservesAll();
  }
};


char BasicBlockPassPrinter::ID = 0;

#if !defined(DDP_LLVM_VERSION_3_6) && !defined(DDP_LLVM_VERSION_3_7)
struct BreakpointPrinter : public ModulePass {
  raw_ostream &Out;
  static char ID;
  DITypeIdentifierMap TypeIdentifierMap;

  BreakpointPrinter(raw_ostream &out)
    : ModulePass(ID), Out(out) {
    }

  void getContextName(DIDescriptor Context, std::string &N) {
    if (Context.isNameSpace()) {
      DINameSpace NS(Context);
      if (!NS.getName().empty()) {
        getContextName(NS.getContext(), N);
        N = N + NS.getName().str() + "::";
      }
    } else if (Context.isType()) {
      DIType TY(Context);
      if (!TY.getName().empty()) {
        getContextName(TY.getContext().resolve(TypeIdentifierMap), N);
        N = N + TY.getName().str() + "::";
      }
    }
  }

  virtual bool runOnModule(Module &M) {
    TypeIdentifierMap.clear();
    NamedMDNode *CU_Nodes = M.getNamedMetadata("llvm.dbg.cu");
    if (CU_Nodes)
      TypeIdentifierMap = generateDITypeIdentifierMap(CU_Nodes);

    StringSet<> Processed;
    if (NamedMDNode *NMD = M.getNamedMetadata("llvm.dbg.sp"))
      for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i) {
        std::string Name;
        DISubprogram SP(NMD->getOperand(i));
        assert((!SP || SP.isSubprogram()) &&
          "A MDNode in llvm.dbg.sp should be null or a DISubprogram.");
        if (!SP)
          continue;
        getContextName(SP.getContext().resolve(TypeIdentifierMap), Name);
        Name = Name + SP.getDisplayName().str();
        if (!Name.empty() && Processed.insert(Name)) {
          Out << Name << "\n";
        }
      }
    return false;
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
  }
};
#endif
 
} // anonymous namespace

//char BreakpointPrinter::ID = 0;


static inline void addPass(PassManagerBase &PM, Pass *P) {
  // Add the pass to the pass manager...
  PM.add(P);

  // If we are verifying all of the intermediate steps, add the verifier...
  if (VerifyEach) PM.add(createVerifierPass(TRUE_ARG));
}

/// AddOptimizationPasses - This routine adds optimization passes
/// based on selected optimization level, OptLevel. This routine
/// duplicates llvm-gcc behaviour.
///
/// OptLevel - Optimization Level
static void AddOptimizationPasses(PassManagerBase &MPM,FunctionPassManager &FPM,
                                  unsigned OptLevel, unsigned SizeLevel) {
  FPM.add(createVerifierPass(TRUE_ARG));                  // Verify that input is correct

  PassManagerBuilder Builder;
  Builder.OptLevel = OptLevel;
  Builder.SizeLevel = SizeLevel;

  if (DisableInline) {
    // No inlining pass
  } else if (OptLevel > 1) {
    unsigned Threshold = 225;
    if (SizeLevel == 1)      // -Os
      Threshold = 75;
    else if (SizeLevel == 2) // -Oz
      Threshold = 25;
    if (OptLevel > 2)
      Threshold = 275;
    Builder.Inliner = createFunctionInliningPass(Threshold);
  } else {
    Builder.Inliner = createAlwaysInlinerPass();
  }
  Builder.DisableUnitAtATime = !UnitAtATime;
  Builder.DisableUnrollLoops = (DisableLoopUnrolling.getNumOccurrences() > 0) ?
                               DisableLoopUnrolling : OptLevel == 0;

  Builder.LoopVectorize =
      DisableLoopVectorization ? false : OptLevel > 1 && SizeLevel < 2;
  Builder.SLPVectorize =
      DisableSLPVectorization ? false : OptLevel > 1 && SizeLevel < 2;

  Builder.populateFunctionPassManager(FPM);
  Builder.populateModulePassManager(MPM);
}

static void AddStandardCompilePasses(PassManagerBase &PM) {
  PM.add(createVerifierPass(TRUE_ARG));                  // Verify that input is correct

  // If the -strip-debug command line option was specified, do it.
  if (StripDebug)
    addPass(PM, createStripSymbolsPass(true));

  if (DisableOptimizations) return;

  // -std-compile-opts adds the same module passes as -O3.
  PassManagerBuilder Builder;
  if (!DisableInline)
    Builder.Inliner = createFunctionInliningPass();
  Builder.OptLevel = 3;
  Builder.populateModulePassManager(PM);
}

static void AddStandardLinkPasses(PassManagerBase &PM) {
  PM.add(createVerifierPass(TRUE_ARG));                  // Verify that input is correct

  // If the -strip-debug command line option was specified, do it.
  if (StripDebug)
    addPass(PM, createStripSymbolsPass(true));

  if (DisableOptimizations) return;

  PassManagerBuilder Builder;
#if defined(DDP_LLVM_VERSION_3_6) || defined(DDP_LLVM_VERSION_3_7)
  Builder.populateLTOPassManager(PM);
#else
  Builder.populateLTOPassManager(PM, /*Internalize=*/ !DisableInternalize,
                                 /*RunInliner=*/ !DisableInline);
#endif
}

void ProfilerSupport(Module &M)
{
  std::string original="";

  char path[1024] = "";
  getcwd(path,1024);

  if ( OriginalFilename.getNumOccurrences() == 0 ) {
    original = std::string(path) + "/" + InputFilename;
  } else {
    original = OriginalFilename;
  }

  errs() << "Original file name: " << original << "\n";
  errs() << "Profile prefix: " << ProfileDBPrefix;

  DBFileManager::Initialize(ProfileDBPrefix,original,ApplicationName);
}

//===----------------------------------------------------------------------===//
// main for opt
//
int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  llvm::PrettyStackTraceProgram X(argc, argv);

  // Enable debug stream buffering.
  EnableDebugBuffering = true;

  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();

  // Initialize passes
  PassRegistry &Registry = *PassRegistry::getPassRegistry();
  initializeCore(Registry);
  initializeScalarOpts(Registry);
  initializeVectorization(Registry);
  initializeIPO(Registry);

  //initializeFunctionSpecializationPass(Registry);
  //initializeFunctionVersioningPass(Registry);
  //initializeFeedbackPass(Registry);
  //initializeDDPInfoInAAPass(Registry);

  initializeAnalysis(Registry);
  initializeIPA(Registry);
  initializeTransformUtils(Registry);
  initializeInstCombine(Registry);
  initializeInstrumentation(Registry);
  initializeTarget(Registry);

  //initializeQueryInterfacePass(Registry);
  //initializeGCMQueriesPass(Registry);
  
  initializeSetProfilerPass(Registry);
  initializeAddressProfilerPass(Registry);
  initializeEdgeProfilerPass(Registry);
  initializeEdgeProbabilityPass(Registry);
  initializeEdgeProbabilityReaderPass(Registry);

  //initializeLoopInvCodeMotionPass(Registry);
  //initializeDeadBlockEliminationPass(Registry);
  //initializeQueryAnalysisPass(Registry);
  //initializeMustAliasQueriesPass(Registry);
  //initializeRedundantValueEliminationPass(Registry);

  cl::ParseCommandLineOptions(argc, argv,
    "llvm .bc -> .bc modular optimizer and analysis printer\n");

  if (AnalyzeOnly && NoOutput) {
    errs() << argv[0] << ": analyze mode conflicts with no-output mode.\n";
    return 1;
  }

  // Allocate a full target machine description only if necessary.
  // FIXME: The choice of target should be controllable on the command line.
  std::auto_ptr<TargetMachine> target;

  SMDiagnostic Err;

  // Load the input module...
#ifdef LLVM_AFTER_34
#if defined(DDP_LLVM_VERSION_3_6) || defined(DDP_LLVM_VERSION_3_7)
  std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);
#else
  std::unique_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));
#endif
#else
  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));
#endif


  if (M.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }

  ProfilerSupport(*M.get());

  // If we are supposed to override the target triple, do so now.
  if (!TargetTriple.empty())
    M->setTargetTriple(Triple::normalize(TargetTriple));

  // Figure out what stream we are supposed to write to...

#ifdef LLVM_AFTER_34
  std::unique_ptr<tool_output_file> Out;
#else
  OwningPtr<tool_output_file> Out;
#endif
  if (NoOutput) {
    if (!OutputFilename.empty())
      errs() << "WARNING: The -o (output filename) option is ignored when\n"
                "the --disable-output option is used.\n";
  } else {
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
  }

  // If the output is set to be emitted to standard out, and standard out is a
  // console, print out a warning message and refuse to do it.  We don't
  // impress anyone by spewing tons of binary goo to a terminal.
  if (!Force && !NoOutput && !AnalyzeOnly && !OutputAssembly)
    if (CheckBitcodeOutputToConsole(Out->os(), !Quiet))
      NoOutput = true;

  // Create a PassManager to hold and optimize the collection of passes we are
  // about to build.
  //
  PassManager Passes;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
#ifdef DDP_LLVM_VERSION_3_7
  TargetLibraryInfoImpl *TLI = new TargetLibraryInfoImpl(Triple(M->getTargetTriple()));
#else
  TargetLibraryInfo *TLI = new TargetLibraryInfo(Triple(M->getTargetTriple()));
#endif

  // The -disable-simplify-libcalls flag actually disables all builtin optzns.
  if (DisableSimplifyLibCalls)
    TLI->disableAllFunctions();

#ifdef DDP_LLVM_VERSION_3_7
  TargetLibraryInfoWrapperPass *TLIPass = new TargetLibraryInfoWrapperPass(*TLI);
  Passes.add(TLIPass);
#else
  Passes.add(TLI);
#endif

  // Add an appropriate TargetData instance for this module.
#ifdef DDP_LLVM_VERSION_3_7
  const DataLayout TD = M.get()->getDataLayout();
  if (TD.isDefault() && !DefaultDataLayout.empty()) {
    M->setDataLayout(DefaultDataLayout);
  }
  //Do nothing else if 3.7.0. There is no DataLayoutPass
#elif defined(LLVM_AFTER_34)
  const DataLayout *TD = M.get()->getDataLayout();
  if (!TD && !DefaultDataLayout.empty()) {
    M->setDataLayout(DefaultDataLayout);
    TD = M.get()->getDataLayout();
  }
  if (TD)
#ifdef DDP_LLVM_VERSION_3_6
    Passes.add(new DataLayoutPass());
#else
    Passes.add(new DataLayoutPass(M.get()));
#endif
#else
  DataLayout *TD = 0;
  const std::string &ModuleDataLayout = M.get()->getDataLayout();
  if (!ModuleDataLayout.empty())
    TD = new DataLayout(ModuleDataLayout);
  else if (!DefaultDataLayout.empty())
    TD = new DataLayout(DefaultDataLayout);
  if (TD)
    Passes.add(TD);
#endif

#ifdef LLVM_AFTER_34
  std::unique_ptr<FunctionPassManager> FPasses;
#else
  OwningPtr<FunctionPassManager> FPasses;
#endif
  if (OptLevelO1 || OptLevelO2 || OptLevelO3) {
    FPasses.reset(new FunctionPassManager(M.get()));
#ifdef DDP_LLVM_VERSION_3_7
//Do nothing if 3.7.0. There is no DataLayoutPass
#elif defined(LLVM_AFTER_34)
    if (TD)
#ifdef DDP_LLVM_VERSION_3_6
      FPasses->add(new DataLayoutPass());
#else
      FPasses->add(new DataLayoutPass(M.get()));
#endif
    //if (TM.get())
    //  TM->addAnalysisPasses(*FPasses);
#else
    if (TD)
      FPasses->add(new DataLayout(*TD));
#endif
  }

  // If the -strip-debug command line option was specified, add it.  If
  // -std-compile-opts was also specified, it will handle StripDebug.
  if (StripDebug && !StandardCompileOpts)
    addPass(Passes, createStripSymbolsPass(true));

  // Create a new optimization pass for each one specified on the command line
  for (unsigned i = 0; i < PassList.size(); ++i) {
    // Check to see if -std-compile-opts was specified before this option.  If
    // so, handle it.
    if (StandardCompileOpts &&
        StandardCompileOpts.getPosition() < PassList.getPosition(i)) {
      AddStandardCompilePasses(Passes);
      StandardCompileOpts = false;
    }

    if (StandardLinkOpts &&
        StandardLinkOpts.getPosition() < PassList.getPosition(i)) {
      AddStandardLinkPasses(Passes);
      StandardLinkOpts = false;
    }

    if (OptLevelO1 && OptLevelO1.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, 1, 0);
      OptLevelO1 = false;
    }

    if (OptLevelO2 && OptLevelO2.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, 2, 0);
      OptLevelO2 = false;
    }

    if (OptLevelOs && OptLevelOs.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, 2, 1);
      OptLevelOs = false;
    }

    if (OptLevelOz && OptLevelOz.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, 2, 2);
      OptLevelOz = false;
    }

    if (OptLevelO3 && OptLevelO3.getPosition() < PassList.getPosition(i)) {
      AddOptimizationPasses(Passes, *FPasses, 3, 0);
      OptLevelO3 = false;
    }

    const PassInfo *PassInf = PassList[i];
    Pass *P = 0;
    if (PassInf->getNormalCtor())
      P = PassInf->getNormalCtor()();
    else
      errs() << argv[0] << ": cannot create pass: "
             << PassInf->getPassName() << "\n";
    if (P) {
      PassKind Kind = P->getPassKind();
      addPass(Passes, P);

      if (AnalyzeOnly) {
        switch (Kind) {
        case PT_BasicBlock:
          Passes.add(new BasicBlockPassPrinter(PassInf, Out->os()));
          break;
        case PT_Region:
          Passes.add(new RegionPassPrinter(PassInf, Out->os()));
          break;
        case PT_Loop:
          Passes.add(new LoopPassPrinter(PassInf, Out->os()));
          break;
        case PT_Function:
          Passes.add(new FunctionPassPrinter(PassInf, Out->os()));
          break;
        case PT_CallGraphSCC:
          Passes.add(new CallGraphSCCPassPrinter(PassInf, Out->os()));
          break;
        default:
          Passes.add(new ModulePassPrinter(PassInf, Out->os()));
          break;
        }
      }
    }

    if (PrintEachXForm)
#ifdef LLVM_AFTER_34
      Passes.add(createPrintModulePass(errs()));
#else
      Passes.add(createPrintModulePass(&errs()));
#endif
  }

  // If -std-compile-opts was specified at the end of the pass list, add them.
  if (StandardCompileOpts) {
    AddStandardCompilePasses(Passes);
    StandardCompileOpts = false;
  }

  if (StandardLinkOpts) {
    AddStandardLinkPasses(Passes);
    StandardLinkOpts = false;
  }

  if (OptLevelO1)
    AddOptimizationPasses(Passes, *FPasses, 1, 0);

  if (OptLevelO2)
    AddOptimizationPasses(Passes, *FPasses, 2, 0);

  if (OptLevelOs)
    AddOptimizationPasses(Passes, *FPasses, 2, 1);

  if (OptLevelOz)
    AddOptimizationPasses(Passes, *FPasses, 2, 2);

  if (OptLevelO3)
    AddOptimizationPasses(Passes, *FPasses, 3, 0);

  if (OptLevelO1 || OptLevelO2 || OptLevelO3) {
    FPasses->doInitialization();
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
      FPasses->run(*F);
    FPasses->doFinalization();
  }

  // Check that the module is well formed on completion of optimization
  if (!NoVerify && !VerifyEach)
    Passes.add(createVerifierPass(TRUE_ARG));


  PassManager LastPasses;  
  // Write bitcode or assembly to the output as the last step...
  if (!NoOutput && !AnalyzeOnly) {
    if (OutputAssembly)
#ifdef LLVM_AFTER_34
      LastPasses.add(createPrintModulePass(Out->os()));
#else
      LastPasses.add(createPrintModulePass(&Out->os()));
#endif
    else
      LastPasses.add(createBitcodeWriterPass(Out->os()));
  }

  // Before executing passes, print the final values of the LLVM options.
  cl::PrintOptionValues();

  // Now that we have all of the passes ready, run them.
  Passes.run(*M.get());

  // Must keep this in a separate pass manager!
  // Guarantees that all profile modifications are committed first.
  LastPasses.run(*M.get());

  // Declare success.
  if (!NoOutput || PrintBreakpoints)
    Out->keep();

  return 0;
}
