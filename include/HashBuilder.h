#ifndef _HASH_BUILDER_H_
#define _HASH_BUILDER_H_

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

using namespace llvm;

// This is the lambda function type that will be called to build the hash
// function, by various signatures.
typedef std::function<Value * (IRBuilder<>, Value *)> HashBuilder;

class HashBuilderFactory {
public:
   //Calculating the int size large enough a pointer has to be fixed.
   // Calculating at each call may not a good idea. It would be better stored
   // in a static/global/MACRO and reused as needed. (not sure how to do it in
   // Factory class with static functions).
   // It can be calculated from DataLayout. Eg:
   //IntegerType* intType = Builder.GetInsertBlock()->getDataLayout()->
   //                                  getIntPtrType(Builder.getContext());
   // FIXME: The users of this expect a i32. Fix these together.
   // FIXME: The actual hashing is only designed for 32 bits. Has to be fixed
   // whenever moving to 64 bit.

   static HashBuilder CreateZeroIndex() {
      return [=](IRBuilder<> Builder, Value *V)->Value * {
         IntegerType* intType = Builder.getInt32Ty();
         return ConstantInt::get(intType,0);
      };
   }

   static HashBuilder CreateKnuthIndex(const int shift, const int mask){
      return [=](IRBuilder<> Builder, Value *V)->Value * {
         IntegerType* intType = Builder.getInt32Ty();
         Value * index = Builder.CreatePtrToInt(V,intType);
         index = Builder.CreateBinOp(Instruction::LShr, index,
                                            ConstantInt::get(intType,shift));
         index = Builder.CreateMul(index, ConstantInt::get(intType,2654435761));
         return Builder.CreateAnd(index, ConstantInt::get(intType,mask));
      };
   }

   static HashBuilder CreateShiftMaskIndex(const int shift, const int mask){
      return [=](IRBuilder<> Builder, Value *V)->Value * {
         IntegerType* intType = Builder.getInt32Ty();
         Value * index = Builder.CreatePtrToInt(V, intType);
         index = Builder.CreateBinOp(Instruction::LShr, index,
                                       ConstantInt::get(intType, shift));
         return Builder.CreateAnd(index, ConstantInt::get(intType, mask));
      };
   }

   static HashBuilder CreateXorIndex(const int shift, const int mask,
                                     const Instruction::BinaryOps preOp =
                                           Instruction::BinaryOps::BinaryOpsEnd,
                                    const int preOpArg = 0) {
      return [=](IRBuilder<> Builder, Value *V)->Value * {
         IntegerType* intType = Builder.getInt32Ty();
         V = Builder.CreatePtrToInt(V,intType);

         if(preOp > Instruction::BinaryOps::BinaryOpsBegin
         && preOp < Instruction::BinaryOps::BinaryOpsEnd) {
            V = Builder.CreateBinOp(preOp,V,ConstantInt::get(intType,preOpArg));
         }

         Value *mask3 = ConstantInt::get(intType,0xFF000000);
         //Value *mask2 = ConstantInt::get(intType,0xFF0000);
         //Value *mask1 = ConstantInt::get(intType,0xFF00);
         Value *mask0 = ConstantInt::get(intType,0xFF);

         Value *val3 = Builder.CreateLShr(Builder.CreateAnd(mask3,V),
                                                ConstantInt::get(intType,24));
         //Value *val2 = Builder.CreateLShr(Builder.CreateAnd(mask2,V),
         //                                     ConstantInt::get(intType,8));
         //Value *val1 = Builder.CreateShl(Builder.CreateAnd(mask1,V),
         //                                     ConstantInt::get(intType,8));
         Value *val0 = Builder.CreateShl(Builder.CreateAnd(mask0,V),
                                                ConstantInt::get(intType,24));

         //CreateXor(Builder.CreateXor(Builder.CreateXor(V,val3),val2),val1),val0);
         Value *valRandom = Builder.CreateXor(Builder.CreateXor(V,val3),val0);

         Value *index = Builder.CreateBinOp(Instruction::LShr, valRandom,
                                            ConstantInt::get(intType,shift));
         return Builder.CreateAnd(index,ConstantInt::get(intType,mask));
      };
   }

   static HashBuilder CreateStructSizeDynXorIndex(const int structSize){
      return [=](IRBuilder<> Builder, Value* V) -> Value* {
         IntegerType* intType = Builder.getInt32Ty();
         V = Builder.CreatePtrToInt(V,intType);
         V = Builder.CreateURem(V,ConstantInt::get(intType,structSize));
         if(structSize > 256) {
            if(structSize > 256*4) {
            // NOTE: Assumption is that struct is not bigger than 256k
            // Xor upper and lower half, discarding lower two bits
               Value *upperHalf = Builder.CreateLShr(Builder.CreateAnd(
                                          ConstantInt::get(intType,0x3FC00),V),
                                          ConstantInt::get(intType,8));
               V = Builder.CreateXor(upperHalf,V);
            }

          // Just shift by 4 because probably most of the struct elements will
          // be 4 bytes wide.
            V = Builder.CreateLShr(V,ConstantInt::get(intType,2));
         }
      };
   }
};

#endif    // _HASH_BUILDER_H_
