//===------------------------- BuildSignature.cpp -------------------------===//
//
//===----------------------------------------------------------------------===//
//
// Here we implement the functions that build signatures of different types.
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
#include "BuildSignature.h"
#include "SetInstrumentFactory.h"
#include <string>
#include <sstream>
#include <iostream>
#include <cmath>
#include <vector>

using namespace llvm;

static ManagedStatic<LLVMContext> GlobalContext;

static LLVMContext &getGlobalContext() {
	return *GlobalContext;
}

///=============================================================================

SimpleSignature::SimpleSignature(int aNumBits, const HashBuilder &hb) :
																 numBits(aNumBits), Ty(NULL),
																 hashBuilderLambda(hb) {
	Ty = Type::getIntNTy(getGlobalContext(), numBits);
}

Value* SimpleSignature::allocateLocal(IRBuilder<> Builder) {
	Value *AI = Builder.CreateAlloca(Ty, Builder.getInt32(1), "SimpleSignature");
	Builder.CreateStore(ConstantInt::get(Ty, 0), AI);
	return AI;
}

Value* SimpleSignature::allocateGlobal(IRBuilder<> Builder) {
	GlobalVariable *GV = new GlobalVariable(Ty, false, GlobalValue::ExternalLinkage,
																					ConstantInt::get(Ty, 0));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Builder.CreateStore(ConstantInt::get(Ty, 0), GV);
	return GV;
}

void SimpleSignature::insertPointer(IRBuilder<> Builder, Value *Sign,
		Value *V) {
	Value *index = Builder.CreateZExt(hashBuilderLambda(Builder, V), Ty);
	Value *load = Builder.CreateLoad(Sign);
	Value *val = Builder.CreateZExt(
			Builder.CreateBinOp(Instruction::Shl, ConstantInt::get(Ty, 1),
					index), Ty);
	Builder.CreateStore(Builder.CreateOr(load, val), Sign);
}

Value * SimpleSignature::checkMembership(IRBuilder<> Builder,
																				 Value *Sign, Value *V) {
	// Probably not as efficient as it could b
	Value *index = Builder.CreateZExt(hashBuilderLambda(Builder, V), Ty);
	Value *load = Builder.CreateLoad(Sign);
	Value *val = Builder.CreateBinOp(Instruction::Shl,
																	 ConstantInt::get(Ty, 1), index);

	val = Builder.CreateAnd(load, val);
	val = Builder.CreateICmpNE(val, ConstantInt::get(Ty, 0));

	return Builder.CreateZExt(val, Builder.getInt32Ty());
}

Type * SimpleSignature::getSignatureType() {
	return PointerType::get(Ty, 0);
}

std::string SimpleSignature::getName() {
	std::stringstream ss;
	ss << "SimpleSignature" << numBits;
	return ss.str();
}

///=======================================================================

ArraySignature::ArraySignature(int anumBitsEl, int alength,
		const HashBuilder &hb) :
		numBitsEl(anumBitsEl), length(alength), hashBuilderLambda(hb) {
	ElTy = Type::getIntNTy(getGlobalContext(), numBitsEl);
	SignTy = PointerType::get(ElTy, 0);

	isPow2 = true;
	switch (numBitsEl) {
	case 8:
		pow2 = 3;
		break;
	case 16:
		pow2 = 4;
		break;
	case 32:
		pow2 = 5;
		break;
	case 64:
		pow2 = 6;
		break;
	case 128:
		pow2 = 7;
		break;
	case 256:
		pow2 = 8;
		break;
	case 512:
		pow2 = 9;
		break;
	case 1024:
		pow2 = 10;
		break;
	default:
		isPow2 = false;
	}

	if (isPow2)
		pow2mask = (1 << pow2) - 1;
}

Value* ArraySignature::allocateLocal(IRBuilder<> Builder) {
	Value *AI = Builder.CreateAlloca(ElTy, Builder.getInt32(length),
																								"ArraySignature");
	Builder.CreateMemSet(AI, Builder.getInt8(0),
											 Builder.getInt64(length * 4), 4);
	return AI;
}

Value* ArraySignature::allocateGlobal(IRBuilder<> Builder) {
	ArrayType *AT = ArrayType::get(ElTy, length);
	GlobalVariable *GV = new GlobalVariable(AT, false, GlobalValue::ExternalLinkage,
																					Constant::getNullValue(AT));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Value *index[2];
	index[0] = Builder.getInt32(0);
	index[1] = Builder.getInt32(0);
	ArrayRef<Value*> indices(index);
	Value *gep = Builder.CreateGEP(GV, indices);
	Builder.CreateMemSet(gep, Builder.getInt8(0), Builder.getInt64(length * 4), 4);
	return gep;
}

void ArraySignature::insertPointer(IRBuilder<> Builder, Value *Sign, Value *V) {
	Value *index = hashBuilderLambda(Builder, V);
	assert(isPow2 && "Use element that's power of 2 for now!");

// Turn index into a array index
	Value *arrayOffset = Builder.CreateBinOp(Instruction::LShr, index,
																										Builder.getInt32(pow2));
	Value *gep;
	/*
	 if (isa<GlobalVariable>(Sign))
	 {
	 Value *index[2];
	 index[0] = Builder.getInt32(0);
	 index[1] = arrayOffset;
	 ArrayRef<Value*> indices(index);
	 gep = Builder.CreateGEP(Sign,indices);
	 }
	 else*/
	{
		gep = Builder.CreateGEP(Sign, arrayOffset);
	}

	Value *bitOffset = Builder.CreateAnd(index, Builder.getInt32(pow2mask));
	Value *orVal = Builder.CreateBinOp(Instruction::Shl, Builder.getInt32(1),
			bitOffset);

	if (numBitsEl > 32)
		orVal = Builder.CreateZExt(orVal, ElTy);
	else if (numBitsEl < 32)
		orVal = Builder.CreateTrunc(orVal, ElTy);

	Value *word = Builder.CreateLoad(gep);
	Builder.CreateStore(Builder.CreateOr(word, orVal), gep);
}

Value* ArraySignature::checkMembership(IRBuilder<> Builder, Value *Sign,
		Value *V) {
	Value *index = hashBuilderLambda(Builder, V);

	assert(isPow2 && "Use element that's power of 2 for now!");

	// Turn index into a array index
	Value *arrayOffset = Builder.CreateBinOp(Instruction::LShr, index,
			Builder.getInt32(pow2));
	Value *gep;
	/*if (isa<GlobalVariable>(Sign))
	 {
	 Value *index[2];
	 index[0] = Builder.getInt32(0);
	 index[1] = arrayOffset;
	 ArrayRef<Value*> indices(index);
	 gep = Builder.CreateGEP(Sign,indices);
	 }
	 else */
	{
		gep = Builder.CreateGEP(Sign, arrayOffset);
	}

	Value *bitOffset = Builder.CreateAnd(index, Builder.getInt32(pow2mask));
	Value *andVal = Builder.CreateBinOp(Instruction::Shl, Builder.getInt32(1),
			bitOffset);

	if (numBitsEl > 32)
		andVal = Builder.CreateZExt(andVal, ElTy);
	else if (numBitsEl < 32)
		andVal = Builder.CreateTrunc(andVal, ElTy);

	Value *word = Builder.CreateLoad(gep);
	Value *val = Builder.CreateAnd(word, andVal);

	val = Builder.CreateICmpNE(val, ConstantInt::get(ElTy, 0));
	return Builder.CreateZExt(val, Builder.getInt32Ty());
}

