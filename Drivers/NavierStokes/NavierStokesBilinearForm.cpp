/*
 *  NavierStokesBilinearForm.cpp
 *
 *  Created by Nathan Roberts on 3/6/12.
 *
 */

#include "NavierStokesBilinearForm.h"

using namespace std;

// trial variable names:
static const string & S_U1_HAT = "\\widehat{u}_1";
static const string & S_U2_HAT = "\\widehat{u}_2";
static const string & S_T_HAT = "\\widehat{T}";
static const string & S_F1_N_HAT = "\\widehat{F}_{1n}";
static const string & S_F2_N_HAT = "\\widehat{F}_{1n}";
static const string & S_F3_N_HAT = "\\widehat{F}_{1n}";
static const string & S_F4_N_HAT = "\\widehat{F}_{1n}";
static const string & S_U1 = "u_1";
static const string & S_U2 = "u_2";
static const string & S_RHO = "\\rho";
static const string & S_T = "T";
static const string & S_SIGMA_11 = "\\tau_{11}";
static const string & S_SIGMA_21 = "\\tau_{21}";
static const string & S_SIGMA_22 = "\\tau_{22}";
static const string & S_Q_1 = "\\q_{1}";
static const string & S_Q_2 = "\\q_{2}";
static const string & S_OMEGA = "\\omega";

static const string & S_DEFAULT_TRIAL = "invalid trial";

// test variable names:
static const string & S_Q_1 = "q_1";
static const string & S_Q_2 = "q_2";
static const string & S_Q_3 = "q_3";
static const string & S_V_1 = "v_1";
static const string & S_V_2 = "v_2";
static const string & S_V_3 = "v_3";
static const string & S_V_4 = "v_4";
static const string & S_DEFAULT_TEST = "invalid test";

NavierStokesBilinearForm::NavierStokesBilinearForm(double Reyn, double Mach) {
  _Reyn = Reyn;
  _Mach = Mach;
  
  _testIDs.push_back(TAU_1);
  _testIDs.push_back(TAU_2);
  _testIDs.push_back(TAU_3);
  _testIDs.push_back(V_1);
  _testIDs.push_back(V_2);
  _testIDs.push_back(V_3);
  _testIDs.push_back(V_4);
  
  _trialIDs.push_back(U1_HAT);
  _trialIDs.push_back(U2_HAT);
  _trialIDs.push_back(T_HAT);
  _trialIDs.push_back(F1_N);
  _trialIDs.push_back(F2_N);
  _trialIDs.push_back(F3_N);
  _trialIDs.push_back(F4_N);
  
  _trialIDs.push_back(U1);
  _trialIDs.push_back(U2);
  _trialIDs.push_back(RHO);
  _trialIDs.push_back(T);
  _trialIDs.push_back(SIGMA_11);
  _trialIDs.push_back(SIGMA_21);
  _trialIDs.push_back(SIGMA_22);
  _trialIDs.push_back(Q_1);
  _trialIDs.push_back(Q_2);
  _trialIDs.push_back(OMEGA);
  
  pair<int, int> trialTest;
  // first equation, x part:
  trialTest = make_pair(RHO,V_1);
  _backFlowInteractions_x[trialTest].push_back(U1);
  trialTest = make_pair(U1,V_1);
  _backFlowInteractions_x[trialTest].push_back(RHO);
  // first equation, y part:
  trialTest = make_pair(RHO,V_1);
  _backFlowInteractions_y[trialTest].push_back(U2);
  trialTest = make_pair(U2,V_1);
  _backFlowInteractions_y[trialTest].push_back(RHO);
  
  // second equation, 
}

const string & NavierStokesBilinearForm::testName(int testID) {
  switch (testID) {
  case TAU_1:
    return S_TAU_1;
    break;
  case TAU_2:
    return S_TAU_2;
    break;
    break;
  case TAU_3:
    return S_TAU_3;
    break;
  case V_1:
    return S_V_1;
    break;
  case V_2:
    return S_V_2;
    break;
  case V_3:
    return S_V_3;
    break;
  case V_4:
    return S_V_4;
    break;
  default:
    return S_DEFAULT_TEST;
  }
}

const string & NavierStokesBilinearForm::trialName(int trialID) {
  switch(trialID) {
  case U1_HAT:
    return S_U1_HAT;
    break;
  case U2_HAT:
    return S_U2_HAT;
    break;
  case T_HAT:
    return S_T_HAT;
    break;
  case F1_N:
    return S_F1_N;
    break;
  case F2_N:
    return S_F2_N;
    break;
  case F3_N:
    return S_F3_N;
    break;
  case F4_N:
    return S_F4_N;
    break;
  case U1:
    return S_U1;
    break;
  case U2:
    return S_U2;
    break;
  case RHO:
    return S_RHO;
    break;
  case T:
    return S_T;
    break;
  case SIGMA_11:
    return S_SIGMA_11;
    break;
  case SIGMA_21:
    return S_SIGMA_21;
    break;
  case SIGMA_22:
    return S_SIGMA_22;
    break;
  case Q_1:
    return S_Q_1;
    break;
  case Q_2:
    return S_Q_2;
    break;
  case OMEGA:
    return S_OMEGA;
    break;
  default:
    return S_DEFAULT_TRIAL;
  }
}

