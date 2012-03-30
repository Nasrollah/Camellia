//
//  LinearTerm.h
//  Camellia
//
//  Created by Nathan Roberts on 3/30/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_LinearTerm_h
#define Camellia_LinearTerm_h

#include "Intrepid_Basis.hpp"
#include "Function.h"
#include "Var.h"
#include "BasisCache.h"

class LinearTerm;
typedef Teuchos::RCP<LinearTerm> LinearTermPtr;

typedef Teuchos::RCP< Basis<double,FieldContainer<double> > > BasisPtr;
typedef Teuchos::RCP< const FieldContainer<double> > constFCPtr;

class LinearTerm {
  int _rank; // gets set after first var is added
  typedef pair< FunctionPtr, VarPtr > LinearSummand;
  vector< LinearSummand > _summands;
  set<int> _varIDs;
  VarType _termType; // shouldn't mix
public: // was protected; changed for debugging (no big deal either way, I think)
  const vector< LinearSummand > & summands() const;
public:
  LinearTerm(FunctionPtr weight, VarPtr var);
  LinearTerm(double weight, VarPtr var);
  LinearTerm(vector<double> weight, VarPtr var);
  LinearTerm( VarPtr v );
  // copy constructor:
  LinearTerm( const LinearTerm &a );  
  void addVar(FunctionPtr weight, VarPtr var);
  void addVar(double weight, VarPtr var);
  void addVar(vector<double> vector_weight, VarPtr var);
  
  const set<int> & varIDs() const;
  
  VarType termType() const;
  //  vector< EOperatorExtended > varOps(int varID);
  
  // compute the value of linearTerm for non-zero varID at the cubature points, for each basis function in basis
  // values shape: (C,F,P), (C,F,P,D), or (C,F,P,D,D)
  void values(FieldContainer<double> &values, int varID, BasisPtr basis, BasisCachePtr basisCache, 
              bool applyCubatureWeights = false, int sideIndex = -1);
  
  int rank() const;  // 0 for scalar, 1 for vector, etc.
  
  // operator overloading niceties:
  
  LinearTerm& operator=(const LinearTerm &rhs);
  LinearTerm& operator+=(const LinearTerm &rhs);
  
  LinearTerm& operator+=(VarPtr v);
};

// operator overloading for syntax sugar:
LinearTermPtr operator+(LinearTermPtr a1, LinearTermPtr a2);

LinearTermPtr operator+(VarPtr v, LinearTermPtr a);

LinearTermPtr operator+(LinearTermPtr a, VarPtr v);

LinearTermPtr operator*(FunctionPtr f, VarPtr v);

LinearTermPtr operator*(double weight, VarPtr v);

LinearTermPtr operator*(VarPtr v, double weight);

LinearTermPtr operator*(vector<double> weight, VarPtr v);

LinearTermPtr operator*(VarPtr v, vector<double> weight);

LinearTermPtr operator+(VarPtr v1, VarPtr v2);

LinearTermPtr operator/(VarPtr v, double weight);

LinearTermPtr operator-(VarPtr v1, VarPtr v2);

LinearTermPtr operator-(VarPtr v);

LinearTermPtr operator-(LinearTermPtr a, VarPtr v);

#endif