Type *ArraySignature::getSignatureType() {
	return PointerType::get(ElTy, 0);
}

std::string ArraySignature::getName() {
	std::stringstream ss;
	ss << "ArraySignature_" << numBitsEl << "_" << length;
	return ss.str();
}

///=======================================================================
BankedSignature::BankedSignature(int nBanks, int anumBitsEl, int alength) :
																	numBanks(nBanks), numBitsEl(anumBitsEl) {
	ElTy = Type::getIntNTy(getGlobalContext(), numBitsEl);
	SignTy = PointerType::get(ElTy, 0);

	int tot = numBitsEl * alength;
	int targetlevel = 0;
	while (tot >>= 1)
		++targetlevel;

	int offset = 2;
	int mask = (1 << targetlevel) - 1;
	for (int i = 0; i < nBanks; i++) {
		ArraySignature * as = new ArraySignature(numBitsEl, alength,
				HashBuilderFactory::CreateXorIndex(offset, mask));
		offset += targetlevel;
		banks.push_back(as);
	}
}

BankedSignature::BankedSignature(int anumBitsEl, const std::vector<int> &lengths,
																 std::vector<HashBuilder> &hashes) :
																												numBitsEl(anumBitsEl) {
	ElTy = Type::getIntNTy(getGlobalContext(), numBitsEl);
	SignTy = PointerType::get(ElTy, 0);

	assert(lengths.size() > 0 && "At least 1 bank is necessary");
	assert(
			lengths.size() == hashes.size()
					&& "Use same number of hashes as lengths");
	numBanks = lengths.size();

	for (int i = 0; i < numBanks; i++) {
		assert(lengths[i] > 0 && "Length should be at least 1");
		assert(hashes[i] && "Use a valid hash function");
		ArraySignature * as = new ArraySignature(numBitsEl, lengths[i],
				hashes[i]);
		banks.push_back(as);
	}
}

BankedSignature::~BankedSignature() {
	for (int i = 0; i < numBanks; i++) {
		delete banks[i];
	}
}

Value* BankedSignature::allocateLocal(IRBuilder<> Builder) {
	int totalLength = 0;
	for (auto &it : banks) {
		totalLength += it->getLength();
	}
	Value *AI = Builder.CreateAlloca(ElTy, Builder.getInt32(totalLength),
																											"BankedSignature");
	Builder.CreateMemSet(
					AI, Builder.getInt8(0), Builder.getInt64(totalLength * 4), 4);
	return AI;
}

Value* BankedSignature::allocateGlobal(IRBuilder<> Builder) {
	int totalLength = 0;
	for (auto &it : banks) {
		totalLength += it->getLength();
	}
	ArrayType *AT = ArrayType::get(ElTy, totalLength);
	GlobalVariable *GV = new GlobalVariable(AT, false, GlobalValue::ExternalLinkage,
																					Constant::getNullValue(AT));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Value *index[2];
	index[0] = Builder.getInt32(0);
	index[1] = Builder.getInt32(0);
	ArrayRef<Value*> indices(index);
	Value *gep = Builder.CreateGEP(GV, indices);
	Builder.CreateMemSet(gep, Builder.getInt8(0),Builder.getInt64(totalLength * 4), 4);
	return gep;
}

void BankedSignature::insertPointer(IRBuilder<> Builder,
																		Value *Sign, Value *V) {
	int cumulativeLength = 0;
	for (int i = 0; i < numBanks; i++) {
		Value *index[1];
		index[0] = Builder.getInt32(cumulativeLength);
		ArrayRef<Value*> indices(index);

		Value *gep = Builder.CreateGEP(Sign, indices);
		banks[i]->insertPointer(Builder, gep, V);
		cumulativeLength += banks[i]->getLength();
	}
}

Value* BankedSignature::checkMembership(IRBuilder<> Builder,
																				Value *Sign, Value *V) {
	Value *a = NULL;
	int cumulativeLength = 0;
	for (int i = 0; i < numBanks; i++) {
		Value *index[1];
		index[0] = Builder.getInt32(cumulativeLength);
		ArrayRef<Value*> indices(index);
		Value *gep = Builder.CreateGEP(Sign, indices);
		Value *res = banks[i]->checkMembership(Builder, gep, V);
		if (a == NULL)
			a = res;
		else
			a = Builder.CreateAnd(a, res);
		cumulativeLength += banks[i]->getLength();
	}
	return a;
}

Value* BankedSignature::getSignatureInfo(sigInfoType infoType,
																				 IRBuilder<> Builder,
																				 Value *Signature,
																				 Value *V /* = nullptr */) {
	if (infoType == population) {
		assert(numBitsEl == 32 && "Sizes other than 32 not yet supported.");
		Module *M =
				(Module*) Builder.GetInsertBlock()->getParent()->getParent();
		Constant* BitCountFn = M->getOrInsertFunction("Count_Bits",
				Builder.getInt32Ty(), Signature->getType(),
				Builder.getInt32Ty(), (Type*) 0);
		int totalLength = 0;
		for (auto &it : banks) {
			totalLength += it->getLength();
		}
		std::vector<Value *> Args(2);
		Args[0] = Signature;
		Args[1] = Builder.getInt32(totalLength);
		ArrayRef<Value*> args(Args);
		return Builder.CreateCall(BitCountFn, args);
	} else {
		return Builder.getInt32(0);
	}
}

Type *BankedSignature::getSignatureType() {
	return SignTy;
}

std::string BankedSignature::getName() {
	std::stringstream ss;
	int length = banks[0]->getLength();
	for (auto &it : banks) {
		if (length != it->getLength()) {
			std::cerr << "Different length banks in signature. Name is based on"
								<< " only first bank" << std::endl;
		}
	}

//FIXME: Using old naming style which implies banks with same length, for
// compatibility with instr-test. Update both together when needed.
	ss << "BankedSignature_" << numBanks << "x" << numBitsEl * length;
	return ss.str();
}

///=============================================================================

LibCallSignature::LibCallSignature() {}

Value* LibCallSignature::allocateLocal(IRBuilder<> Builder) {
	Value *AI = Builder.CreateAlloca(Builder.getInt32Ty(),
																	 Builder.getInt32(64), "Signature_2K");
	Builder.CreateMemSet(AI, Builder.getInt8(0), Builder.getInt64(64), 4);
	return AI;
}

