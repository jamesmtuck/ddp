//===- Instrument.cpp - Set Profiler for data dependence profiling -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// 
// This instruments the code for all the sets based on queries.
// 
//
//===----------------------------------------------------------------------===//
//
#define DEBUG_TYPE "setinstrument"

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
#include "Instrument.h"
#include  "SetInstrumentFactory.h"
#include <vector>

STATISTIC(NumQueries, "Number of queries instrumented");
STATISTIC(NumRepeatQueries, "Number of repeated queries during instrumentataion");
STATISTIC(NumQueryAllocas, "Number of query allocas added");
STATISTIC(NumMembershipTests, "Number of membership tests added");
STATISTIC(NumInsertions, "Number of insertions added");

using namespace llvm;

extern cl::opt<unsigned> TableSize;

static cl::opt<bool> PeriodicDumping("enable-periodic-dumping", cl::Hidden,
		cl::desc("Profile data dumped out periodically "
				"(-period specifies number of func calls)"), cl::init(false));

static cl::opt<unsigned int> Period("period", cl::Hidden,
		cl::desc("Number of function calls captured in each profile dump"),
		cl::init(1000));

static cl::opt<unsigned int> DumpRefid("dump-refid", cl::Hidden,
		cl::desc("Dump a log of all comparisons on the named refid"),
		cl::init(0));

static cl::opt<unsigned int> SignSize("signsize", cl::Hidden,
		cl::desc("Signature Size. Size = 1024 by default"), cl::init(1024));

static cl::opt<bool> UseLibCalls("ddp-use-runtime-lib-calls", cl::Hidden,
		cl::desc("Use library calls to operate on signatures "
				"(usually much higher overhead)"), cl::init(false));

cl::opt<bool> PerfInstr("perfinstr", cl::Hidden,
		cl::desc("Perfect Instrumentation is enabled"), cl::init(false));

cl::opt<bool> RangeInstr("rangeinstr", cl::Hidden,
		cl::desc("RangeSet Instrumentation is enabled"), cl::init(false));

cl::opt<bool> HybridSign("hybrid", cl::Hidden,
		cl::desc("Range Checking + Banked Signature"), cl::init(false));

static cl::opt<bool> HTInstr("htinstr", cl::Hidden,
		cl::desc("Hash Table Instrumentation is enabled"), cl::init(false));

static cl::opt<bool> SignInstr("signinstr", cl::Hidden,
		cl::desc("Signature Instrumentation is enabled"), cl::init(false));

static cl::opt<bool> FastSign("fastsign", cl::Hidden,
		cl::desc("Use faster signatures (less accurate)"), cl::init(false));

static cl::opt<bool> EarlyTermination("early-termination", cl::Hidden,
		cl::desc("Stop checking for a dependence in a region once it's "
				"confirmed"), cl::init(false));

static cl::opt<bool> PopulationCount("population-count", cl::Hidden,
		cl::desc("Store the population counts for signatures in DB"),
		cl::init(true));

cl::opt<bool> StructSizeDynSign("struct-size-based-sign", cl::Hidden,
		cl::desc("Create banked signature which takes traced struct "
				"size into account"), cl::init(false));

static void Insert_Prof_Init(Function &F, unsigned int IsEdgeProf,
		Instruction* pos) {
	assert(pos && "Error: Position for insertion of Prof_Init cannot be null");

	DEBUG_WITH_TYPE("ddp", outs() << "Insert_Prof_Init - Begin\n");

	errs() << "INSERTING PROF INIT\n";
	Module &M = *(F.getParent());
	LLVMContext &Context = F.getContext();

	Type *typeArray[2] = { Type::getInt32Ty(Context), Type::getInt8Ty(Context) };
#ifdef DEBUG
	DEBUG_WITH_TYPE("ddp", outs() << "Inserting Prof Init\n");
#endif	
	FunctionType *funcType = FunctionType::get(Type::getVoidTy(Context),
			typeArray, 0);
	Constant* ProfInitFn = M.getOrInsertFunction("Prof_Init", funcType);

	std::vector<Value*> Args(2);
	Args[0] = ConstantInt::get(Type::getInt32Ty(Context), TableSize, false);
	Args[1] = ConstantInt::get(Type::getInt8Ty(Context), IsEdgeProf, false);
	CallInst::Create(ProfInitFn, makeArrayRef(Args), "", pos);
	errs() << "CALL TO PROFILE INIT INSERTED\n";
	DEBUG_WITH_TYPE("ddp", outs() << "Insert_Prof_Init - End\n");
}

