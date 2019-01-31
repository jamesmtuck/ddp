//===- Signature.h - Set Profiler for data dependence profiling ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is an important base class for signatures in signature-based DDP.
//
//===----------------------------------------------------------------------===//

#ifndef NEW_PROJECT_SIGNATURE_H
#define NEW_PROJECT_SIGNATURE_H

#include "llvm/IR/Module.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

#define SIZE_32 1
#define SIZE_64 2
#define SIZE_96 3
#define SIZE_128 4
#define SIZE_160 5
#define SIZE_256 8
#define SIZE_512 16
#define SIZE_768 24
#define SIZE_1K 32
#define SIZE_2K 64

namespace llvm {
	enum SignatureType {
		BITS_32 = 1, /* The 32 bit signature */
		BITS_64 = 2, /* The 64 bit signature */
		BITS_96 = 3, /* The 96 bit signature */
		BITS_128 = 4, /* The 128 bit signature */
		BITS_160 = 5, /* The 160 bit signature */
		BITS_256 = 6, /* The 256 bit signature */
		BITS_512 = 7, /* The 512 bit signature */
		BITS_768 = 8, /* The 512 bit signature */
		BITS_1K = 9, /* The 1K bit signature */
		BITS_2K = 10 /* The 2K bit signature */
	};

	class Signature {
		static unsigned int AllocaMem;
		static unsigned int ZeroOutMem;
		static unsigned int InsertMem;
		static unsigned int DisambigMem;
		static unsigned int MaxSignRefID;
		enum SignatureType STy;
		Value* Bits;
		LLVMContext* Context;
		Module *M;
		unsigned int SetNum;
		unsigned int VectorConfig;

		static unsigned int getSignRefID();

		void FillVectorConfig_HT();

		void FillVectorConfig_BF2();

		void FillVectorConfig();

		static char* getString(SignatureType ST);

		bool insertCall;

	public:
		Signature(enum SignatureType SignType, Instruction* EnterPos,
							Instruction *AZEnterPos, LLVMContext *CurrContext,
							Module* CurrModule, unsigned int CurrSetNum, bool insertCall = true);

		void HWSignCache_Insert_Value(Value* InsertVal, Instruction* EnterPos,
																	Function *F, unsigned int SignRefID,
																	unsigned int InstRefID);

		void Insert_Value(Value* InsertVal, Instruction* EnterPos);

		void HWSignCache_MembershipCheckWith(Value *LoadAddr, Instruction* EnterPos,
																				 Function *F, unsigned int SignRefID,
																				 unsigned int InstRefID);

		Instruction* MembershipCheckWith(Value *LoadAddr, Instruction* EnterPos);

	};
}

#endif // NEW_PROJECT_SIGNATURE_H
