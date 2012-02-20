#include "ConfusionBilinearForm.h"
#include "ConfectionManufacturedSolution.h"

typedef Sacado::Fad::SFad<double,2> F2;                          // FAD with # of independent vars fixed at 2 (x and y)
typedef Sacado::Fad::SFad< Sacado::Fad::SFad<double,2>, 2> F2_2; // same thing, but nested so we can take 2 derivatives

ConfectionManufacturedSolution::ConfectionManufacturedSolution(double epsilon, double beta_x, double beta_y) {
  _epsilon = epsilon;
  _beta_x  = beta_x;
  _beta_y  = beta_y;
  
  // set the class variables from ExactSolution:
  _bc = Teuchos::rcp(this,false);  // false: don't let the RCP own the memory
  _rhs = Teuchos::rcp(this,false);
  _bilinearForm = Teuchos::rcp(new ConfusionBilinearForm(epsilon,beta_x,beta_y));
}

int ConfectionManufacturedSolution::H1Order() {
  // -1 for non-polynomial solution...
  return -1;
}

template <typename T> const T ConfectionManufacturedSolution::u(T &x, T &y) {
  // in Bui, Demkowicz, Ghattas' paper
  double pi = acos(0.0)*2.0;
  T t = 1.0 + sin(pi*(1.0+x)*(1.0+y)*(1.0+y)/8.0);
  //  T t = x + (exp((_beta_x * x)/_epsilon) - 1.0)/(1.0 - exp(_beta_x/_epsilon));
  //  t  *= y + (exp((_beta_y * y)/_epsilon) - 1.0)/(1.0 - exp(_beta_y/_epsilon));
  return t;
}

double ConfectionManufacturedSolution::solutionValue(int trialID,
						     FieldContainer<double> &physicalPoint) {
  
  double x = physicalPoint(0);
  double y = physicalPoint(1);
  F2 sx(2,0,x), sy(2,1,y), su; // s for Sacado 
  su = u(sx,sy);
  double value;
  
  switch(trialID) {
  case ConfusionBilinearForm::U:
    value = su.val();
    break;
  case ConfusionBilinearForm::U_HAT:
    value = su.val();
    break;
  case ConfusionBilinearForm::SIGMA_1:
    value = _epsilon * su.dx(0); // SIGMA_1 == eps * d/dx (u)
    break;
  case ConfusionBilinearForm::SIGMA_2:
    value = _epsilon * su.dx(1); // SIGMA_2 == eps * d/dy (u)
    break;
  case ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT:
    TEST_FOR_EXCEPTION( trialID == ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT,
			std::invalid_argument,
			"for fluxes, you must call solutionValue with unitNormal argument.");
    break;
  default:
    TEST_FOR_EXCEPTION( true, std::invalid_argument,
			"solutionValues called with unknown trialID.");
  }
  return value;
}

double ConfectionManufacturedSolution::solutionValue(int trialID,
						     FieldContainer<double> &physicalPoint,
						     FieldContainer<double> &unitNormal) {
  if ( trialID != ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT ) {
    return solutionValue(trialID,physicalPoint);
  }
  // otherwise, get SIGMA_1 and SIGMA_2, and the unit normal
  double sigma1 = solutionValue(ConfusionBilinearForm::SIGMA_1,physicalPoint);
  double sigma2 = solutionValue(ConfusionBilinearForm::SIGMA_2,physicalPoint);
  double u = solutionValue(ConfusionBilinearForm::U,physicalPoint);
  double n1 = unitNormal(0);
  double n2 = unitNormal(1);
  double sigma_n = sigma1*n1 + sigma2*n2;
  double beta_n = _beta_x*n1 + _beta_y*n2;
  return u*beta_n - sigma_n;
}

/********** RHS implementation **********/
bool ConfectionManufacturedSolution::nonZeroRHS(int testVarID) {
  if (testVarID == ConfusionBilinearForm::TAU) { // the vector test function, zero RHS
    return false;
  } else if (testVarID == ConfusionBilinearForm::V) {
    return true;
  } else {
    return false; // could throw an exception here
  }
}

void ConfectionManufacturedSolution::rhs(int testVarID, const FieldContainer<double> &physicalPoints, FieldContainer<double> &values) {
  int numCells = physicalPoints.dimension(0);
  int numPoints = physicalPoints.dimension(1);
  int spaceDim = physicalPoints.dimension(2);
  if (testVarID == ConfusionBilinearForm::V) {
    // f = - eps * (d^2/dx^2 + d^2/dy^2) ( u ) + beta_x du/dx + beta_y du/dy
    values.resize(numCells,numPoints);
    for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
      for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
        double x = physicalPoints(cellIndex,ptIndex,0);
        double y = physicalPoints(cellIndex,ptIndex,1);
        F2_2 sx(2,0,x), sy(2,1,y), su; // s for Sacado 
        sx.val() = F2(2,0,x);
        sy.val() = F2(2,1,y);
        su = u(sx,sy);

	/* these are values of convection-diffusion
	   values(cellIndex,ptIndex) = - _epsilon * ( su.dx(0).dx(0) + su.dx(1).dx(1) ) 
	   + _beta_x * su.dx(0).val() + _beta_y * su.dx(1).val();
	*/

	// this RHS corresponds to only convection
	//values(cellIndex,ptIndex) = _beta_x * su.dx(0).val() + _beta_y * su.dx(1).val();
	
	// exact RHS
	double pi = acos(0.0)*2.0;
	values(cellIndex,ptIndex) = pi/10.0*(1.0+y) * (1.0+y + 1.5*(1.0+x)) * cos(pi*(1.0+x)*(1.0+y)*(1.0+y)/8.0);
	
      }
    }
  }
}