Value* LibCallSignature::allocateGlobal(IRBuilder<> Builder) {
	ArrayType *AT = ArrayType::get(Builder.getInt32Ty(), 64);
	GlobalVariable *GV = new GlobalVariable(AT, false,
			GlobalValue::ExternalLinkage, Constant::getNullValue(AT));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Value *index[2];
	index[0] = Builder.getInt32(0);
	index[1] = Builder.getInt32(0);
	ArrayRef<Value*> indices(index);
	Value *gep = Builder.CreateGEP(GV, indices);
	return gep;
}

void LibCallSignature::insertPointer(IRBuilder<> Builder,
																		 Value *Sign, Value *V) {
	Constant* SetInsertFn;
	std::stringstream InsertValFnName;
	InsertValFnName << "Insert_Value_2K";

	Module *M = Builder.GetInsertBlock()->getParent()->getParent();

	SetInsertFn = M->getOrInsertFunction(InsertValFnName.str(),
			Builder.getVoidTy(), Sign->getType(), V->getType(), (Type*) 0);
	Value *gep;
	/*if (isa<GlobalVariable>(Sign))
	 {
	 Value *index[2];
	 index[0] = Builder.getInt32(0);
	 index[1] = Builder.getInt32(0);
	 ArrayRef<Value*> indices(index);
	 gep = Builder.CreateGEP(Sign,indices);
	 }
	 else*/
	{
		gep = Sign;
	}

	Value * args[2] = { gep, V };
	ArrayRef<Value*> Args(args);
	Builder.CreateCall(SetInsertFn, Args);
}

Value* LibCallSignature::checkMembership(IRBuilder<> Builder,
																				 Value *Sign, Value *V) {
	//const Type *LoadTy = V->getType();
	Type *LoadTy = V->getType();
	Type *IntTy = Builder.getInt32Ty();
	Constant* MembCheckFn;
	std::stringstream ss;
	ss << "MembershipCheck_2K";

	Module *M = Builder.GetInsertBlock()->getParent()->getParent();
	std::vector<Type *> type_vect;
	type_vect.push_back(LoadTy);
	type_vect.push_back(Sign->getType());
	type_vect.push_back((Type *) 0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(IntTy, typeArray, 0);
	MembCheckFn = M->getOrInsertFunction(StringRef(ss.str()), funcType);
	/*
	 MembCheckFn = M->getOrInsertFunction(ss.str(),
	 IntTy,
	 LoadTy,
	 Sign->getType(),
	 (Type*)0);
	 */
	Value *gep;
	/*if (isa<GlobalVariable>(Sign))
	 {
	 Value *index[2];
	 index[0] = Builder.getInt32(0);
	 index[1] = Builder.getInt32(0);
	 ArrayRef<Value*> indices(index);
	 gep = Builder.CreateGEP(Sign,indices);
	 }
	 else*/
	{
		gep = Sign;
	}

	Value* Args[2] = { V, gep };
	ArrayRef<Value*> args(Args);
	std::stringstream call;
	call << "MembCheck_Set"; // << SetNum;
	return Builder.CreateCall(MembCheckFn, args);
}

Type *LibCallSignature::getSignatureType() {
	return PointerType::get(Type::getInt32Ty(getGlobalContext()), 0);
}

std::string LibCallSignature::getName() {
	return std::string("DDPLibraryRuntime_2K");
}

///=======================================================================

Value* PerfectSet::allocateLocal(IRBuilder<> Builder) {
	return allocateHeap(Builder);
}

Value* PerfectSet::allocateGlobal(IRBuilder<> Builder) {
	return allocateHeap(Builder);
}

Value* PerfectSet::allocateHeap(IRBuilder<> Builder) {
	Module *M = Builder.GetInsertBlock()->getParent()->getParent();
	std::vector<Type *> vect;
	ArrayRef<Type *> typeArray(vect);
	FunctionType *funcType = FunctionType::get(
			Type::getInt32PtrTy(Builder.getContext()), typeArray, 0);

	// Call Get_New_Set to get a new set for this function
	Constant* GetNewSetFn = M->getOrInsertFunction("Get_New_Set", funcType);
	ArrayRef<Value*> args; // no args
	return Builder.CreateCall(GetNewSetFn, args);
}

void PerfectSet::insertPointer(IRBuilder<> Builder, Value *Signature,
		Value *V) {
	//Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	//const Type *ValueTy = V->getType();
	Type *ValueTy = V->getType();
	Constant* SetInsertFn;
	Type* VoidTy = Builder.getVoidTy();
	std::stringstream InsertValueBuf;
	InsertValueBuf << "PerfectSet_Insert_Value";
	std::vector<Type *> type_vect;
	type_vect.push_back(Signature->getType());
	type_vect.push_back(ValueTy);
	//type_vect.push_back((Type *)0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(VoidTy, typeArray, 0);
	SetInsertFn = M->getOrInsertFunction(StringRef(InsertValueBuf.str()), funcType);

	/*
	 SetInsertFn = M->getOrInsertFunction(InsertValueBuf.str(),
	 VoidTy, // Return should be NULL actually
	 Signature->getType(),
	 ValueTy,
	 (Type*)0);
	 */
	std::vector<Value *> Args(2);
	Args[0] = Signature;
	Args[1] = V;
	Builder.CreateCall(SetInsertFn, ArrayRef<Value*>(Args));
}

Value* PerfectSet::checkMembership(IRBuilder<> Builder,
																	 Value *Signature,	Value *V) {
	//Module *M = Builder.GetInsertBlock()->getModule();
	std::vector<Type *> type_vect;
	type_vect.push_back(V->getType());
	//type_vect.push_back(ValueTy);
	type_vect.push_back(Signature->getType());
	//type_vect.push_back((Type *)0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(Type::getInt32Ty(
																			Builder.getContext()), typeArray, 0);
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	Constant* MembCheckFn = M->getOrInsertFunction(
																	"PerfectSet_MembershipCheck", funcType);

	std::vector<Value *> Args(2);
	// Now, get the Args into a vector
	Args[0] = V;
	Args[1] = Signature;
	ArrayRef<Value*> args(Args);

	return Builder.CreateCall(MembCheckFn, args);
}

Value* PerfectSet::getSignatureInfo(sigInfoType infoType,
																		IRBuilder<> Builder, Value *Signature,
																		Value *V /* = nullptr */) {
	if (infoType == population) {
		std::vector<Type *> type_vect;
		type_vect.push_back(Signature->getType());
		ArrayRef<Type *> typeArray(type_vect);
		FunctionType *funcType = FunctionType::get(Type::getInt32Ty(
																		 Builder.getContext()), typeArray, 0);
		Module *M =
				(Module*) Builder.GetInsertBlock()->getParent()->getParent();
		Constant* MembCheckFn = M->getOrInsertFunction(
																				"PerfectSet_Population", funcType);
		std::vector<Value *> Args(1);
		Args[0] = Signature;
		ArrayRef<Value*> args(Args);
		return Builder.CreateCall(MembCheckFn, args);
	} else {
		return Builder.getInt32(0);
	}
}

