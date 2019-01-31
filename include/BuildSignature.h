#ifndef BUILDSIGNATURE_H
#define BUILDSIGNATURE_H

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
#include "HashBuilder.h"
#include <cstdio>

using namespace llvm;

// SImple: Other information that signature can provide.
typedef enum {
   population = 0,
   sizeOfEnum //Useful in case we want to loop over these.
} sigInfoType;

// SImple: Set Implementation
class SImple {
public:
  virtual Value* allocateLocal(IRBuilder<> Builder) = 0;
  virtual Value* allocateGlobal(IRBuilder<> Builder) = 0;
  virtual Value* allocateHeap(IRBuilder<> Builder) {
    assert(0 && "Impement support for heap allocation.");
  }

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature,
                                                Value *V) = 0;
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature,
                                                    Value *V) = 0;
  virtual void freeSet(IRBuilder<>,Value*) = 0;

  virtual Type *getSignatureType() = 0;
  virtual std::string getName() = 0;

  virtual Value* getSignatureInfo(sigInfoType infoType,
                                IRBuilder<> Builder, Value *Signature,
                                                     Value *V = nullptr) {
  //If not implemented, always return a constant int 0;
    return Builder.getInt32(0);
  }
};

class SimpleSignature : public SImple {
 private:
  int numBits;
  Type *Ty;

  //  Value *AI;
  HashBuilder hashBuilderLambda;

 public:
  SimpleSignature(int numBits, const HashBuilder &hb);

  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Sign, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Sign, Value *V);

  // do nothing, because we never put signatures on the heap
  virtual void freeSet(IRBuilder<> Builder, Value*) {}

  virtual Type *getSignatureType();
  virtual std::string getName();
};

///
/// ArraySignature is a signature implementation that is too large to
/// fit into a standard iN data type. Theoretically, you could
/// just keeping using SimpleSignature for arbitrarily large signatures
/// using the iN types in LLVM.  But, this requires loading and storing
/// the entire signature on an insert operation, which is not ideal.
/// The ArraySignature fixes that by allocating an array and only updating
/// the relevant element.
///
///
class ArraySignature : public SImple {
 private:
  int numBitsEl;
  int length;
  bool isPow2;
  int pow2;
  int pow2mask;

  Type *ElTy;
  Type *SignTy;

  HashBuilder hashBuilderLambda;

 public:
  ArraySignature(int numBitsEl, int length, const HashBuilder &hb);

  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Sign, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Sign, Value *V);

  // do nothing, because we never put signatures on the heap
  virtual void freeSet(IRBuilder<> Builder, Value *) {}

  virtual Type *getSignatureType();
  virtual std::string getName();
  virtual int getLength() {return length;}
};

class BankedSignature : public SImple {
 private:
  int numBanks;
  int numBitsEl;
  Type *ElTy;
  Type *SignTy;

  std::vector<ArraySignature*> banks;

 public:
  BankedSignature(int nBanks, int numBitsEl, int length);
  BankedSignature(int anumBitsEl, const std::vector<int> &lengths,
                  std::vector<HashBuilder> &hashes);
  virtual ~BankedSignature();

  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Sign, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Sign, Value *V);

// Do nothing, because we never put signatures on the heap
  virtual void freeSet(IRBuilder<> Builder, Value *) {}

  virtual Value* getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
                                  Value *Signature, Value *V = nullptr);

  virtual Type *getSignatureType();

// Below two functions will be used by RangeAndBankedSignatures to create it's
// struct type.
  virtual Type *getElementType() { return ElTy; }
  virtual unsigned long int getTotalLength() {
     unsigned long int totalLength = 0;
     for(auto &it : banks) { totalLength += it->getLength(); }
     return totalLength;
  }

  virtual std::string getName();
};