bool NavierStokesBilinearForm::trialTestOperators(int testID1, int testID2, 
						  vector<EOperatorExtended> &trialOps,
						  vector<EOperatorExtended> &testOps) {

  // each test function only has one trace/flux involved
  if ( isFluxOrTrace(trialID) ) {
    testOps.push_back(IntrepidExtendedTypes::OPERATOR_VALUE);
    switch (testID) {
    case V_1:
      return (trialID == F1_N);
    case V_2:
      return (trialID == F2_N);
    case V_3:
      return (trialID == F3_N);
    case V_4:
      return (trialID == F4_N);
    }
    testOps.push_back(IntrepidExtendedTypes::OPERATOR_DOT_NORMAL);
    switch (testID) {
    case TAU_1:
      return (trialID == U1_HAT);
    case TAU_2:
      return (trialID == U2_HAT);
    case TAU_3:
      return (trialID == T_HAT);
    }
    TEST_FOR_EXCEPTION(true, std::invalid_argument, "unknown flux or trace");
  }
     
  bool returnValue = false; // unless we specify otherwise, trial and test don't interact
  switch (testID) {
  case TAU_1:

    if ((trialID==SIGMA_11) || (trialID==SIGMA_22) ){
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_X);// x component of test fxn
    }
    if ( (trialID==SIGMA_21) || (trialID == OMEGA)){
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_Y);
    }
    if ( trialID==U1){
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_DIV);
    }
    break;

  case TAU_2:

    if ((trialID==SIGMA_11) || (trialID==SIGMA_22) ){
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_Y);// x component of test fxn
    }
    if ( (trialID==SIGMA_21) || (trialID == OMEGA)){
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_X);
    }
    if ((trialID==U2) ){ // stresses are gradients of U1, U2
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_DIV);
    }
    break;

  case TAU_3: // test function for Fourier's heat law 

    if (trialID==Q_1) {
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_X);// x component of test fxn
    } 
    if (trialID==Q_2) {
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_Y);// y component of test fxn
    }
    if ( (trialID==T) ) { // heat is gradient of temp
      returnValue = true;
      testOps.push_back(IntrepidExtendedTypes::OPERATOR_DIV);
    }
    
  case V_1: // all constitutive law equations
  case V_2:
  case V_3:
  case V_4:
    // if there's nothing in the Eulerian interactions matrix, there's no interaction
    trialOps.push_back(IntrepidExtendedTypes::OPERATOR_VALUE);
    pair<int, int> key = make_pair(trialID,testID);
    if (   (_backFlowInteractions_x.find(key) == _backFlowInteractions_x.end() )
	   && (_backFlowInteractions_y.find(key) == _backFlowInteractions_y.end() ) ) {
      return false;
    }    
    returnValue = true;
    testOps.push_back(IntrepidExtendedTypes::OPERATOR_GRAD);
    break;
  default:
    break;
  }    
  return returnValue;
}