void PerfectSet::freeSet(IRBuilder<> Builder, Value *Signature) {
	std::vector<Type *> type_vect;
	type_vect.push_back(Signature->getType());
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(Type::getVoidTy(
																			Builder.getContext()), typeArray, 0);

	//  Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	// Call Get_New_Set to get a new set for this function
	Constant* FreeSetFn = M->getOrInsertFunction("Free_Set", funcType);

	std::vector<Value*> Args(1);
	Args[0] = Signature;
	ArrayRef<Value*> args(Args);
	Builder.CreateCall(FreeSetFn, args);
}

Type *PerfectSet::getSignatureType() {
	return PointerType::get(Type::getIntNTy(getGlobalContext(), 32), 0);
}

std::string PerfectSet::getName() {
	return std::string("DDPPerfectSet");
}

/// RangeAndBankedSignature ==============================================

RangeAndBankedSignature::RangeAndBankedSignature(int nBanks, int numBitsEl,
		int length) :
		bankSig(nBanks, numBitsEl, length), internalType(NULL) {
	//Nothing to do here. Just initializer lists.
}

void RangeAndBankedSignature::setInternalType(Module* M) {
	Type *elType = bankSig.getElementType();
	unsigned long int totalLength = bankSig.getTotalLength();
	assert(elType->isIntegerTy());
	std::string name = "RangeAndBankedSignature_i"
			+ std::to_string(elType->getIntegerBitWidth()) + "x"
			+ std::to_string(totalLength) + "_struct";
	internalType = M->getTypeByName(name);
	if (internalType == NULL) {
		Type *Int32 = IntegerType::get(M->getContext(), 32);
		Type *types[3];
		types[0] = Int32;
		types[1] = Int32;
		types[2] = ArrayType::get(elType, totalLength);
		ArrayRef<Type*> typeArray(types, 3);
		internalType = StructType::create(typeArray, name);
	}
}

Value* RangeAndBankedSignature::allocateLocal(IRBuilder<> Builder) {
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	setInternalType(M);
	assert(internalType != NULL);
	Value *sig = Builder.CreateAlloca(internalType, Builder.getInt32(1), "range");
	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Builder.CreateStore(Builder.getInt32(-1), Builder.CreateGEP(sig, indices));
	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Builder.CreateStore(Builder.getInt32(0), Builder.CreateGEP(sig, indices2));
	Value *index3[2] = { Builder.getInt32(0), Builder.getInt32(2) };
	ArrayRef<Value*> indices3(index3);
	Value * gep = Builder.CreateGEP(sig, indices3);
	assert(Builder.getInt32Ty() == bankSig.getElementType());
						 // Currently the below memset will work only for int 32 types.
	Builder.CreateMemSet(gep, Builder.getInt8(0),
			Builder.getInt64(bankSig.getTotalLength() * 4), 4);

//  return Builder.CreateBitCast(sig,getSignatureType());
	return sig;
}

Value* RangeAndBankedSignature::allocateGlobal(IRBuilder<> Builder) {
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	setInternalType(M);
	assert(internalType != NULL);
	ArrayType *AT = ArrayType::get(internalType, 1);
	GlobalVariable *GV = new GlobalVariable(AT, false,
			GlobalValue::ExternalLinkage, Constant::getNullValue(AT));
	M->getGlobalList().push_back(GV);
	Value *gindex[2];
	gindex[0] = Builder.getInt32(0);
	gindex[1] = Builder.getInt32(0);
	ArrayRef<Value*> gindices(gindex);
	Value *sig = Builder.CreateGEP(GV, gindices);
	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Builder.CreateStore(Builder.getInt32(-1), Builder.CreateGEP(sig, indices));
	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Builder.CreateStore(Builder.getInt32(0), Builder.CreateGEP(sig, indices2));
	Value *index3[2] = { Builder.getInt32(0), Builder.getInt32(2) };
	ArrayRef<Value*> indices3(index3);
	Value * gep = Builder.CreateGEP(sig, indices3);
	assert(Builder.getInt32Ty() == bankSig.getElementType());
							//Currently the below memset will work only for int 32 types.
	Builder.CreateMemSet(gep, Builder.getInt8(0),
			Builder.getInt64(bankSig.getTotalLength() * 4), 4);
	return sig;
}

void RangeAndBankedSignature::insertPointer(IRBuilder<> Builder,
																					  Value *Signature, Value *V) {
	Value* addr = Builder.CreatePtrToInt(V, Builder.getInt32Ty());
	assert(internalType != NULL);
// Value* sig = Builder.CreateBitCast(Signature,internalType->getPointerTo(),"castedSig");
	Value * sig = Signature;

	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Value *minPtr = Builder.CreateGEP(sig, indices);
	Value *minVal = Builder.CreateLoad(minPtr);
	Value *minCmp = Builder.CreateICmpULT(addr, minVal, "minCmp");
	minVal = Builder.CreateSelect(minCmp, addr, minVal);
	Builder.CreateStore(minVal, minPtr);

	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Value *maxPtr = Builder.CreateGEP(sig, indices2);
	Value *maxVal = Builder.CreateLoad(maxPtr);
	Value *maxCmp = Builder.CreateICmpUGT(addr, maxVal, "maxCmp");
	maxVal = Builder.CreateSelect(maxCmp, addr, maxVal);
	Builder.CreateStore(maxVal, maxPtr);

	Value *index3[2] = { Builder.getInt32(0), Builder.getInt32(2) };
	ArrayRef<Value*> indices3(index3);
	Value *bankSignPtr = Builder.CreateBitCast(Builder.CreateGEP(sig, indices3),
			bankSig.getSignatureType());

	bankSig.insertPointer(Builder, bankSignPtr, V);
}