/***************** BC Implementation *****************/
bool ConfectionManufacturedSolution::bcsImposed(int varID){
  // returns true if there are any BCs anywhere imposed on varID
  return (varID == ConfusionBilinearForm::U_HAT);
  //return (varID == ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT);
}

void ConfectionManufacturedSolution::imposeBC(int varID, FieldContainer<double> &physicalPoints,
					      FieldContainer<double> &unitNormals,
					      FieldContainer<double> &dirichletValues,
					      FieldContainer<bool> &imposeHere) {
  int numCells = physicalPoints.dimension(0);
  int numPoints = physicalPoints.dimension(1);
  int spaceDim = physicalPoints.dimension(2);
  
  TEST_FOR_EXCEPTION( ( spaceDim != 2  ),
		      std::invalid_argument,
		      "ConfusionBC expects spaceDim==2.");  
  
  TEST_FOR_EXCEPTION( ( dirichletValues.dimension(0) != numCells ) 
		      || ( dirichletValues.dimension(1) != numPoints ) 
		      || ( dirichletValues.rank() != 2  ),
		      std::invalid_argument,
		      "dirichletValues dimensions should be (numCells,numPoints).");
  TEST_FOR_EXCEPTION( ( imposeHere.dimension(0) != numCells ) 
		      || ( imposeHere.dimension(1) != numPoints ) 
		      || ( imposeHere.rank() != 2  ),
		      std::invalid_argument,
		      "imposeHere dimensions should be (numCells,numPoints).");
  Teuchos::Array<int> pointDimensions;
  pointDimensions.push_back(2);
  for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
    for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
      FieldContainer<double> physicalPoint(pointDimensions,
                                           &physicalPoints(cellIndex,ptIndex,0));
      FieldContainer<double> unitNormal(pointDimensions,
                                        &unitNormals(cellIndex,ptIndex,0));
      double beta_n = unitNormals(cellIndex,ptIndex,0)*_beta_x + unitNormals(cellIndex,ptIndex,1)*_beta_y;
      double x = physicalPoint(cellIndex,ptIndex,0);
      double y = physicalPoint(cellIndex,ptIndex,1);
      dirichletValues(cellIndex,ptIndex) = solutionValue(varID, physicalPoint, unitNormal);
      /*
      dirichletValues(cellIndex,ptIndex) = beta_n*(1.0-x);
      if (abs(x)<1e-14){
	dirichletValues(cellIndex,ptIndex) = beta_n*(1.0-y);
      }
      */
      if ((abs(x+1.0)<1e-14) || (abs(y+1.0)<1e-14)) {
	imposeHere(cellIndex,ptIndex) = true; // impose using dirichlet only on inflow
      }
    }
  }
}

void ConfectionManufacturedSolution::getConstraints(FieldContainer<double> &physicalPoints, 
						    FieldContainer<double> &unitNormals,
						    vector<map<int,FieldContainer<double > > > &constraintCoeffs,
						    vector<FieldContainer<double > > &constraintValues){
    
  int numCells = physicalPoints.dimension(0);
  int numPoints = physicalPoints.dimension(1);
  int spaceDim = physicalPoints.dimension(2);       
  map<int,FieldContainer<double> > outflowConstraint;
  FieldContainer<double> uCoeffs(numCells,numPoints);
  FieldContainer<double> beta_sigmaCoeffs(numCells,numPoints);
  FieldContainer<double> outflowValues(numCells,numPoints);
  double tol = 1e-12;
  // default to no constraints, apply on outflow only
  uCoeffs.initialize(0.0);
  beta_sigmaCoeffs.initialize(0.0);
  outflowValues.initialize(0.0);
    
  for (int cellIndex=0;cellIndex<numCells;cellIndex++){
    for (int pointIndex=0;pointIndex<numPoints;pointIndex++){
      double x = physicalPoints(cellIndex,pointIndex,0);
      double y = physicalPoints(cellIndex,pointIndex,1);

      double beta_n = _beta_x*unitNormals(cellIndex,pointIndex,0)+_beta_y*unitNormals(cellIndex,pointIndex,1);

	
      if ((abs(x-1.0) < tol) || (abs(y-1.0) < tol)) { // if on outflow boundary
	TEST_FOR_EXCEPTION(beta_n < 0,std::invalid_argument,"Inflow condition on boundary");
	  
	// this combo isolates sigma_n
	//	  uCoeffs(cellIndex,pointIndex) = 1.0;
	uCoeffs(cellIndex,pointIndex) = beta_n;
	beta_sigmaCoeffs(cellIndex,pointIndex) = -1.0;	    
      }
	
    }
  }
  outflowConstraint[ConfusionBilinearForm::U_HAT] = uCoeffs;
  outflowConstraint[ConfusionBilinearForm::BETA_N_U_MINUS_SIGMA_HAT] = beta_sigmaCoeffs;	        
  constraintCoeffs.push_back(outflowConstraint); // only one constraint on outflow
    
}