Value* SetInstrument::getPointerOperand(Instruction* I) {
	if (LoadInst* LI = dyn_cast < LoadInst > (I)) {
		return LI->getPointerOperand();
	} else if (StoreInst* SI = dyn_cast < StoreInst > (I)) {
		return SI->getPointerOperand();
	} else {
		assert(0 && "getPointerOperand() on Inst that isn't load/store");
		return NULL;
	}
}

static const int minStructSize = 16;

SetInstrument::SetInstrument(ddp::Queries &aAQ, Function &Fn,
		ProfileDBHelper &dbHelper) :
		AQ(aAQ), F(Fn), Region(Fn), Context(F.getContext()), M(*F.getParent()), pos(
				F.getEntryBlock().getFirstNonPHI()), DBHelper(dbHelper) {
	errs() << "+++INSTRUMENTING FUNCTION: " << Fn.getName() << "\n";
	errs() << "Query size: " << AQ.size() << "\n";
	inserted.clear();
	ProfileSets.clear();
	StructPsetMap.clear();
	clearChecked();

	ddp::Queries::query_iterator i, end = AQ.end();
	for (i = AQ.begin(); i != end; i++) {
		unsigned int set = (*i).pset;
		if (ProfileSets.find(set) == ProfileSets.end()) {
			SImple *Set = NULL;
			errs() << "SIMPLE SET\n";
			// Haven't seen this set before. allocate it.
			if (SignInstr) {
				if (UseLibCalls)
					Set = SImpleFactory::CreateLibCallSignature();
				else {
					if (FastSign) {
						Set = SImpleFactory::CreateFastSignature(SignSize);
					} else if (HybridSign) {
						Set = SImpleFactory::CreateHybridSignature(SignSize);
					} else {
						//Detect if store comes from a struct and create struct style signature.
						int structSize;
						//if(StructSizeDynSign.getValue() && minStructSize <= ( structSize = traceStructSize(i->rhs->getOperand(1)) ) ) {
						if (StructSizeDynSign && minStructSize <= (structSize =
								traceStructSize(i->rhs->getOperand(1)))) {
							errs() << "Creating struct based signature : ID="
									<< (*i).id << " Size = " << structSize
									<< "\n";
							assert(
									StructPsetMap.find(set)
											== StructPsetMap.end());
							StructPsetMap[set] = structSize;
							Set = SImpleFactory::CreateDynStructSignature(
									SignSize, structSize);
						} else {
							Set = SImpleFactory::CreateAccurateSignature(
									SignSize);
						}
					}
				}

				if (DumpRefid.getNumOccurrences() > 0) {
					if (DumpRefid == (*i).id) {
						Set = new DumpSet(Set, (*i).id);
					}
				}

				ProfileSets[set] = new SetInstrumentHelper<>(Region, Set,
						EarlyTermination);

			} else if (PerfInstr) {
				errs() << "PERFECT SET\n";
				ProfileSets[set] = new SetInstrumentHelper<SImple,
						AllocateHeap<SImple> >(Region,
						SImpleFactory::CreatePerfectSet(), EarlyTermination);
			} else if (RangeInstr) {
				ProfileSets[set] = new SetInstrumentHelper<>(Region,
						SImpleFactory::CreateRangeSet(), EarlyTermination);
			} else if (HTInstr) {
				//typedef SetInstrumentHelper< HashTableSet, AllocateUniqueGlobal<HashTableSet> >
				//        HashTableHelper;
				typedef SetInstrumentHelper<> HashTableHelper;

				ProfileSets[set] = new HashTableHelper(Region,
						(HashTableSet*) SImpleFactory::CreateHashTableSet(),
						EarlyTermination);

			} else {
				DEBUG_WITH_TYPE("ddp",
						outs() << "Please specify an instrumentation option\n");
				assert(0);
			}
		} else {
			errs() << "NO SIGINSTR\n";
			if (StructPsetMap.find(set) != StructPsetMap.end()) {
				assert(StructPsetMap[set] > minStructSize);
				errs() << "Creating struct based signature : ID=" << (*i).id
						<< " Size = " << StructPsetMap[set] << "\n";
			}
			if (DumpRefid.getNumOccurrences() > 0) {
				if (DumpRefid == (*i).id) {
					SImple &Set = ProfileSets[set]->getSetImpl();
					delete ProfileSets[set];
					SImple *nSet = new DumpSet(&Set, (*i).id);
					ProfileSets[set] = new SetInstrumentHelper<>(Region, nSet,
							EarlyTermination);
				}
			}
		}
	}

	if (strcmp(F.getName().data(), "main") == 0) {
		// To do: add this to the constructor list, not to main
		Insert_Prof_Init(Fn, 0, Fn.getEntryBlock().getFirstNonPHI());
	}
	(Fn.getParent())->print(llvm::errs(), nullptr);
}