Value* RangeAndBankedSignature::checkMembership(IRBuilder<> Builder,
																								Value *Signature, Value *V) {
	Value* addr = Builder.CreatePtrToInt(V, Builder.getInt32Ty());
	assert(internalType != NULL);
//   Value* sig = Builder.CreateBitCast(Signature,internalType->getPointerTo(),"castedSig");
	Value * sig = Signature;

	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Value *minVal = Builder.CreateLoad(Builder.CreateGEP(sig, indices));
	Value* rangeCheckResult = Builder.CreateICmpUGE(addr, minVal, "minCmp");

	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Value *maxVal = Builder.CreateLoad(Builder.CreateGEP(sig, indices2));
	rangeCheckResult = Builder.CreateAnd(
			Builder.CreateICmpULE(addr, maxVal, "maxCmp"), rangeCheckResult,
			"andMinMax");
	//Value* rangeCheckResult32 = Builder.CreateZExt(rangeCheckResult,Builder.getInt32Ty());

	BasicBlock *Old = Builder.GetInsertBlock();
	BasicBlock *split = Old->splitBasicBlock(Builder.GetInsertPoint(),
			Old->getName() + ".split");
	BasicBlock *newBB = BasicBlock::Create(Builder.getContext(), "",
																				 Old->getParent(), split);

	// splitBasicBlock puts in a terminator for us (argh!) so we must remove it!
	Old->getTerminator()->eraseFromParent();

	IRBuilder<> OB(Old);
	OB.CreateCondBr(rangeCheckResult, newBB, split);

	// only evaluate check if so far it has come out to zero

	IRBuilder<> NB(newBB);
	Value *index3[2] = { NB.getInt32(0), NB.getInt32(2) };
	ArrayRef<Value*> indices3(index3);
	Value *bankSignPtr = NB.CreateBitCast(NB.CreateGEP(sig, indices3),
			bankSig.getSignatureType());
	Value* sigCheck = bankSig.checkMembership(NB, bankSignPtr, V);
	NB.CreateBr(split);

	IRBuilder<> SB(split, split->begin());
	PHINode *phi = SB.CreatePHI(SB.getInt32Ty(), 2);
	phi->addIncoming(SB.getInt32(0), Old);
	phi->addIncoming(sigCheck, newBB);

	//Old->dump();
	//newBB->dump();
	//split->dump();

	return phi;
}

Value* RangeAndBankedSignature::getSignatureInfo(sigInfoType infoType,
		IRBuilder<> Builder, Value *Signature, Value *V /* = nullptr */) {
	return bankSig.getSignatureInfo(infoType, Builder, Signature, V);
}

Type *RangeAndBankedSignature::getSignatureType() {
	assert(internalType != NULL);
	return internalType;
}

std::string RangeAndBankedSignature::getName() {
	return std::string("RangeAnd" + bankSig.getName());
}

/// RangeSet =============================================================

StructType* RangeSet::internalType = NULL;

RangeSet::RangeSet() {
	if (internalType == NULL) {
		std::string name = "RangeSet_struct";
		Type *Int32 = IntegerType::get(getGlobalContext(), 32);
		Type *types[3];
		types[0] = Int32;
		types[1] = Int32;
		ArrayRef<Type*> typeArray(types, 2);
		internalType = StructType::create(typeArray, name);
	}
}

Value* RangeSet::allocateLocal(IRBuilder<> Builder) {
	assert(internalType);
	Value *sig = Builder.CreateAlloca(internalType, Builder.getInt32(1), "range");
	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Builder.CreateStore(Builder.getInt32(-1), Builder.CreateGEP(sig, indices));
	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Builder.CreateStore(Builder.getInt32(0), Builder.CreateGEP(sig, indices2));
//  return Builder.CreateBitCast(sig,getSignatureType());
	return sig;
}

Value* RangeSet::allocateGlobal(IRBuilder<> Builder) {
	assert(internalType != NULL);
	ArrayType *AT = ArrayType::get(internalType, 1);
	GlobalVariable *GV = new GlobalVariable(AT, false,
			GlobalValue::ExternalLinkage, Constant::getNullValue(AT));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Value *gindex[2];
	gindex[0] = Builder.getInt32(0);
	gindex[1] = Builder.getInt32(0);
	ArrayRef<Value*> gindices(gindex);
	Value *sig = Builder.CreateGEP(GV, gindices);
	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Builder.CreateStore(Builder.getInt32(-1), Builder.CreateGEP(sig, indices));
	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Builder.CreateStore(Builder.getInt32(0), Builder.CreateGEP(sig, indices2));
	return sig;
}

void RangeSet::insertPointer(IRBuilder<> Builder, Value *Signature, Value *V) {
	Value* addr = Builder.CreatePtrToInt(V, Builder.getInt32Ty());
	assert(internalType != NULL);
// Value* sig = Builder.CreateBitCast(Signature,internalType->getPointerTo(),"castedSig");

	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Value *minPtr = Builder.CreateGEP(Signature, indices);
	Value *minVal = Builder.CreateLoad(minPtr);
	Value *minCmp = Builder.CreateICmpULT(addr, minVal, "minCmp");
	minVal = Builder.CreateSelect(minCmp, addr, minVal);
	Builder.CreateStore(minVal, minPtr);

	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Value *maxPtr = Builder.CreateGEP(Signature, indices2);
	Value *maxVal = Builder.CreateLoad(maxPtr);
	Value *maxCmp = Builder.CreateICmpUGT(addr, maxVal, "maxCmp");
	maxVal = Builder.CreateSelect(maxCmp, addr, maxVal);
	Builder.CreateStore(maxVal, maxPtr);
}

Value* RangeSet::checkMembership(IRBuilder<> Builder,
																 Value *Signature, Value *V) {
	Value* addr = Builder.CreatePtrToInt(V, Builder.getInt32Ty());
	assert(internalType != NULL);
// Value* sig = Builder.CreateBitCast(Signature,internalType->getPointerTo(),"castedSig");

	Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
	ArrayRef<Value*> indices(index);
	Value *minVal = Builder.CreateLoad(Builder.CreateGEP(Signature, indices));
	Value* rangeCheckResult = Builder.CreateICmpUGE(addr, minVal, "minCmp");

	Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
	ArrayRef<Value*> indices2(index2);
	Value *maxVal = Builder.CreateLoad(Builder.CreateGEP(Signature, indices2));
	rangeCheckResult = Builder.CreateAnd(Builder.CreateICmpULE(
												addr, maxVal, "maxCmp"), rangeCheckResult, "andMinMax");
	return Builder.CreateZExt(rangeCheckResult, Builder.getInt32Ty());
}

Value* RangeSet::getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
		Value *Signature, Value *V /* = nullptr */) {
	if (infoType == population) {
	//Instead of reporting the range, let's report the difference of max and min.
	// This will be a good reflective of population in range set.

// Value* sig = Builder.CreateBitCast(Signature,internalType->getPointerTo(),"castedSig");

		Value *index[2] = { Builder.getInt32(0), Builder.getInt32(0) };
		ArrayRef<Value*> indices(index);
		Value *minVal = Builder.CreateLoad(Builder.CreateGEP(Signature, indices));
		Value *index2[2] = { Builder.getInt32(0), Builder.getInt32(1) };
		ArrayRef<Value*> indices2(index2);
		Value *maxVal = Builder.CreateLoad(Builder.CreateGEP(Signature, indices2));
		return Builder.CreateSub(maxVal, minVal);
	} else {
		return Builder.getInt32(0);
	}
}

Type *RangeSet::getSignatureType() {
	return internalType;
}

