#ifndef SETINSTRUMENTFACTORY_H
#define SETINSTRUMENTFACTORY_H

#include "BuildSignature.h"

///
/// This class makes it easy to create signatures with given properties,
/// without needing to construct intermediate objects, like the Hashing
/// class. Overtime, we'll tune the implementation of CreateFastSignature
/// and CreateAccurateSignature to provide best solutiosn for # bits needed.
///
/// It also my intention this Factory will support other types of sets,
/// including tables, as time goes on.
///
class SImpleFactory {
 public:
  static SImple *CreateFastSignature(unsigned int bits);
  static SImple *CreateAccurateSignature(unsigned int bits);
  static SImple *CreateHybridSignature(unsigned int bits);
  static SImple *CreateDynStructSignature(unsigned int bits,
                                          unsigned int structSize);

  //static SetInstrument *CreateSimpleSignature(int bits);
  //static SetInstrument *CreateSimpleSignatureWithKnuthHash(int bits);
  //static SetInstrument *CreateSimpleSignature(int bits, BuildHashIndex &h);
  //static SetInstrument *CreateArraySignature(int bits);
  //static SetInstrument *CreateArraySignatureWithKnuthHash(int bits);

  static SImple *CreateLibCallSignature();
  static SImple *CreatePerfectSet();
  static SImple *CreateRangeSet();
  static SImple *CreateHashTableSet();
};

#endif     // SETINSTRUMENTFACTORY_H
