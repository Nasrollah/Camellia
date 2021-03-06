#ifndef PROJECTOR
#define PROJECTOR

#include "IP.h"

#include "Basis.h"

class Function;

typedef Teuchos::RCP<Function> FunctionPtr;

class Projector {
 public:

  // newest version:
  static void projectFunctionOntoBasis(Intrepid::FieldContainer<double> &basisCoefficients,
                                       FunctionPtr fxn, BasisPtr basis, BasisCachePtr basisCache,
                                       IPPtr ip, VarPtr v,
                                       std::set<int>fieldIndicesToSkip = std::set<int>());
  
  static void projectFunctionOntoBasis(Intrepid::FieldContainer<double> &basisCoefficients,
                                       FunctionPtr fxn, BasisPtr basis, BasisCachePtr basisCache);
  
  static void projectFunctionOntoBasisInterpolating(Intrepid::FieldContainer<double> &basisCoefficients,
                                                    FunctionPtr fxn, BasisPtr basis, BasisCachePtr domainBasisCache);
};
#endif