std::string RangeSet::getName() {
	return std::string("RangeSet");
}

/// RangeSet =============================================================

Value* RangeSetLibCall::allocateLocal(IRBuilder<> Builder) {
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	Type *Int32 = IntegerType::get(Builder.getContext(), 32);
	std::string name = "RangeSet_struct";
	StructType *mystruct = M->getTypeByName(name);
	if (mystruct == NULL) {
		Type *types[2];
		types[0] = Int32;
		types[1] = Int32;
		ArrayRef<Type*> typeArray(types, 2);
		mystruct = StructType::create(typeArray, name);
	}
	Value *AI = Builder.CreateAlloca(mystruct, Builder.getInt32(1), "range");
	//  Builder.CreateMemSet(AI,Builder.getInt8(0),Builder.getInt64(8),4);
	Value *index[2];
	index[0] = Builder.getInt32(0);
	index[1] = Builder.getInt32(0);
	ArrayRef<Value*> indices(index);
	Value * gep = Builder.CreateGEP(AI, indices);
	Builder.CreateStore(Builder.getInt32(-1), gep);
	Value *index2[2];
	index2[0] = Builder.getInt32(0);
	index2[1] = Builder.getInt32(1);
	ArrayRef<Value*> indices2(index2);
	Builder.CreateStore(Builder.getInt32(0), Builder.CreateGEP(AI, indices2));
	return Builder.CreateBitCast(gep, getSignatureType());
}

Value* RangeSetLibCall::allocateGlobal(IRBuilder<> Builder) {
	assert(0 && "RangetSetLibCall::allocateGlobal not yet implemented");
	return allocateHeap(Builder);
}

Value* RangeSetLibCall::allocateHeap(IRBuilder<> Builder) {
	Module *M = Builder.GetInsertBlock()->getParent()->getParent();

// Call Get_New_Set to get a new set for this function
	Constant* GetNewSetFn = M->getOrInsertFunction("RangeSet_New",
			Type::getInt32PtrTy(Builder.getContext()), // return a void pointer
			(Type*) 0);

	ArrayRef<Value*> args; // no args
	return Builder.CreateCall(GetNewSetFn, args);
}

