//  Camellia
//
//  Created by Truman Ellis on 6/4/2012.

#include "InnerProductScratchPad.h"
#include "RefinementStrategy.h"
#include "SolutionExporter.h"

#ifdef HAVE_MPI
#include <Teuchos_GlobalMPISession.hpp>
#include "mpi_choice.hpp"
#else
#include "choice.hpp"
#endif

double pi = 2.0*acos(0.0);

int H1Order = 3, pToAdd = 2;

class ConstantXBoundary : public SpatialFilter {
   private:
      double xval;
   public:
      ConstantXBoundary(double xval): xval(xval) {};
      bool matchesPoint(double x, double y) {
         double tol = 1e-14;
         return (abs(x-xval) < tol);
      }
};

class ConstantYBoundary : public SpatialFilter {
   private:
      double yval;
   public:
      ConstantYBoundary(double yval): yval(yval) {};
      bool matchesPoint(double x, double y) {
         double tol = 1e-14;
         return (abs(y-yval) < tol);
      }
};

class DiscontinuousInitialCondition : public Function {
   private:
      double xloc;
      double valL;
      double valR;
   public:
      DiscontinuousInitialCondition(double xloc, double valL, double valR) : Function(0), xloc(xloc), valL(valL), valR(valR) {}
      void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
         int numCells = values.dimension(0);
         int numPoints = values.dimension(1);

         const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
         for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
            for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
               double x = (*points)(cellIndex,ptIndex,0);
               if (x <= xloc)
                  values(cellIndex, ptIndex) = valL;
               else
                  values(cellIndex, ptIndex) = valR;
            }
         }
      }
};

class RampedInitialCondition : public Function {
   private:
      double xloc;
      double valL;
      double valR;
      double h;
   public:
      RampedInitialCondition(double xloc, double valL, double valR, double h) : Function(0), xloc(xloc), valL(valL), valR(valR), h(h) {}
      void setH(double h_) { h = h_; }
      void values(FieldContainer<double> &values, BasisCachePtr basisCache) {
         int numCells = values.dimension(0);
         int numPoints = values.dimension(1);

         const FieldContainer<double> *points = &(basisCache->getPhysicalCubaturePoints());
         for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
            for (int ptIndex=0; ptIndex<numPoints; ptIndex++) 
            {
               double x = (*points)(cellIndex,ptIndex,0);
               if (x <= xloc-h/2.)
                  values(cellIndex, ptIndex) = valL;
               else if (x >= xloc+h/2.)
                  values(cellIndex, ptIndex) = valR;
               else
                  values(cellIndex, ptIndex) = valL+(valR-valL)/h*(x-xloc+h/2);
            }
         }
      }
};