int SetInstrument::traceStructSize(Value *val) {
	Value *startVal = val;
	Instruction *inst;
	std::map<PHINode*, int> phiSelector;
	while (1) {
		if (!isa < Instruction > (val)) {
			// FIXME: Should we test for constant GEP ?
			return 0;
		} else {
			inst = cast < Instruction > (val);
			if (inst->getOpcode() == Instruction::GetElementPtr) {
				val = inst->getOperand(0);
				Type* ty = val->getType();
				if (ty->isPointerTy()
						&& ty->getPointerElementType()->isStructTy()) {
					Module *mod = inst->getParent()->getParent()->getParent();
					int structSize = (mod->getDataLayout()).getTypeAllocSize(
							ty->getPointerElementType());
					//errs()<<"Struct traced : Size = "<<structSize<<" Type = "<<*(ty->getPointerElementType())<<"\n";
					if (structSize >= minStructSize) {
						return structSize;
					}
				}
			} else if (inst->getOpcode() == Instruction::BitCast) {
				val = inst->getOperand(0);
			} else if (inst->getOpcode() == Instruction::PHI) {
				PHINode *phi = cast < PHINode > (inst);
				if (phiSelector.find(phi) == phiSelector.end()) {
					phiSelector[phi] = 0;
				} else {
					phiSelector[phi]++;
					if (phiSelector[phi] >= phi->getNumIncomingValues())
						return 0;
				}
				val = phi->getIncomingValue(phiSelector[phi]);
			} else {
				return 0;
			}
//         errs()<<" ++++++ Tracing : "<<*val<<"\n";
		}
	}
}

void SetInstrument::getReturnInsts(std::vector<ReturnInst*> &Rets) {
	for (Function::iterator i = F.begin(); i != F.end(); i++) {
		//BasicBlock *BB = i;
		BasicBlock *BB = &*i;
		if (BB->getTerminator()) {
			if (isa < ReturnInst > (BB->getTerminator())) {
				Rets.push_back(dyn_cast < ReturnInst > (BB->getTerminator()));
			}
		}
	}
}