void RangeSetLibCall::insertPointer(IRBuilder<> Builder, Value *Signature,
		Value *V) {
	//Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	//const Type *ValueTy = V->getType();
	Type *ValueTy = V->getType();

	Constant* SetInsertFn;
	Type* VoidTy = Builder.getVoidTy();

	std::stringstream InsertValueBuf;
	InsertValueBuf << "RangeSet_Insert_Value";

	std::vector<Type *> type_vect;
	type_vect.push_back(Signature->getType());
	type_vect.push_back(ValueTy);
	type_vect.push_back((Type *) 0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(VoidTy, typeArray, 0);
	SetInsertFn = M->getOrInsertFunction(StringRef(InsertValueBuf.str()),
			funcType);

	/*
	 SetInsertFn = M->getOrInsertFunction(InsertValueBuf.str(),
	 VoidTy, // Return should be NULL actually
	 Signature->getType(),
	 ValueTy,
	 (Type*)0);
	 */

	std::vector<Value *> Args(2);
	Args[0] = Signature;
	Args[1] = V;
	ArrayRef<Value*> args(Args);

	Builder.CreateCall(SetInsertFn, args);
}

Value* RangeSetLibCall::checkMembership(IRBuilder<> Builder, Value *Signature,
		Value *V) {
	std::stringstream MembershipCheckBuf;
	MembershipCheckBuf << "RangeSet_MembershipCheck";

	//Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();

	std::vector<Type *> type_vect;
	type_vect.push_back(V->getType());
	type_vect.push_back(Signature->getType());
	type_vect.push_back((Type *) 0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(Builder.getInt32Ty(), typeArray,
			0);
	Constant* MembCheckFn = M->getOrInsertFunction(
			StringRef(MembershipCheckBuf.str()), funcType);

	/*
	 Constant* MembCheckFn = M->getOrInsertFunction(MembershipCheckBuf.str(),
	 Builder.getInt32Ty(),
	 V->getType(),
	 Signature->getType(),
	 (Type*)0);
	 */

	std::vector<Value *> Args(2);
	// Now, get the Args into a vector
	Args[0] = V;
	Args[1] = Signature;
	ArrayRef<Value*> args(Args);

	return Builder.CreateCall(MembCheckFn, args);
}

void RangeSetLibCall::freeSet(IRBuilder<> Builder, Value *Signature) {
	//  Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	// Call Get_New_Set to get a new set for this function
	Constant* FreeSetFn = M->getOrInsertFunction("RangeSet_Free",
			Builder.getVoidTy(), // return NULL
			Type::getInt32PtrTy(Builder.getContext()),
			// set pointer
			(Type*) 0);

	std::vector<Value*> Args(1);
	Args[0] = Signature;
	ArrayRef<Value*> args(Args);
	Builder.CreateCall(FreeSetFn, args);
}

Type *RangeSetLibCall::getSignatureType() {
	return PointerType::get(Type::getIntNTy(getGlobalContext(), 32), 0);
}

std::string RangeSetLibCall::getName() {
	return std::string("DDPRangeSetLibCall");
}

///=== DumpSet ===========================================================

void DumpSet::allocateDump(IRBuilder<> Builder) {
	Module *M = Builder.GetInsertBlock()->getParent()->getParent();

	// Call Get_New_Set to get a new set for this function
	Constant* DumpSetFn = M->getOrInsertFunction("DumpSet_Init",
			Type::getVoidTy(Builder.getContext()), // return a void pointer
			Type::getInt32Ty(Builder.getContext()), (Type*) 0);

	std::vector<Value*> v(1);
	v[0] = Builder.getInt32(refid);
	ArrayRef<Value*> args(v); // no args
	Builder.CreateCall(DumpSetFn, args);
}

Value* DumpSet::allocateLocal(IRBuilder<> Builder) {
	allocateDump(Builder);
	return set->allocateLocal(Builder);
}

Value* DumpSet::allocateGlobal(IRBuilder<> Builder) {
	allocateDump(Builder);
	return set->allocateGlobal(Builder);
}

Value* DumpSet::allocateHeap(IRBuilder<> Builder) {
	allocateDump(Builder);
	return set->allocateHeap(Builder);
}

void DumpSet::insertPointer(IRBuilder<> Builder, Value *Signature, Value *V) {
	set->insertPointer(Builder, Signature, V);

	//Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	//const Type *ValueTy = V->getType();
	Type *ValueTy = V->getType();

	Constant* SetInsertFn;
	Type* VoidTy = Builder.getVoidTy();

	std::stringstream InsertValueBuf;
	InsertValueBuf << "DumpSet_Insert_Value";

	std::vector<Type *> type_vect;
	type_vect.push_back(Builder.getInt32Ty());
	type_vect.push_back(ValueTy);
	type_vect.push_back((Type *) 0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType = FunctionType::get(VoidTy, typeArray, 0);
	SetInsertFn = M->getOrInsertFunction(StringRef(InsertValueBuf.str()),
			funcType);

	/*
	 SetInsertFn = M->getOrInsertFunction(InsertValueBuf.str(),
	 VoidTy, // Return should be NULL actually
	 Builder.getInt32Ty(),
	 ValueTy,
	 (Type*)0);
	 */

	std::vector<Value *> Args(2);
	Args[0] = Builder.getInt32(refid);
	Args[1] = V;
	ArrayRef<Value*> args(Args);
	Builder.CreateCall(SetInsertFn, args);
}

Value* DumpSet::checkMembership(IRBuilder<> Builder,
																Value *Signature, Value *V) {
	Value *ret = set->checkMembership(Builder, Signature, V);
	BasicBlock::iterator it(cast < Instruction > (ret));
	it++;
	Builder.SetInsertPoint(it->getParent(), it);

	std::stringstream MembershipCheckBuf;
	MembershipCheckBuf << "DumpSet_MembershipCheck";

	//Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();

	std::vector<Type *> type_vect;
	type_vect.push_back(V->getType());
	type_vect.push_back(Builder.getInt32Ty());
	type_vect.push_back(ret->getType());
	type_vect.push_back((Type *) 0);
	ArrayRef<Type *> typeArray(type_vect);
	FunctionType *funcType =
										FunctionType::get(Builder.getInt32Ty(), typeArray, 0);
	Constant* MembCheckFn = M->getOrInsertFunction(
														StringRef(MembershipCheckBuf.str()), funcType);

	/*
	 Constant* MembCheckFn = M->getOrInsertFunction(MembershipCheckBuf.str(),
	 Builder.getInt32Ty(),
	 V->getType(),
	 Builder.getInt32Ty(),
	 ret->getType(),
	 (Type*)0);
	 */

	std::vector<Value *> Args(3);
	// Now, get the Args into a vector
	Args[0] = V;
	Args[1] = Builder.getInt32(refid);
	Args[2] = ret;
	ArrayRef<Value*> args(Args);

	std::stringstream TempBuf;
	TempBuf << "MembCheck_Set_";
	Builder.CreateCall(MembCheckFn, args);
	return ret;
}

void DumpSet::freeSet(IRBuilder<> Builder, Value *Signature) {
	set->freeSet(Builder, Signature);

	//  Module *M = Builder.GetInsertBlock()->getModule();
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	// Call Get_New_Set to get a new set for this function
	Constant* FreeSetFn = M->getOrInsertFunction("DumpSet_Free",
			Builder.getVoidTy(), // return NULL
			Type::getInt32Ty(Builder.getContext()),
			// set pointer
			(Type*) 0);

	std::vector<Value*> Args(1);
	Args[0] = Builder.getInt32(refid);
	ArrayRef<Value*> args(Args);
	Builder.CreateCall(FreeSetFn, args);
}

Type *DumpSet::getSignatureType() {
	return set->getSignatureType();
}

std::string DumpSet::getName() {
	std::stringstream temp;
	temp << "DumpSet_" << set->getName();
	return temp.str();
}

///=======================================================================

#define HASH_TABLE_SIZE 50000

//cl::opt<unsigned>
//TableSize("tablesize", cl::Hidden,
//			 cl::desc("Hash Table Size"), cl::init(HASH_TABLE_SIZE));
unsigned TableSize = HASH_TABLE_SIZE;

Value* HashTableSet::allocateLocal(IRBuilder<> Builder) {
	Type *IntTy = Builder.getInt32Ty();
	size_t s = TableSize / sizeof(unsigned);
	Value *VecVal = ConstantInt::get(IntTy, s, false);
	Value *HashPtr = Builder.CreateAlloca(IntTy, VecVal, "HashTable");
	Builder.CreateMemSet(HashPtr, Builder.getInt8(0), Builder.getInt64(s), 4);
	return HashPtr;
}

Value* HashTableSet::allocateGlobal(IRBuilder<> Builder) {
	Type *IntTy = Builder.getInt32Ty();
	ArrayType *AT = ArrayType::get(IntTy, TableSize / sizeof(unsigned));
	GlobalVariable *GV = new GlobalVariable(AT, false,
																					GlobalValue::ExternalLinkage,
																					Constant::getNullValue(AT));
	BasicBlock *BB = Builder.GetInsertBlock();
	Module *M = BB->getParent()->getParent();
	M->getGlobalList().push_back(GV);
	Value *index[2];
	index[0] = Builder.getInt32(0);
	index[1] = Builder.getInt32(0);
	ArrayRef<Value*> indices(index);
	Value *gep = Builder.CreateGEP(GV, indices);
	Builder.CreateMemSet(gep, Builder.getInt8(0),
											 Builder.getInt64(TableSize / sizeof(unsigned)), 4);
	return gep;
}

Value* HashTableSet::allocateUniqueGlobal(IRBuilder<> Builder) {
	Module *M = Builder.GetInsertBlock()->getParent()->getParent();

	// Call Get_New_Set to get a new set for this function
	Constant* GetNewSetFn = M->getOrInsertFunction("HT_Get_Table",
			Type::getInt32PtrTy(Builder.getContext()), (Type*) 0);

	ArrayRef<Value*> args; // no args
	return Builder.CreateCall(GetNewSetFn, args);
}

Value* HashTableSet::allocateHeap(IRBuilder<> Builder) {
	return allocateLocal(Builder);
}

void HashTableSet::insertPointer(IRBuilder<> Builder,
																 Value *Signature, Value *V) {
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	Constant* HTInsertFn = M->getOrInsertFunction("HT_Insert_Value",
			Builder.getVoidTy(), V->getType(), Signature->getType(), // hash table
			Builder.getInt32Ty(), (Type*) 0);

	std::vector<Value*> Args(3);
	Args[0] = V;
	Args[1] = Signature;
	Args[2] = Builder.getInt32(TableSize);
	ArrayRef<Value*> args(Args);
	Builder.CreateCall(HTInsertFn, args, "");
}

Value* HashTableSet::checkMembership(IRBuilder<> Builder,
																		 Value *Signature, Value *V) {
	Module *M = (Module*) Builder.GetInsertBlock()->getParent()->getParent();
	Constant* HTMembFn = M->getOrInsertFunction("HT_Membership_Check",
			Builder.getInt32Ty(), V->getType(), // load address
			Signature->getType(), // hash table
			Builder.getInt32Ty(), (Type*) 0);

	std::vector<Value*> Args(3);
	Args[0] = V;
	Args[1] = Signature;
	Args[2] = Builder.getInt32(TableSize);
	ArrayRef<Value*> args(Args);

	//std::stringstream buf3;
	//buf3 << "MembCheck_Load_" << Q.id;
	return Builder.CreateCall(HTMembFn, args);
}

void HashTableSet::freeSet(IRBuilder<> Builder, Value *Signature) {
	// nothing needed since it's an alloca
}

Type *HashTableSet::getSignatureType() {
	return PointerType::get(Type::getIntNTy(getGlobalContext(), 32), 0);
}

std::string HashTableSet::getName() {
	return std::string("DDPHashTableSet");
}

///=======================================================================

SImple *SImpleFactory::CreatePerfectSet() {
	return new PerfectSet();
}

SImple *SImpleFactory::CreateRangeSet() {
	return new RangeSet();
}

SImple *SImpleFactory::CreateHashTableSet() {
	return new HashTableSet();
}

SImple *SImpleFactory::CreateFastSignature(unsigned int bits) {
	SImple *S;
	if (bits <= 32) {
		S = new SimpleSignature(32,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0x1F));
	} else if (bits <= 64) {
		S = new SimpleSignature(64,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0x3F));
	} else if (bits <= 128) {
		S = new SimpleSignature(128,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0x7F));
	} else if (bits <= 256) {
		S = new SimpleSignature(256,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0xFF));
	} else if (bits <= 512) {
		S = new ArraySignature(32, 16,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0x1FF));
	} else if (bits <= 1024) {
		S = new BankedSignature(2, 32, 16);
	} else if (bits <= 2048) {
		S = new ArraySignature(32, 64,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0x7FF));
		//S = new BankedSignature(2,32,32);
	} else {
		//S = new BankedSignature(3,32,32);
		// requesting really big signature
		S = new ArraySignature(32, 128,
				HashBuilderFactory::CreateShiftMaskIndex(2, 0xFFF));
	}
	return S;
}

