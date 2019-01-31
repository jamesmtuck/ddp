#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>

#include "llvm-c/Core.h"

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/CallGraphSCCPass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/DebugInfo.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Analysis/LoopPass.h"
#include "llvm/Analysis/RegionPass.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Support/PassNameParser.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/Debug.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/SystemUtils.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/LinkAllPasses.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include <memory>
#include <algorithm>


//#include "DDPInfoInAA.h"
//#include "QueryInterface.h"
//#include "GCMQueries.h"
//#include "MustAliasQueries.h"
#include "SetProfiler.h"
//#include "FunctionSpecialization.h"
//#include "FunctionVersioning.h"
//#include "QueryAnalysis.h"
//#include "feedback.h"
//#include "opts.h"

#include "db/ProfilerDatabase.h"

using namespace llvm;

void ProfilerSupport();

// The OptimizationList is automatically populated with registered Passes by the
// PassNameParser.
//
static cl::list<const PassInfo*, bool, PassNameParser>
PassList(cl::desc("Optimizations available:"));


static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<bool>
OutputAssembly("S", cl::desc("Write output as LLVM assembly"));

static cl::opt<std::string>
OriginalFilename("origin",cl::desc("The original C/C++ file from which this code is compiled"),cl::init(" "));

static cl::opt<std::string>
ProfileDBPrefix("profile-db-prefix",cl::desc("The location of the profiling database"),cl::init(""));

/*static cl::opt<bool>
DoProfile("do-profile",
  cl::desc("Add profiling instrumentation."),
  cl::init(false));

static cl::opt<bool>
UseProfiler("use-profile",
  cl::desc("Use profiling instrumentation."),
  cl::init(false));

static cl::opt<bool>
GCM("gcm",
  cl::desc("Perform GCM Optimization."),
  cl::init(false));
*/

/*static cl::opt<bool>
DumpSummary("summary",
  cl::desc("Dump summary stats."),
  cl::init(false));
*/

static inline void addPass(PassManagerBase &PM, Pass *P) {
  // Add the pass to the pass manager...
  PM.add(P);

  // If we are verifying all of the intermediate steps, add the verifier...
  PM.add(createVerifierPass());
}


int
main (int argc, char ** argv)
{

  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  LLVMContext &Context = getGlobalContext();

  // 1. Must come first for PassList command line paramter 
  //      Register new DDP specific passes
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
  //initializeLoopInvCodeMotionPass(Registry);
  //initializeModifiedEdgeProfilerPass(Registry);
  //  initializeModLoaderPassPass(Registry);
  //initializeDeadBlockEliminationPass(Registry);
  //initializeQueryAnalysisPass(Registry);
  //initializeMustAliasQueriesPass(Registry);
  //initializeRedundantValueEliminationPass(Registry);

  // 2. Parse command line
  cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");

  OwningPtr<tool_output_file> Out;
  // Default to standard output.
  if (OutputFilename.empty())
    OutputFilename = "-";
  
  std::string ErrorInfo;
  Out.reset(new tool_output_file(OutputFilename.c_str(), ErrorInfo,
				 sys::fs::F_Binary));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    return 1;
  }

  SMDiagnostic Err;

  std::auto_ptr<Module> M;
  M.reset(ParseIRFile(InputFilename, Err, Context));

  if (M.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }

  ProfilerSupport();

  PassManager Passes;

  // Create a new optimization pass for each one specified on the command line
  for (unsigned i = 0; i < PassList.size(); ++i) {
    // Check to see if -std-compile-opts was specified before this option.  If

    const PassInfo *PassInf = PassList[i];
    Pass *P = 0;
    if (PassInf->getNormalCtor())
      P = PassInf->getNormalCtor()();
    else
      errs() << argv[0] << ": cannot create pass: "
             << PassInf->getPassName() << "\n";
    if (P) {
      //PassKind Kind = P->getPassKind();
      addPass(Passes, P);
    }
  }

  Passes.add(createVerifierPass());
  Passes.run(*M.get());

  PassManager LastPasses;  
  if (OutputAssembly)
    LastPasses.add(createPrintModulePass(&Out->os()));
  else
    LastPasses.add(createBitcodeWriterPass(Out->os()));

  // Must keep this in a separate pass manager!
  // Guarantees that all profile modifications are committed first.
  LastPasses.run(*M.get());

  Out->keep();

  return 0;
}


static cl::opt<std::string>
ApplicationName("appname",cl::desc("The name of the application this is a part of"),cl::init(" "));


void ProfilerSupport()
{
  std::string original="";

  char path[1024] = "";
  getcwd(path,1024);

  if ( OriginalFilename.getNumOccurrences() == 0 ) {
    original = std::string(path) + "/" + InputFilename;
  } else {
    original = OriginalFilename;
  }

  DBFileManager::Initialize(ProfileDBPrefix,original,ApplicationName);
}