class LibCallSignature : public SImple {
 public:
  LibCallSignature();

  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Sign, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Sign, Value *V);

  // do nothing, because we never put signatures on the heap
  virtual void freeSet(IRBuilder<> Builder, Value *) {}

  virtual Type *getSignatureType();
  virtual std::string getName();
};

class PerfectSet : public SImple {
 public:
  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);
  virtual Value* allocateHeap(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature);

  virtual Type *getSignatureType();
  virtual std::string getName();

  virtual Value* getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
                                  Value *Signature, Value *V = nullptr);
};

class RangeAndBankedSignature : public SImple {
  BankedSignature bankSig;
  StructType* internalType;
  void setInternalType(Module *M);
 public:
  RangeAndBankedSignature(int nBanks, int numBitsEl, int length);
  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature) {
     bankSig.freeSet(Builder, Signature);
  }

  virtual Value* getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
                                  Value *Signature, Value *V = nullptr);

  virtual Type *getSignatureType();
  virtual std::string getName();
};

class RangeSet : public SImple {
  static StructType* internalType;
 public:
  RangeSet();
  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature){}

  virtual Value* getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
                                  Value *Signature, Value *V = nullptr);

  virtual Type *getSignatureType();
  virtual std::string getName();
};

class RangeSetLibCall : public SImple {
 public:
  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);
  virtual Value* allocateHeap(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature);

  virtual Type *getSignatureType();
  virtual std::string getName();
};

class HashTableSet : public SImple {
 public:
  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);
  virtual Value* allocateHeap(IRBuilder<> Builder);
  virtual Value* allocateUniqueGlobal(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature);

  virtual Type *getSignatureType();
  virtual std::string getName();
};

class DumpSet : public SImple {
 protected:
  SImple *set;
  int refid;

  void allocateDump(IRBuilder<> Builder);

 public:
  DumpSet(SImple *aSet,int arefid): set(aSet),refid(arefid) {}

  virtual Value* allocateLocal(IRBuilder<> Builder);
  virtual Value* allocateGlobal(IRBuilder<> Builder);
  virtual Value* allocateHeap(IRBuilder<> Builder);

  virtual void insertPointer(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual Value* checkMembership(IRBuilder<> Builder, Value *Signature, Value *V);
  virtual void freeSet(IRBuilder<> Builder, Value *Signature);

  virtual Type *getSignatureType();
  virtual std::string getName();
};

template <typename SetType>
class AllocateLocal {
 protected:
  SetType &S;
 public:
  AllocateLocal(SetType &aS): S(aS) {}
  Value * allocate(IRBuilder<> &Builder) {
    return S.allocateLocal(Builder);
  }
  void free(IRBuilder<> Builder, Value * Signature) {}
};

template <typename SetType>
class AllocateGlobal {
 protected:
  SetType &S;
 public:
  AllocateGlobal(SetType &aS): S(aS) {}
  Value * allocate(IRBuilder<> &Builder) {
    return S.allocateGlobal(Builder);
  }
  void free(IRBuilder<> Builder, Value * Signature) {}
};

template <typename SetType>
class AllocateUniqueGlobal {
 protected:
  SetType &S;
 public:
  AllocateUniqueGlobal(SetType &aS): S(aS) {}
  Value * allocate(IRBuilder<> &Builder) {
    return S.allocateUniqueGlobal(Builder);
  }
  void free(IRBuilder<> Builder, Value* Signature) {}
};

template <typename SetType>
class AllocateHeap {
 protected:
  SetType &S;
 public:
  AllocateHeap(SetType &aS): S(aS) {}
  Value * allocate(IRBuilder<> &Builder) {
    return S.allocateHeap(Builder);
  }
  void free(IRBuilder<> Builder, Value *Signature) {
    S.freeSet(Builder,Signature);
  }
};

template
<
    // Decide where sets should be allocated in memory
  typename SetImpl = SImple,
  typename AllocatePolicy = AllocateLocal<SImple>
>
class SImpleWrapper
{
 protected:
   AllocatePolicy Alloc;
   SetImpl &S;
   Value * Sign;
 public:
 SImpleWrapper(SetImpl &aS)
 :Alloc(aS),S(aS),Sign(NULL) {}

