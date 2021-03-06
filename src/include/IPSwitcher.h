//
//  IP.h
//  Camellia
//
//  Created by Nathan Roberts on 3/30/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_IP_SWITCH
#define Camellia_IP_SWITCH

#include "IP.h"

using namespace std;
class IPSwitcher : public IP {
  IPPtr _ip1;
  IPPtr _ip2;
  double _minH; // min element  size for when to 
public:
  IPSwitcher(IPPtr ip1, IPPtr ip2, double minH);
    
  void computeInnerProductMatrix(FieldContainer<double> &innerProduct, 
                                 Teuchos::RCP<DofOrdering> dofOrdering,
                                 Teuchos::RCP<BasisCache> basisCache);
  
  void computeInnerProductVector(FieldContainer<double> &ipVector, 
                                 VarPtr var, FunctionPtr fxn,
                                 Teuchos::RCP<DofOrdering> dofOrdering, 
                                 Teuchos::RCP<BasisCache> basisCache);

  double computeMaxConditionNumber(DofOrderingPtr testSpace, BasisCachePtr basisCache);
  
  // added by Jesse
  LinearTermPtr evaluate(map< int, FunctionPtr> &varFunctions, bool boundaryPart);

  bool hasBoundaryTerms();
    
  void printInteractions();
};

typedef Teuchos::RCP<IP> IPPtr;

#endif