#if 0
// Here, we iterate over the each of the return BBs (basic blocks containing return 
// statements) and create an if-condition which decides when an instr call to dump 
// the profile data is called
// Original: return
// Final: if(call_count >= period) { dump_prof_data(); call_count = 0; } return;
void SetInstrument::insertDumpInstrumentation(Pass *P, Function &F) {
	Type* VoidTy = Type::getVoidTy(Context);
	Type* IntTy = Type::getInt32Ty(Context);

	std::string str = "Prof_" + F.getName().str() + ".out";
	unsigned int strSize = str.length() + 1;
	//	unsigned int strSize = 4;
	llvm::Constant *format_const =
	llvm::ConstantDataArray::getString(Context, str);

	llvm::GlobalVariable *var =
	new llvm::GlobalVariable(
			*F.getParent(), llvm::ArrayType::get(llvm::IntegerType::get(Context, 8), strSize),
			true, llvm::GlobalValue::PrivateLinkage, format_const, ".str");

	// charater is 8 bits each, and there a 4 of them in "%d\n"
	llvm::Constant *zero =
	llvm::Constant::getNullValue(llvm::IntegerType::getInt32Ty(Context));

	std::vector<llvm::Constant*> indices;
	indices.push_back(zero);
	indices.push_back(zero);
	llvm::Constant *var_ref =
	llvm::ConstantExpr::getGetElementPtr(var, indices);

	// Create the global variable call_count.
	// To make it unambiguous, we'll add the function name to the end of the variable name
	GlobalVariable *CountVar = new GlobalVariable(M, IntTy, false,
			GlobalValue::InternalLinkage, Constant::getNullValue(IntTy), "call_count_" + F.getName());

	// Find the return BBs.
	// We'll get the return insts, and then find their parents.
	std::vector<ReturnInst*> Rets;
	getReturnInsts(Rets);

	for(unsigned int i = 0; i < Rets.size(); i++) {
		ReturnInst* RI = Rets[i];
		BasicBlock* OrigRetBB = RI->getParent();

		//-------------- Creating the CFG required to insert the dumping --------------------

		// Now, split the block at RI
		BasicBlock* InstrBB = SplitBlock(OrigRetBB, RI, P);

		// Now, we have OrigRetBB which has all the statements except the ReturnInst RI.
		// Now, split the InstrBB at RI again to create the NewRetBB
		BasicBlock* NewRetBB = SplitBlock(InstrBB, RI, P);

		// Now, load the CountVar in the OrigRetBB.
		LoadInst* CountVal = new LoadInst(CountVar,"call_count_" + F.getName() + "_load", OrigRetBB->getTerminator());

		// get zero value
		Value* OneVal = ConstantInt::get(Type::getInt32Ty(Context), 1, false);

		// Increment CountVal by 1 and store it.
		Value *NewCountVal = BinaryOperator::Create(Instruction::Add, CountVal, OneVal,
				"Add_call_count_" + F.getName(), OrigRetBB->getTerminator());
		new StoreInst(NewCountVal, CountVar, OrigRetBB->getTerminator());

		// Get the value for Period
		Value* PeriodVal = ConstantInt::get(IntTy, Period, false);

		// Compare the CountVal with the Period value provided
		ICmpInst* Compare = new ICmpInst(OrigRetBB->getTerminator(), ICmpInst::ICMP_UGE, NewCountVal, PeriodVal, "call_count_icmp");

		// Now, insert a branch instruction to choose between the InstrBB (if Compare is TRUE), and NewRetBB (if Compare is FALSE) 
		// if(CountVal >= PeriodVal), Compare = TRUE, Branch -> InstrBB
		// else, Compare = FALSE, Branch->NewRetBB
		BranchInst *BI = BranchInst::Create(InstrBB, NewRetBB, Compare, OrigRetBB->getTerminator());

		// Replace all uses of OrigRetBB->getTerminator with BI
		OrigRetBB->getTerminator()->replaceAllUsesWith(BI);
		OrigRetBB->getTerminator()->eraseFromParent();

		//--------------------------------------------------------------------

		//------------ Reset call_count variable and add instr call in InstrBB -----------
		// Reset call_count variable

		// get zero value
		Value* ZeroVal = ConstantInt::get(IntTy, 0, false);

		new StoreInst(ZeroVal, CountVar, InstrBB->getTerminator());

		// insert instrumentation call 
		Constant* DumpProfDataFn = M.getOrInsertFunction("Dump_Prof_Data", VoidTy,
				var_ref->getType(),
				IntTy,// start RefID
				IntTy,// end RefID
				(Type*)0);
		// Args
		std::vector<Value*> Args(3);
		Args[0] = var_ref;
		Args[1] = ConstantInt::get(Type::getInt32Ty(Context), QI.startRefID, false);
		Args[2] = ConstantInt::get(Type::getInt32Ty(Context), QI.endRefID, false);
		CallInst::Create(DumpProfDataFn, makeArrayRef(Args), "", InstrBB->getTerminator());
		//	CallInst::Create(DumpProfDataFn,"", InstrBB->getTerminator());
		//----------------------------------------------------------------------------------

	}
}
#endif