 void allocate(IRBuilder<> Builder) {
   Sign = Alloc.allocate(Builder);
 }
 void insertPointer(IRBuilder<> Builder, Value *V) {
   S.insertPointer(Builder,Sign,V);
 }
 Value* checkMembership(IRBuilder<> Builder, Value *V) {
   return S.checkMembership(Builder,Sign,V);
 }
 Value* getSignatureInfo(sigInfoType infoType, IRBuilder<> Builder,
                                              Value *V = nullptr) {
   return S.getSignatureInfo(infoType,Builder,Sign,V);
 }
 void freeSet(IRBuilder<> Builder) {
   Alloc.free(Builder,Sign);
 }
 Type *getSignatureType() { return S.getSignatureType(); }
 std::string getName() { return S.getName(); }

 SetImpl &getSetImpl() { return S; }
};

///
/// This class is a convenient wrapper class for use during
/// code generation. It knows how to interface with a SetInstrument
/// class type and it provides the interface expected by the Instrumentation
/// pass in setprof library.
///
///
template
<
    // Decide where sets should be allocated in memory
  typename SetImpl = SImple
>
class AbstractSetInstrumentHelper {
 public:
  virtual void Insert_Value(Value *Ptr, Instruction *pos) = 0;
  virtual Instruction* MembershipCheckWith(Value *Ptr, Instruction *pos) = 0;
  virtual void FreeSet() = 0;
  virtual SetImpl &getSetImpl() = 0;
  virtual Value* getSignatureInfo(sigInfoType infoType, Instruction *pos,
                                  Value *Ptr = nullptr) = 0;
};


/*********
  Region Policy:  Regions have a single entrance and may have multiple exits.
  They must implement two functions: getEntry() and getExits().

  For now, we only have one Region kind, Function. But, we should consider
  Loops also.
 */

class FunctionRegion {
  Function &F;
  bool avail;
  std::vector<const Instruction*> exits;
 public:
   FunctionRegion(Function &af): F(af), avail(false) {}

   Instruction &getEntry() {
     return *F.getEntryBlock().begin();
   }

   const std::vector<const Instruction*> &getExits() {
     if (avail)
      return exits;
     for(Function::iterator i = F.begin(); i != F.end(); i++) {
    	 BasicBlock *BB = &*i;
       if(BB->getTerminator()) {
      	 if(isa<ReturnInst>(BB->getTerminator())) {
      	   exits.push_back(dyn_cast<Instruction>(BB->getTerminator()));
      	 }
       }
     }
     avail=true;
     return exits;
   }
};

template
<
    // Decide where sets should be allocated in memory
  typename SetImpl = SImple,
  typename AllocatePolicy = AllocateLocal<SImple>,
  typename Region = FunctionRegion
>
class SetInstrumentHelper : public AbstractSetInstrumentHelper<SetImpl> {
 private:
 SImpleWrapper<SetImpl, AllocatePolicy > BS; //!< Type of signature we'll be generating
  //! The signature, either a global variable or alloca, that is generated
  // in the code.
  //Value *Sign;
  //AllocatePolicy Alloc;
  Region &R;
  bool EarlyTerm;
  Value *ET;

 public:
 SetInstrumentHelper(Region &aR, SetImpl *SI, bool early=false)
 : BS(*SI),R(aR),EarlyTerm(early),ET(NULL) {
    IRBuilder<> Builder(&R.getEntry());
    BS.allocate(Builder);
    if (EarlyTerm)
      {
	ET = Builder.CreateAlloca(Builder.getInt32Ty(),  Builder.getInt32(1),
                                                   "earlyterm");
	Builder.CreateStore(ConstantInt::get(Builder.getInt32Ty(),0),ET);
      }
  }