SImple *SImpleFactory::CreateDynStructSignature(unsigned int bits,
		unsigned int structSize) {
	SImple *S;
	assert(bits > 512 && "Struct dynamic signature requires 1024 bits or higher \
																											for banked signature.");
	int nBanks, bankSize;
	const int nBitsPerEl = 32;
	if (bits <= 1024) {
		nBanks = 2;
		bankSize = 16;
	} else if (bits <= 2048) {
		nBanks = 2;
		bankSize = 32;
	} else if (bits <= 3072) {
		nBanks = 3;
		bankSize = 32;
	} else if (bits <= 4096) {
		nBanks = 2;
		bankSize = 64;
	} else {
		// FIXME: do something smarter here. Requesting really big signature
		nBanks = 2;
		bankSize = 128;
	}

	std::vector<int> lengths = std::vector<int>(nBanks, bankSize);

	int tot = nBitsPerEl * bankSize;
	int targetlevel = 0;
	while (tot >>= 1)
		++targetlevel;
	int offset = 0; //We don't need to offset 4 bits for int size any more
	int mask = (1 << targetlevel) - 1;

	std::vector < HashBuilder > hashes;
	for (int i = 0; i < nBanks; i++) {
		hashes.push_back(
				HashBuilderFactory::CreateXorIndex(offset, mask,
						Instruction::BinaryOps::UDiv, structSize));
		offset += targetlevel;
	}

// Create additional small bank for structure specific distinguishing.
// Size of it would be ceil(structSize/32).
	if (structSize > 256 * 1024) {
		std::cerr << "DDP WARN: A struct size larger than 256K is not supported \
															in current hash. Please fix code." << std::endl;
	}
	int extraBankSize = std::ceil(structSize / 32.0);
	if (extraBankSize > 8) {
		extraBankSize = 8;
	}
	lengths.push_back(static_cast<int>(extraBankSize));
	auto lambdaHash = [=](IRBuilder<> Builder, Value* V) -> Value* {
		V = Builder.CreatePtrToInt(V,Builder.getInt32Ty());
		V = Builder.CreateURem(V,Builder.getInt32(structSize));
		if(structSize > 256) {
			if(structSize > 256*4) {
				//NOTE: Assumption is that struct is not bigger than 256k
				//Xor upper and lower half, discarding lower two bits
			Value *upperHalf = Builder.CreateLShr(Builder.CreateAnd(
															Builder.getInt32(0x3FC00),V),Builder.getInt32(8));
			V = Builder.CreateXor(upperHalf,V);
		}

	//Just shift by 4 because probably most of the struct elements will be 4 bytes wide.
		V = Builder.CreateLShr(V,Builder.getInt32(2));
	}
	return V;
}	;
	hashes.push_back(lambdaHash);

	S = new BankedSignature(nBitsPerEl, lengths, hashes);
	return S;
}

SImple *SImpleFactory::CreateHybridSignature(unsigned int bits) {
	SImple *S;
	assert(bits > 512 &&
		"Hybrid signature not defined with Simple Signature yet. \
								Use 1024 bits or higher for banked signature.");
	if (bits <= 1024) {
		S = new RangeAndBankedSignature(2, 32, 16);
	} else if (bits <= 2048) {
		S = new RangeAndBankedSignature(2, 32, 32);
	} else if (bits <= 3072) {
		S = new RangeAndBankedSignature(3, 32, 32);
	} else if (bits <= 4096) {
		S = new RangeAndBankedSignature(2, 32, 32 * 2);
	} else {
		// FIXME: do something smarter here. Requesting really big signature
		S = new RangeAndBankedSignature(2, 32, 32 * 4);
	}
	return S;
}

SImple *SImpleFactory::CreateAccurateSignature(unsigned int bits) {
	SImple *S;
	if (bits <= 32) {
		S = new SimpleSignature(32,
				HashBuilderFactory::CreateKnuthIndex(2, 0x1F));
	} else if (bits <= 64) {
		S = new SimpleSignature(64,
				HashBuilderFactory::CreateKnuthIndex(2, 0x3F));
	} else if (bits <= 128) {
		S = new SimpleSignature(128,
				HashBuilderFactory::CreateKnuthIndex(2, 0x7F));
	} else if (bits <= 256) {
		S = new SimpleSignature(256,
				HashBuilderFactory::CreateKnuthIndex(2, 0xFF));
	} else if (bits <= 512) {
		S = new ArraySignature(32, 16,
				HashBuilderFactory::CreateKnuthIndex(2, 0x1FF));
	} else if (bits <= 1024) {
		S = new BankedSignature(2, 32, 16);
	} else if (bits <= 2048) {
		S = new BankedSignature(2, 32, 32);
	} else if (bits <= 3072) {
		S = new BankedSignature(3, 32, 32);
	} else if (bits <= 4096) {
		S = new BankedSignature(2, 32, 32 * 2);
	} else {
		// FIXME: do something smarter here
		// requesting really big signature
		S = new BankedSignature(2, 32, 32 * 4);
	}
	return S;
}

SImple *SImpleFactory::CreateLibCallSignature() {
	SImple *S = new LibCallSignature();
	return S;
}