AllocaInst* SetInstrument::allocateVariableForQuery(ddp::Query &Q,
		std::vector<ReturnInst*> &Rets) {

	// Create the variable for tracking the load
	//Q.done = false;
	Instruction * lhs = Q.lhs;
	//unsigned long long j = Q.RHS.find_first();     
	RefPair p = std::make_pair(lhs, Q.pset);

	/*
	 Someone fix me please!!!

	 This logic is a big hack.  I'm basically trying to figure out if the same load
	 will be compared against the same signature more than once.  If it is, then I want
	 to re-use the same local variable to cache the result rather than using a different
	 local variable.
	 */
	AllocaInst *PerLoadAI = NULL;
	std::stringstream ss;
	ss << "PerQuery" << Q.id;

	if (queryVars.find(p) == queryVars.end()) {
		// Order:
		// BB: Var Decl; Store 0, Var Decl

		//printf("%x:%d query\n",lhs,Q.pset);

		Type *IntTy = Type::getInt32Ty(Context);

		PerLoadAI = new AllocaInst(IntTy, 0, Twine(ss.str()), pos);
		Value* ConstantZero = ConstantInt::get(IntTy, 0, false);
		new StoreInst(ConstantZero, PerLoadAI, pos);

		queryVars[p] = PerLoadAI;

		NumQueryAllocas++;
	} else {

		//printf("%x:%d repeated query\n",lhs,Q.pset);

		PerLoadAI = queryVars[p];
		Q.repeated = true;
		NumRepeatQueries++;
	}

	/*
	 GlobalVariable *Count=NULL;
	 GlobalVariable *Extra=NULL;
	 GlobalVariable *population=nullptr;
	 {
	 // TO DO: Control this with flag
	 BasicBlock &entry = F.getEntryBlock();
	 IRBuilder<> Builder(entry.getFirstNonPHI());
	 
	 std::string t=("prof_counter");
	 Type *int32 = IntegerType::get(Context,32);
	 Count = new GlobalVariable(*F.getParent(),int32,
	 false,llvm::GlobalValue::PrivateLinkage,
	 ConstantInt::get(int32,0),t);
	 }
	 {
	 // TO DO: Control this with flag
	 BasicBlock &entry = F.getEntryBlock();
	 IRBuilder<> Builder(Q.rhs);
	 
	 std::string t=("extra_counter");
	 Type *int32 = IntegerType::get(Context,32);
	 Extra = new GlobalVariable(*F.getParent(),int32,
	 false,llvm::GlobalValue::PrivateLinkage,
	 ConstantInt::get(int32,0),t);

	 Builder.CreateStore(Builder.CreateAdd(Builder.CreateLoad(Extra),ConstantInt::get(int32,1)),Extra);
	 } 
	 //if(PopulationCount.getValue())
	 if(PopulationCount)
	 {
	 if(populationCounterMap.find(Q.pset) == populationCounterMap.end()){
	 // TO DO: Control this with flag
	 Type *int32 = IntegerType::get(Context,32);
	 population = new GlobalVariable(*F.getParent(),int32, false,llvm::GlobalValue::PrivateLinkage, ConstantInt::get(int32,0),"population_counter");
	 for(unsigned int j = 0; j < Rets.size(); j++) {
	 IRBuilder<> Builder(Rets[j]);
	 Value *sigPopulationValue = ProfileSets[Q.pset]->getSignatureInfo(sigInfoType::population,Rets[j]);
	 Builder.CreateStore(Builder.CreateAdd(Builder.CreateLoad(population),sigPopulationValue),population);
	 }
	 populationCounterMap[Q.pset] = population;
	 } else {
	 population = populationCounterMap[Q.pset];
	 }
	 }

	 // JMT HACK
	 /* TODO: Someone fix me please

	 There is a more elegant solution that is possible here.  
	 
	 If we are getting a new alloca above (if-case) then we need a new global. But,
	 if we get an old Alloca out of the map, we can re-use the old global associated
	 with it. 
	 
	 This requires changes to DBHelper because we need to tell it something like:
	 ...give me a new global id but use this counter and schedule the udpate at 
	 program termination using the counter provided instead. 

	 That would be much more efficient.     
	 */

	// Get function
	//GlobalVariable *profdb_gv = Count;
	//DBHelper.addDBRecord(Q.id,Count,Q.total,fCount,Extra,population);
	//Type* VoidTy  = Type::getVoidTy(Context);	

	/*
	 for(unsigned int j = 0; j < Rets.size(); j++) {
	 Value* Var = new LoadInst(PerLoadAI, ss.str(), Rets[j]);
	 Value *Var_GV = new LoadInst(profdb_gv, "t1d", Rets[j]);
	 Value *New_GV = BinaryOperator::Create(Instruction::Add, Var, Var_GV, "t2d", Rets[j]);
	 new StoreInst(New_GV, profdb_gv, Rets[j]);    
	 }
	 */

	return PerLoadAI;
}

