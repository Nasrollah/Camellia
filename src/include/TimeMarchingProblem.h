//
//  TimeMarchingProblem.h
//  Camellia
//
//  Created by Nathan Roberts on 2/27/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_TimeMarchingProblem_h
#define Camellia_TimeMarchingProblem_h

#include "RHS.h"

class Solution;

class TimeMarchingProblem : public BF, public RHS {
  Teuchos::RCP<RHS> _rhs;
  double _dt;
  Teuchos::RCP<Solution> _previousTimeSolution;
protected:
  BFPtr _bilinearForm;
public:
  TimeMarchingProblem(BFPtr bilinearForm, Teuchos::RCP<RHS> rhs);
  
  // BILINEAR FORM:
  virtual void trialTestOperators(int trialID, int testID, 
                                  vector<Camellia::EOperator> &trialOps,
                                  vector<Camellia::EOperator> &testOps);
  
  virtual void applyBilinearFormData(FieldContainer<double> &trialValues, FieldContainer<double> &testValues, 
                                     int trialID, int testID, int operatorIndex,
                                     Teuchos::RCP<BasisCache> basisCache);
  
  virtual Camellia::EFunctionSpace functionSpaceForTest(int testID);
  virtual Camellia::EFunctionSpace functionSpaceForTrial(int trialID); 
  
  virtual bool isFluxOrTrace(int trialID);
  
  virtual const string & testName(int testID);
  virtual const string & trialName(int trialID);
  
  // RHS:
  bool nonZeroRHS(int testVarID);
  vector<Camellia::EOperator> operatorsForTestID(int testID);
  virtual void rhs(int testVarID, int operatorIndex, Teuchos::RCP<BasisCache> basisCache, 
                   FieldContainer<double> &values);
  
  // methods that belong to the TimeMarchingProblem:
  virtual void timeLHS(FieldContainer<double> trialValues, int trialID, Teuchos::RCP<BasisCache> basisCache);
  virtual void timeRHS(FieldContainer<double> values, int testID, Teuchos::RCP<BasisCache> basisCache);
  
  void setTimeStepSize(double dt);
  double timeStepSize();
  
  virtual bool hasTimeDerivative(int trialID, int testID) = 0;
  bool testHasTimeDerivative(int testID);
  
  Teuchos::RCP<Solution> previousTimeSolution();
  void setPreviousTimeSolution(Teuchos::RCP<Solution> previousSolution);
  
};

#endif
