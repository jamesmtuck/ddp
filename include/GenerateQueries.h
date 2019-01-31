//
//===----------------------------------------------------------------------===//
//
// This class generates queries for signature-based DDP to analyze in runtime.
//
//===----------------------------------------------------------------------===//
//
#ifndef GENERATE_QUERIES_H
#define GENERATE_QUERIES_H

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

#include "ProfileDBHelper.h"
#include <vector>
#include <string>

using namespace llvm;

namespace ddp {
  typedef enum {
    QueryAtLHS=0,
    //QueryAtRHS=1,
    QueryAtEndRegion=2
  } QueryPlacement;

  struct Query {
    unsigned long long id;
    Instruction *lhs;
    Instruction *rhs;
    unsigned long long pset;
    bool repeated;
    unsigned int total;

    Query():id(0),lhs(NULL),rhs(NULL),pset(0),repeated(false),total(0) {}

    Query(unsigned long long aid, Instruction *alhs, Instruction *arhs,
	                                           unsigned long long apset=0)
      :id(aid),lhs(alhs),rhs(arhs),pset(apset),repeated(false),total(0) {}
  };

  class Queries {
  public:
    typedef std::vector<Query> QueryVector;
    typedef std::vector<Query>::iterator query_iterator;
    typedef std::vector<Query>::const_iterator query_const_iterator;

  protected:
    QueryVector v;

    void insertQuery(Query &Q) {
      v.push_back(Q);
    }
    virtual void insertQuery(Instruction *lhs, Instruction *rhs,
                             unsigned long long id = 0,
                             unsigned long long pset = 0) {
      Query Q(id,lhs,rhs,pset);
      insertQuery(Q);
    }

  public:
    query_iterator begin() { return v.begin(); }
    query_iterator end() { return v.end(); }
    //query_const_iterator const_begin() { return v.cbegin(); }
    //query_const_iterator const_end() { return v.cend(); }

    size_t size() { return v.size(); }

    QueryVector getQueryVector() { return v; }
  };

  class MayAliasQueries : public Queries {
  private:
     static Value* traceToAddressSource(Value *val, std::set<PHINode*> &seenPhi);
     static bool addressNeverTaken(Instruction *inst, std::set<PHINode*> &seenPhi);
     static GetElementPtrInst* traceToStructGEP(Value *val);
     static bool isConstAddr(Value *val, std::set<PHINode*> &seenPhi);
  protected:
    virtual void insertQuery(Instruction *lhs, Instruction *rhs,
                             unsigned long long id = 0,
                             unsigned long long fileid = 0,
                             unsigned long long pset = 0);
  public:
    virtual void run(Function &F, AAResultsWrapperPass &AA, ProfileDBHelper &db);
  };

  void printBacktrace(const std::string &filename, Value *val);
}

#endif //GENERATE_QUERIES_H