//static cl::opt<int>
//LimitInstrumentation("limit-instrumentation", cl::Hidden,
//		 cl::desc("limit the number of instrumented queries"), cl::init(-1));

int LimitInstrumentation = -1;

void SetInstrument::instrNoAliasQueries(std::vector<ReturnInst*> &Rets) {
	unsigned int size = AQ.size();

	//if (LimitInstrumentation.getNumOccurrences()>0 && 
	if (LimitInstrumentation > 0 && size > (unsigned) LimitInstrumentation)
		size = LimitInstrumentation;

	ddp::Queries::query_iterator i, end = AQ.end();
	{
		// TO DO: Control this with flag
		BasicBlock &entry = F.getEntryBlock();
		IRBuilder<> Builder(entry.getFirstNonPHI());

		std::string t = ("funentry_cntr");
		Type *int32 = IntegerType::get(Context, 32);
		fCount = new GlobalVariable(*F.getParent(), int32, false,
				llvm::GlobalValue::PrivateLinkage, ConstantInt::get(int32, 0),
				t);
		Builder.CreateStore(
				Builder.CreateAdd(Builder.CreateLoad(fCount),
						ConstantInt::get(int32, 1)), fCount);
	}

	for (i = AQ.begin(); i != end; i++) {
		ddp::Query &Q = *i;

		NumQueries++;
		// CREATE A VARIABLE FOR THIS QUERY -- FROM THE INSTRLOAD FUNCTION ABOVE
		// UPDATE THE COUNTERS BASED ON THAT VARIABLE
		// PASS THAT VARIABLE INTO MembershipCheck. 

		// JMT Hack: please fix me. this only works if LHS & RHS have a single bit set
		AllocaInst* QueryVar = allocateVariableForQuery(Q, Rets);

		// Perform insertions for each of the RHS elements
		// The actual number of insertions is based on 
		// the underlying storage

		InsertValue(Q.rhs, Q);

		// JMT Hack: please fix me. this only works if LHS has a single bit
		if (Q.repeated)
			continue;

		// Perform membership checks for each of the elements
		// on the LHS, with each of the elements on the RHS
		// The actual number of membership checks is based 
		// on the underlying storage
		errs() << "--MEMBERSHIP CHECK\n";
		MembershipCheck(Q.lhs, Q, QueryVar);
		errs() << "MEMBERSHIP CHECK\n";
	}
	instrExits(Rets);
	errs() << "NO ALIAS QUERIES\n";
}