  SetImpl &getSetImpl() { return BS.getSetImpl(); }

///
/// Insert pointer, Ptr, into the signature at position, pos. Ptr is
/// immediately converted to an i32/i64 using ptrtoint, so no need to worry
/// about what kind of pointer it is.
///
  virtual void Insert_Value(Value *Ptr, Instruction *pos) {
    IRBuilder<> B(pos);
    BS.insertPointer(B,Ptr);
  }

///
/// Check if address, Ptr, is present in the signature. Perform check
/// at the location before pos.  Return the instruction that produces
/// final boolean result as an i32: true means it is a member, false means
/// it is not.
///
  virtual Instruction* MembershipCheckWith(Value *Ptr, Instruction *pos) {
    if (!EarlyTerm) {
    	IRBuilder<> B(pos);
		return (Instruction*)BS.checkMembership(B,Ptr);
    } else {
		// Split basic block
		BasicBlock *Old = pos->getParent();
		BasicBlock *split = Old->splitBasicBlock(pos,Old->getName()+".split");
		LLVMContext &context = pos->getParent()->getContext();
		BasicBlock *newBB = BasicBlock::Create(context,"",Old->getParent(),split);

		// splitBasicBlock puts in a terminator for us (argh!) so we must remove it!
		Old->getTerminator()->eraseFromParent();

		IRBuilder<> OB(Old);
		Value *Ld = OB.CreateLoad(ET);
		OB.CreateCondBr(OB.CreateICmpEQ(Ld,OB.getInt32(0)),newBB,split);

	// Only evaluate check if so far it has come out to zero.
  // First completely construct this block, since checkMembership may split it.
		Instruction* newBBbr = (Instruction *) (IRBuilder<>(newBB).CreateBr(split));
		Value* V = BS.checkMembership(IRBuilder<>(newBBbr),Ptr);

  //Don't reuse the IRBuilder because checkMembership can split blocks, causing
  // the IRBuilder to become invalid.
		IRBuilder<>(newBBbr).CreateStore(V,ET);

		//IRBuilder<> SB(split->begin());
		IRBuilder<> SB(split);
		PHINode *phi = SB.CreatePHI(SB.getInt32Ty(),2);
		phi->addIncoming(SB.getInt32(1),Old);

	// We cannot depend on newBB since the block might be split, invalidating
  // the BB. Instead we can use V's basic block if it exists.
		if(isa<Instruction>(V)) {
			newBB = cast<Instruction>(V)->getParent();
		}
		phi->addIncoming(V,newBB);

		//Old->dump();
		//newBB->dump();
		//split->dump();
		return phi;
    }
  }

  virtual void FreeSet() {
    std::vector<const Instruction*>::const_iterator i = R.getExits().begin(),
                                                    end = R.getExits().end();
    for(;i!=end; i++) {
      IRBuilder<> B(const_cast<Instruction*>(*i));
      BS.freeSet(B);
    }
  }

///
/// Get extra information from signature. This can be corresponding to a
/// location and/or a value.
/// Returns the value containing the requested information.
///
  virtual Value* getSignatureInfo(sigInfoType infoType, Instruction *pos,
                                  Value *Ptr = nullptr){
     IRBuilder<> B(pos);
     return (Instruction*)BS.getSignatureInfo(infoType,B,Ptr);
  }
};

///
/// Provided primarily for testing.  Do not generate the API
/// if it's used for instrumentation.
///
class BuildSignatureAPI {
  SImple &BS;

 public:
  BuildSignatureAPI(SImple &aBS);

  void CreateAllocateFn(Module *M);
  void CreateInsertFn(Module *M);
  void CreateMembershipFn(Module *M);

  void CreateAPI(Module *M) {
    CreateAllocateFn(M);
    CreateInsertFn(M);
    CreateMembershipFn(M);
  }
};

#endif          // BUILDSIGNATURE_H