int main(int argc, char *argv[]) {
#ifdef HAVE_MPI
  Teuchos::GlobalMPISession mpiSession(&argc, &argv,0);
  choice::MpiArgs args( argc, argv );
#else
  choice::Args args( argc, argv );
#endif
  int commRank = Teuchos::GlobalMPISession::getRank();
  int numProcs = Teuchos::GlobalMPISession::getNProc();

   // Required arguments
  int problem = args.Input<int>("--problem", "which problem to run");

   // Optional arguments (have defaults)
  int numRefs = args.Input("--numRefs", "number of refinement steps", 0);
  int norm = args.Input("--norm", "norm", 0);
  int polyOrder = args.Input("--polyOrder", "polynomial order for field variables", 2);
  int deltaP = args.Input("--deltaP", "how much to enrich test space", 2);
  int xCells = args.Input("--xCells", "number of cells in the x direction", 8);
  int tCells = args.Input("--tCells", "number of cells in the t direction", 4);
  int maxNewtonIterations = args.Input("--maxIterations", "maximum number of Newton iterations", 20);
  int numPreRefs = args.Input<int>("--numPreRefs","pre-refinements on singularity",0);

  args.Process();

   ////////////////////   PROBLEM DEFINITIONS   ///////////////////////
  int H1Order = polyOrder+1;

  double xmin, xmax, xint, tmax;
  double GAMMA, rhoL, rhoR, uL, uR, pL, pR, eL, eR, TL, TR;
  double Cv = 718;
  double Cp = 1010;
  double R = 287;
  double Pr = 0.713;
  string problemName;

  switch (problem)
  {
    case 0:
    // Simple problem for testing
    problemName = "SimpleShock";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.1;

    // GAMMA = 1.4;
    rhoL = 1;
    rhoR = 1;
    uL = 1;
    uR = 0;
    // pL = 0;
    // pR = 0;
    // eL = pL/(rhoL*(GAMMA-1));
    // eR = pR/(rhoR*(GAMMA-1));
    // TL = eL/Cv;
    // TR = eR/Cv;
    TL = 1;
    TR = 1;
    break;
    case 1:
    // Simple problem for testing
    problemName = "SimpleRarefaction";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.1;

    // GAMMA = 1.4;
    rhoL = 1;
    rhoR = 1;
    uL = -1;
    uR = 1;
    // pL = 0;
    // pR = 0;
    // eL = pL/(rhoL*(GAMMA-1));
    // eR = pR/(rhoR*(GAMMA-1));
    // TL = eL/Cv;
    // TR = eR/Cv;
    TL = 1;
    TR = 1;
    break;
    case 2:
    // Sod shock tube
    problemName = "Sod";
    xmin = 0;
    xmax = 1;
    xint = 0.5;
    tmax = 0.2;

    // GAMMA = 1.4;
    rhoL = 1;
    rhoR = .125;
    uL = 0;
    uR = 0;
    // pL = 0;
    // pR = 0;
    // eL = pL/(rhoL*(GAMMA-1));
    // eR = pR/(rhoR*(GAMMA-1));
    // TL = eL/Cv;
    // TR = eR/Cv;
    TL = 1/rhoL;
    TR = .1/rhoR;
    break;
    default:
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "invalid problem number");
  }
  if (commRank == 0)
    cout << "Running the " << problemName << " problem" << endl;

  ////////////////////   DECLARE VARIABLES   ///////////////////////
  // define test variables
  VarFactory varFactory;
  VarPtr vc = varFactory.testVar("vc", HGRAD);
  VarPtr vm = varFactory.testVar("vm", HGRAD);
  VarPtr ve = varFactory.testVar("ve", HGRAD);

  // define trial variables
  VarPtr rho = varFactory.fieldVar("rho");
  VarPtr u = varFactory.fieldVar("u");
  VarPtr T = varFactory.fieldVar("T");
  VarPtr Fc = varFactory.fluxVar("Fc");
  VarPtr Fm = varFactory.fluxVar("Fm");
  VarPtr Fe = varFactory.fluxVar("Fe");

  ////////////////////   INITIALIZE USEFUL VARIABLES   ///////////////////////
  // Define useful functions
  FunctionPtr one = Function::constant(1.0);
  FunctionPtr zero = Function::zero();

  // Initialize useful variables
  Teuchos::RCP<BCEasy> bc = Teuchos::rcp( new BCEasy );
  BFPtr bf = Teuchos::rcp( new BF(varFactory) );

  ////////////////////   BUILD MESH   ///////////////////////
  FieldContainer<double> meshBoundary(4,2);

  meshBoundary(0,0) =  xmin; // x1
  meshBoundary(0,1) =  0; // y1
  meshBoundary(1,0) =  xmax;
  meshBoundary(1,1) =  0;
  meshBoundary(2,0) =  xmax;
  meshBoundary(2,1) =  tmax;
  meshBoundary(3,0) =  xmin;
  meshBoundary(3,1) =  tmax;

  MeshPtr mesh = Mesh::buildQuadMesh(meshBoundary, xCells, tCells,
      bf, H1Order, H1Order+pToAdd, false);

  ////////////////////   SET INITIAL CONDITIONS   ///////////////////////
  BCPtr nullBC = Teuchos::rcp((BC*)NULL);
  RHSPtr nullRHS = Teuchos::rcp((RHS*)NULL);
  IPPtr nullIP = Teuchos::rcp((IP*)NULL);
  SolutionPtr backgroundFlow = Teuchos::rcp(new Solution(mesh, nullBC, nullRHS, nullIP) );

  map<int, Teuchos::RCP<Function> > initialConditions;
  // initialConditions[rho->ID()] = Teuchos::rcp( new DiscontinuousInitialCondition(xint, rhoL, rhoR) ) ;
  // initialConditions[u->ID()]   = Teuchos::rcp( new DiscontinuousInitialCondition(xint, uL, uR) );
  // initialConditions[T->ID()]   = Teuchos::rcp( new DiscontinuousInitialCondition(xint, TL, TR) );
  initialConditions[rho->ID()] = Teuchos::rcp( new RampedInitialCondition(xint, rhoL, rhoR, (xmax-xmin)/xCells/pow(2.,numPreRefs)) ) ;
  initialConditions[u->ID()]   = Teuchos::rcp( new RampedInitialCondition(xint, uL, uR,     (xmax-xmin)/xCells/pow(2.,numPreRefs)) );
  initialConditions[T->ID()]   = Teuchos::rcp( new RampedInitialCondition(xint, TL, TR,     (xmax-xmin)/xCells/pow(2.,numPreRefs)) );

  backgroundFlow->projectOntoMesh(initialConditions);

  ////////////////////   DEFINE BILINEAR FORM   ///////////////////////
  // Set up problem

  R = 1;
  Cv = 1;
  Cp = 1;
  Pr = 1;

  FunctionPtr rho_prev = Function::solution(rho, backgroundFlow);
  FunctionPtr u_prev   = Function::solution(u, backgroundFlow);
  FunctionPtr T_prev   = Function::solution(T, backgroundFlow);

  // vc terms:
  bf->addTerm( -rho_prev*u, vc->dx());
  bf->addTerm( -u_prev*rho, vc->dx());
  bf->addTerm( -rho, vc->dy());
  bf->addTerm( Fc, vc);

  // vm terms:
  bf->addTerm( -rho_prev*u_prev*u, vm->dx());
  bf->addTerm( -rho_prev*u_prev*u, vm->dx());
  bf->addTerm( -u_prev*u_prev*rho, vm->dx());
  bf->addTerm( -R*rho_prev*T, vm->dx());
  bf->addTerm( -R*T_prev*rho, vm->dx());
  bf->addTerm( -rho_prev*u, vm->dy());
  bf->addTerm( -u_prev*rho, vm->dy());
  bf->addTerm( Fm, vm);

  // ve terms:
  bf->addTerm( -Cv*rho_prev*T_prev*u, ve->dx());
  bf->addTerm( -Cv*u_prev*rho_prev*T, ve->dx());
  bf->addTerm( -Cv*T_prev*u_prev*rho, ve->dx());
  bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
  bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
  bf->addTerm( -0.5*rho_prev*u_prev*u_prev*u, ve->dx());
  bf->addTerm( -0.5*u_prev*u_prev*u_prev*rho, ve->dx());
  bf->addTerm( -R*rho_prev*T_prev*u, ve->dx());
  bf->addTerm( -R*rho_prev*u_prev*T, ve->dx());
  bf->addTerm( -R*u_prev*T_prev*rho, ve->dx());
  bf->addTerm( -Cv*rho_prev*T, ve->dy());
  bf->addTerm( -Cv*T_prev*rho, ve->dy());
  bf->addTerm( -0.5*rho_prev*u_prev*u, ve->dy());
  bf->addTerm( -0.5*rho_prev*u_prev*u, ve->dy());
  bf->addTerm( -0.5*u_prev*u_prev*rho, ve->dy());
  bf->addTerm( Fe, ve);

  ////////////////////   SPECIFY RHS   ///////////////////////
  Teuchos::RCP<RHSEasy> rhs = Teuchos::rcp( new RHSEasy );

  // vc terms:
  rhs->addTerm( rho_prev*u_prev * vc->dx() );
  rhs->addTerm( rho_prev * vc->dy() );

  // vc terms:
  rhs->addTerm( rho_prev*u_prev*u_prev * vm->dx() );
  rhs->addTerm( R*rho_prev*T_prev * vm->dx() );
  rhs->addTerm( rho_prev*u_prev * vm->dy() );

  // ve terms:
  rhs->addTerm( Cv*rho_prev*u_prev*T_prev * ve->dx() );
  rhs->addTerm( 0.5*rho_prev*u_prev*u_prev*u_prev * ve->dx() );
  rhs->addTerm( R*rho_prev*u_prev*T_prev * ve->dx() );
  rhs->addTerm( Cv*rho_prev*T_prev * ve->dy() );
  rhs->addTerm( 0.5*rho_prev*u_prev*u_prev * ve->dy() );

  ////////////////////   DEFINE INNER PRODUCT(S)   ///////////////////////
  // Automatic graph norm
  IPPtr ip = Teuchos::rcp(new IP);
  if (norm == 0)
  {
    ip = bf->graphNorm();
  }
  // Manual Graph norm
  else if (norm == 1)
  {
    ip->addTerm(u_prev*vc->dx()+vc->dy()+u_prev*u_prev*vm->dx()+R*T_prev*vm->dx()+u_prev*vm->dy()
      +Cv*T_prev*u_prev*ve->dx()+0.5*u_prev*u_prev*u_prev*ve->dx()+R*T_prev*u_prev*ve->dx()+Cv*T_prev*ve->dy()+0.5*u_prev*u_prev*ve->dy());
    ip->addTerm(rho_prev*vc->dx()+rho_prev*u_prev*vm->dx()+rho_prev*u_prev*vm->dx()+rho_prev*vm->dy()+Cv*rho_prev*T_prev*ve->dx()
      +0.5*rho_prev*u_prev*u_prev*ve->dx()+0.5*rho_prev*u_prev*u_prev*ve->dx()+0.5*rho_prev*u_prev*u_prev*ve->dx()
      +R*rho_prev*T_prev*ve->dx()+0.5*rho_prev*u_prev*ve->dy()+0.5*rho_prev*u_prev*ve->dy());
    ip->addTerm(R*rho_prev*vm->dx()+Cv*rho_prev*u_prev*ve->dx()+R*rho_prev*u_prev*ve->dx()+Cv*rho_prev*ve->dy());
    ip->addTerm(vc);
    ip->addTerm(vm);
    ip->addTerm(ve);
  }
  // // Decoupled Eulerian and viscous norm
  // else if (norm == 2)
  // {
  //   ip->addTerm(vm->dx() + u_prev*ve->dx());
  //   ip->addTerm(ve->dx());
  //   ip->addTerm(u_prev*vc->dx()+vc->dy()+u_prev*u_prev*vm->dx()+R*T_prev*vm->dx()+u_prev*vm->dy()
  //     +Cv*T_prev*u_prev*ve->dx()+0.5*u_prev*u_prev*u_prev*ve->dx()+R*T_prev*u_prev*ve->dx()+Cv*T_prev*ve->dy()+0.5*u_prev*u_prev*ve->dy());
  //   ip->addTerm(rho_prev*vc->dx()+rho_prev*u_prev*vm->dx()+rho_prev*u_prev*vm->dx()+rho_prev*vm->dy()+Cv*rho_prev*T_prev*ve->dx()
  //     +0.5*rho_prev*u_prev*u_prev*ve->dx()+0.5*rho_prev*u_prev*u_prev*ve->dx()+0.5*rho_prev*u_prev*u_prev*ve->dx()
  //     +R*rho_prev*T_prev*ve->dx()-D_prev*ve->dx()+0.5*rho_prev*u_prev*ve->dy()+0.5*rho_prev*u_prev*ve->dy());
  //   ip->addTerm(R*rho_prev*vm->dx()+Cv*rho_prev*u_prev*ve->dx()+R*rho_prev*u_prev*ve->dx()+Cv*rho_prev*ve->dy());
  //   ip->addTerm(1./mu*S);
  //   ip->addTerm(Pr/(Cp*mu)*tau);
  //   ip->addTerm(vc);
  //   ip->addTerm(vm);
  //   ip->addTerm(ve);
  // }

  ////////////////////   CREATE BCs   ///////////////////////
  SpatialFilterPtr left = Teuchos::rcp( new ConstantXBoundary(xmin) );
  SpatialFilterPtr right = Teuchos::rcp( new ConstantXBoundary(xmax) );
  SpatialFilterPtr init = Teuchos::rcp( new ConstantYBoundary(0) );
  // FunctionPtr rho0  = Teuchos::rcp( new DiscontinuousInitialCondition(xint, rhoL, rhoR) );
  // FunctionPtr mom0 = Teuchos::rcp( new DiscontinuousInitialCondition(xint, uL*rhoL, uR*rhoR) );
  // FunctionPtr E0    = Teuchos::rcp( new DiscontinuousInitialCondition(xint, (rhoL*Cv*TL+0.5*rhoL*uL*uL), (rhoR*Cv*TR+0.5*rhoR*uR*uR)) );
  FunctionPtr rho0  = Teuchos::rcp( new RampedInitialCondition(xint, rhoL, rhoR, (xmax-xmin)/xCells/pow(2.,numPreRefs)) );
  FunctionPtr mom0 = Teuchos::rcp( new RampedInitialCondition(xint, uL*rhoL, uR*rhoR, (xmax-xmin)/xCells/pow(2.,numPreRefs)) );
  FunctionPtr E0    = Teuchos::rcp( new RampedInitialCondition(xint, (rhoL*Cv*TL+0.5*rhoL*uL*uL), (rhoR*Cv*TR+0.5*rhoR*uR*uR), (xmax-xmin)/xCells/pow(2.,numPreRefs)) );
  bc->addDirichlet(Fc, init, -rho0);
  bc->addDirichlet(Fm, init, -mom0);
  bc->addDirichlet(Fe, init, -E0);
  bc->addDirichlet(Fc, left, -rhoL*uL*one);
  bc->addDirichlet(Fc, right, rhoR*uR*one);
  bc->addDirichlet(Fm, left, -(rhoL*uL*uL+R*rhoL*TL)*one);
  bc->addDirichlet(Fm, right, (rhoR*uR*uR+R*rhoR*TR)*one);
  bc->addDirichlet(Fe, left, -(rhoL*Cv*TL+0.5*rhoL*uL*uL+R*Cv*TL)*uL*one);
  bc->addDirichlet(Fe, right, (rhoR*Cv*TR+0.5*rhoR*uR*uR+R*Cv*TL)*uR*one);

  ////////////////////   SOLVE & REFINE   ///////////////////////
  Teuchos::RCP<Solution> solution = Teuchos::rcp( new Solution(mesh, bc, rhs, ip) );
  mesh->registerSolution(backgroundFlow);
  mesh->registerSolution(solution);
  double energyThreshold = 0.2; // for mesh refinements
  RefinementStrategy refinementStrategy( solution, energyThreshold );
  VTKExporter exporter(backgroundFlow, mesh, varFactory);
  set<int> nonlinearVars;
  nonlinearVars.insert(rho->ID());
  nonlinearVars.insert(u->ID());
  nonlinearVars.insert(T->ID());

  if (commRank==0){
    cout << "Number of pre-refinements = " << numPreRefs << endl;
  }
  for (int i =0;i<=numPreRefs;i++){
    vector<ElementPtr> elems = mesh->activeElements();
    vector<ElementPtr>::iterator elemIt;
    vector<int> pointCells;
    for (elemIt=elems.begin();elemIt != elems.end();elemIt++){
      int cellID = (*elemIt)->cellID();
      int numSides = mesh->getElement(cellID)->numSides();
      FieldContainer<double> vertices(numSides,2); //for quads

      mesh->verticesForCell(vertices, cellID);
      bool cellIDset = false;
      for (int j = 0;j<numSides;j++){ // num sides = 4
        if ((abs(vertices(j,0)-xint)<1e-7) && (abs(vertices(j,1))<1e-7) && !cellIDset)
        {
          pointCells.push_back(cellID);
          cellIDset = true;
        }
      }
    }
    if (i<numPreRefs){
      refinementStrategy.refineCells(pointCells);
    }
  }

  double nonlinearRelativeEnergyTolerance = 1e-5; // used to determine convergence of the nonlinear solution
  for (int refIndex=0; refIndex<=numRefs; refIndex++)
  {
    double L2Update = 1e7;
    int iterCount = 0;
    while (L2Update > nonlinearRelativeEnergyTolerance && iterCount < maxNewtonIterations)
    {
      solution->solve(false);
      double rhoL2Update = solution->L2NormOfSolutionGlobal(rho->ID());
      double uL2Update = solution->L2NormOfSolutionGlobal(u->ID());
      double TL2Update = solution->L2NormOfSolutionGlobal(T->ID());
      L2Update = sqrt(rhoL2Update*rhoL2Update + uL2Update*uL2Update + TL2Update*TL2Update);

      // line search algorithm
      double alpha = 1.0;
      bool useLineSearch = true;
      // amount of enriching of grid points on which to ensure positivity
      int posEnrich = 5; 
      if (useLineSearch)
      {
        double lineSearchFactor = .5; 
        double eps = .001;
        FunctionPtr rhoTemp = Function::solution(rho,backgroundFlow) + alpha*Function::solution(rho,solution) - Function::constant(eps);
        FunctionPtr TTemp = Function::solution(T,backgroundFlow) + alpha*Function::solution(T,solution) - Function::constant(eps);
        bool rhoIsPositive = rhoTemp->isPositive(mesh,posEnrich);
        bool TIsPositive = TTemp->isPositive(mesh,posEnrich);
        int iter = 0; int maxIter = 20;
        while (!(rhoIsPositive && TIsPositive) && iter < maxIter)
        {
          alpha = alpha*lineSearchFactor;
          rhoTemp = Function::solution(rho,backgroundFlow) + alpha*Function::solution(rho,solution);
          TTemp = Function::solution(T,backgroundFlow) + alpha*Function::solution(T,solution);
          rhoIsPositive = rhoTemp->isPositive(mesh,posEnrich);
          TIsPositive = TTemp->isPositive(mesh,posEnrich);
          iter++;
        }
        if (commRank==0 && alpha < 1.0){
          cout << "line search factor alpha = " << alpha << endl;
        }
      }

      backgroundFlow->addSolution(solution, alpha, nonlinearVars);
      iterCount++;
      if (commRank == 0)
        cout << "L2 Norm of Update = " << L2Update << endl;
      if (alpha < 1e-2)
        break;
    }
    if (commRank == 0)
      cout << endl;

    if (commRank == 0)
    {
      stringstream outfile;
      outfile << "Euler_" << problemName << "_" << refIndex;
      exporter.exportSolution(outfile.str());
    }

    if (refIndex < numRefs)
    {
      refinementStrategy.refine(commRank==0);
      double newRamp = (xmax-xmin)/(xCells*pow(2., numPreRefs+refIndex+1));
      // if (commRank == 0)
      //   cout << "New ramp width = " << newRamp << endl;
      dynamic_cast< RampedInitialCondition* >(initialConditions[rho->ID()].get())->setH(newRamp);
      dynamic_cast< RampedInitialCondition* >(initialConditions[u->ID()].get())->setH(newRamp);
      dynamic_cast< RampedInitialCondition* >(initialConditions[T->ID()].get())->setH(newRamp);
      dynamic_cast< RampedInitialCondition* >(rho0.get())->setH(newRamp);
      dynamic_cast< RampedInitialCondition* >(mom0.get())->setH(newRamp);
      dynamic_cast< RampedInitialCondition* >(E0.get())->setH(newRamp);
      // backgroundFlow->projectOntoMesh(initialConditions);
    }
  }

  return 0;
}