/*
 Instruction* Instrument::getInst(unsigned int instID) {
 assert(QI.InstIDs.right.find(instID) != QI.InstIDs.right.end());
 return (Instruction*)(QI.InstIDs.right.at(instID));
 }*/

//void Instrument::instrMustAliasQueries(std::vector<ReturnInst*> &Rets) {
/*
 Type *IntTy = Type::getInt32Ty(Context);
 
 for(unsigned int i = 0; i < QI.MustAliasQueries.size(); i++) {
 Query &Q = (QI.MustAliasQueries[i]);
 
 // Create variable for the query, and insert the necessary update_counters function call on return blocks
 AllocaInst* QueryVar = allocateVariableForQuery(Q, Rets);
 
 // Succ on LHS. Pred on RHS		
 // Only one on each side
 
 assert(Q.LHS.count() == 1);
 unsigned int LHSInstID = Q.LHS.find_first();
 
 assert(Q.RHS.count() == 1);
 unsigned int RHSInstID = Q.RHS.find_first();
 
 Instruction* LHSInst = getInst(LHSInstID);
 Instruction* RHSInst = getInst(RHSInstID);
 
 Value* LHSVal = getPointerOperand(LHSInst);
 Value* RHSVal = getPointerOperand(RHSInst);
 
 
 // Pred Block: RHSInst
 //
 //
 // Succ Block: LHSInst -- Insert before LHS
 //
 Instruction* pos = LHSInst;
 
 Constant* CompareValFn = M.getOrInsertFunction("Compare_Values",
 IntTy, // Return Type
 LHSVal->getType(), // LHS Value Type
 RHSVal->getType(), // RHS Value Type
 (Type*)0);
 
 char buf1[50];
 sprintf(buf1, "CompRes_%llu_", Q.ID);	
 std::vector<Value*> Args(2);
 Args[0] = LHSVal;
 Args[1] = RHSVal; 
 Instruction* res = CallInst::Create(CompareValFn, makeArrayRef(Args), "", pos);
 
 // Load the value in the query variable
 Value* OldVal = new LoadInst(QueryVar, buf1, pos);
 
 // Or the value with current value of query variable with the result of the comparison
 char OrBuf[50];
 sprintf(OrBuf, "Or_%llu_", Q.ID);
 Value* NewVal = BinaryOperator::Create(Instruction::Or, OldVal, res, OrBuf, pos);
 
 // Store the updated query val back in the query var
 new StoreInst(NewVal, QueryVar, pos);
 }
 */
//}
bool SetInstrument::instrument(Pass* P, Function &F) {
	errs() << "--SET INSTRUMENT: INSTRUMENT\n";
	if (ProfileSets.begin() == ProfileSets.end()) {
		errs() << "SET INSTRUMENT: FALSE\n";
		return false;
	}

	errs() << "SET INSTRUMENT: INSTRUMENT\n";
	// Note all the return basic blocks in the current function
	// for inserting the updates
	std::vector<ReturnInst*> Rets;
	getReturnInsts(Rets);

	// iterate over the instructions in Queries
	// For each store, perform an insert operation on the set
	// For each load, associate a variable with it, and perform
	// a membership operation -- This operation is
	// a little tricky since its vastly different
	// for different underlying storage configurations, but it
	// should still work with a single powerful interface. 
	// Subsequently, update the global
	// counters array using the variable

	// instrument No Alias Queries
	instrNoAliasQueries(Rets);
	errs() << "INSTR NO ALIAS QUERIES\n";

	// instrument Must Alias Queries
	//instrMustAliasQueries(Rets);

	// Instead of a single dump at the end of the profile run, we might need the ability
	// to periodically dump the profile output. This is accomplished through a function call
	// inserted on all the return blocks from a function. We create an if block here
	// which inserts an instrumentation call to dump the output after each period

	//if(PeriodicDumping) {
	//	insertDumpInstrumentation(P, F);
	//}

	// Make a final call to make any deallocations
	errs() << "FINALIZE\n";
	this->finalize(Rets);
	errs() << "--FINALIZE\n";

	return true;
}