void NavierStokesBilinearForm::applyBilinearFormData(int trialID, int testID, 
						     FieldContainer<double> &trialValues, FieldContainer<double> &testValues,
						     const FieldContainer<double> &points) {
  
  // TODO: handle the traces & fluxes separately
  if (isFluxOrTrace(trialID)){
    if ( (trialID==U1_HAT) || (trialID==U2_HAT) || (trialID==T) ){
      multiplyFCByWeight(testValues,-1.0); // negate to get original value of var
    }
    // otherwise, do nothing
  } else { // if it's a field variable
    
    switch (testID) {
      
    case TAU_1: 
      
      if (trialID==SIGMA_11) {
	multiplyFCByWeight(testValues,1.0/(2*mu())-eta());
      }

      if (trialID==SIGMA_21) {
	multiplyFCByWeight(testValues,1.0/(2*mu());
      }

      if ( trialID==SIGMA_22 ) {
	multiplyFCByWeight(testValues,-eta());
      }

      if (trialID == OMEGA ) {
	multiplyFCByWeight(testValues,-1.0);	
      }
      // U1 has correct values

      break;
    case TAU_2: 
      
      if (trialID==SIGMA_11) {
	multiplyFCByWeight(testValues,-eta());
      }

      if (trialID==SIGMA_21) {
	multiplyFCByWeight(testValues,1.0/(2*mu());
      }

      if ( trialID==SIGMA_22 ) {
	multiplyFCByWeight(testValues,1.0/(2*mu())-eta());
      }

      if (trialID == OMEGA ) {
	multiplyFCByWeight(testValues,1.0);	
      }
      // U2 has correct values

      break;
    case TAU_3:
      if (isHeatStressVariable(trialID)){
	multiplyFCByWeight(testValues,1.0/_kappa); // already paired with correct ops
      }
      // T has correct values
      break;

      /*
	if ( (trialID==U1) || (trialID==U2) || (trialID==T) ) {
	// do nothing -- testTrialValuesAtPoints has the right values (div)
	}
      */
      
    case V_1: // catch-all for Hgrad test functions (on constitutive laws)
    case V_2:
    case V_3:
    case V_4:
      // have grad v -- want grad_v dot something else that depends on background flow
      { // block off to avoid compiler complaints about adding new variables inside switch/case
	// dimensions of testTrialValuesAtPoints should be (C,F,P,D)
	int numCells = testValues.dimension(0);
	int basisCardinality = testValues.dimension(1);
	int numPoints = testValues.dimension(2);
	int spaceDim = testValues.dimension(3);
	// because we change dimensions of the values, by dotting with beta, 
	// we'll need to copy the values and resize the original container
	FieldContainer<double> testValuesCopy = testValues;
	testValues.resize(numCells,basisCardinality,numPoints);
	TEST_FOR_EXCEPTION(spaceDim != 2, std::invalid_argument,
			   "NavierStokesBilinearForm only supports 2 dimensions right now.");
      
	FieldContainer<double> beta = getValuesToDot(trialID,testID,basisCache);
	FieldContainer<double> points = basisCache->getPhysicalCubaturePoints();
	for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
	  for (int basisOrdinal=0; basisOrdinal<basisCardinality; basisOrdinal++) {
	    for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
	      double x = points(cellIndex,ptIndex,0);
	      double y = points(cellIndex,ptIndex,1);
	      testValues(cellIndex,basisOrdinal,ptIndex)  = -beta(cellIndex,ptIndex,0) * testValuesCopy(cellIndex,basisOrdinal,ptIndex,0) + -beta(cellIndex,ptIndex,1) * testValuesCopy(cellIndex,basisOrdinal,ptIndex,1);
            
	    }
	  }
	}
      }
      break; 
    } // end of switch b/w test IDs

  } // end of else
}

FieldContainer<double> NavierStokesBilinearForm::getValuesToDot(int trialID, int testID, Teuchos::RCP<BasisCache> basisCache) {
  int numCells = basisCache->getPhysicalCubaturePoints().dimension(0);
  int numPoints = basisCache->getPhysicalCubaturePoints().dimension(1);
  int spaceDim = basisCache->getPhysicalCubaturePoints().dimension(2);
  
  FieldContainer<double> values(numCells,numPoints);
  _backgroundFlow->solutionValues(values, BurgersBilinearForm::U, basisCache);  
  FieldContainer<double> beta(numCells,numPoints,spaceDim);
  for (int cellIndex=0;cellIndex<numCells;cellIndex++){
    for (int ptIndex=0;ptIndex<numPoints;ptIndex++){
      beta(cellIndex,ptIndex,0) = 1.0*values(cellIndex,ptIndex);
      //      beta(cellIndex,ptIndex,0) = 1.0;
      beta(cellIndex,ptIndex,1) = 1.0;
    }
  }
  return beta;
}
      
EFunctionSpaceExtended NavierStokesBilinearForm::functionSpaceForTest(int testID) {
  switch (testID) {
  case TAU_1:
  case TAU_2:
  case TAU_3:
    return IntrepidExtendedTypes::FUNCTION_SPACE_HDIV;
    break;
  case V_1:
  case V_2:
  case V_3:
    return IntrepidExtendedTypes::FUNCTION_SPACE_HGRAD;
    break;
  case ONE:
    return IntrepidExtendedTypes::FUNCTION_SPACE_ONE;
  default:
    throw "Error: unknown testID";
  }
}

EFunctionSpaceExtended NavierStokesBilinearForm::functionSpaceForTrial(int trialID) {
  // Field variables, and fluxes, are all L2.
  // Traces (like PHI_HAT) are H1 if we use conforming traces.
  // For now, don’t use conforming (like last summer).
  return IntrepidExtendedTypes::FUNCTION_SPACE_HVOL;
}

bool NavierStokesBilinearForm::isFluxOrTrace(int trialID) {
  if ((U1_HAT==trialID) || (U2_HAT==trialID) || (T_HAT==trialID) || // traces
      (F1_N==trialID) || (F2_N==trialID) || (F3_N==trialID)|| (F4_N==trialID)){ // fluxes
    return true;
  } else {
    return false;
  }
}
 
bool NavierStokesBilinearForm::isViscousStressVariable(int trialID) {
  return ( (trialID==SIGMA_11) || (trialID==SIGMA_21) || (trialID==SIGMA_22) || (trialID==OMEGA) );  
}

bool NavierStokesBilinearForm::isHeatStressVariable(int trialID) {
  return ((trialID==Q_1) || (trialID==Q_2));
}
 
bool NavierStokesBilinearForm::isEulerianVariable(int trialID) {
  return ( (trialID==U1) || (trialID==U2) || (trialID==RHO) || (trialID==T));
}
 