// From SignatureInstrument

void SetInstrument::instrExits(RetInstVecTy &Rets) {
	for (SignMap::iterator i = ProfileSets.begin(); i != ProfileSets.end();
			i++) {
		AbstractSetInstrumentHelper < SImple > *ps = i->second;
		ps->FreeSet();
	}
}

void SetInstrument::InsertValue(Instruction *I, ddp::Query &Q) {
	if (inserted.find(I) != inserted.end())
		return;

	// Haven't inserted this InstID yet

	inserted.insert(I);

	// Now, find the set that this belongs to.
	//assert(SA.SetAssignments.find(InstID) != SA.SetAssignments.end());
	unsigned int set = Q.pset;
	// Now, insert the value into the set.
	ProfileSets[set]->Insert_Value(getPointerOperand(I), I);
	NumInsertions++;
	errs() << "VALUE INSERTED\n";
}

// THIS FUNCTION WOULD BE GIVEN THE VARIABLE WHICH NEEDS TO BE UPDATED WITH THE 
// OUTPUT OF THE CHECKINST
Instruction* SetInstrument::MembershipCheck(Instruction *I, ddp::Query &Q,
		AllocaInst* QueryVar) {
	std::stringstream VarBuf;
	VarBuf << "OldVal_" << Q.id;
	Value *OldVal = new LoadInst(QueryVar, VarBuf.str(), I);
	Value *NewVal = OldVal;

	// Check InstID versus each of the sets assigned to BV

	unsigned int set = Q.pset;

	Instruction* CheckInst;

	// Now, check if this membership check has already been scheduled.
	if (checked.find(I) == checked.end()) {
		// No entry for this InstID, so perform the check.
		CheckInst = ProfileSets[set]->MembershipCheckWith(getPointerOperand(I),
				I);
		NumMembershipTests++;
		// add this to the checked structure
		checked[I][set] = CheckInst;
	} else {
		// entry for InstID exists, check if InstID has a check with set already scheduled
		InstMap& IM = checked[I];
		if (IM.find(set) == IM.end()) {
			// Haven't checked this set yet.
			CheckInst = ProfileSets[set]->MembershipCheckWith(
					getPointerOperand(I), I);
			NumMembershipTests++;
			checked[I][set] = CheckInst;
		} else {
			// This check has already been scheduled. Get the CheckInst and return it.
			CheckInst = checked[I][set];
		}
	}

	std::stringstream OrBuf;
	OrBuf << "Or_" << Q.id;
	NewVal = BinaryOperator::Create(Instruction::Or, NewVal, CheckInst,
			OrBuf.str(), I);

	new StoreInst(NewVal, QueryVar, I);

	//I->getParent()->dump();

	errs() << "MembershipCheck\n";
	return NULL;
}

void SetInstrument::finalize(std::vector<ReturnInst*>& Rets) {
	for (SignMap::iterator i = ProfileSets.begin(); i != ProfileSets.end();
			i++) {
#ifndef USEBUILDER
		Signature* sign = i->second;
		sign->~Signature();
#endif
	}
	ProfileSets.clear();
}

void SetInstrument::clearChecked() {
	for (Int2InstMap::iterator i = checked.begin(); i != checked.end(); i++) {
		i->second.clear();
	}
	checked.clear();
}

