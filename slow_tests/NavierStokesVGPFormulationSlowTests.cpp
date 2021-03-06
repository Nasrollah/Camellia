//
//  NavierStokesVGPFormulationTests.cpp
//  Camellia
//
//  Created by Nate Roberts on 2/5/15.
//
//

#include "NavierStokesVGPFormulation.h"
#include "MeshFactory.h"

#include "HDF5Exporter.h"

#include "Teuchos_UnitTestHarness.hpp"
namespace {
  TEUCHOS_UNIT_TEST( NavierStokesVGPFormulation, ExactSolution_2D )
  {
    int spaceDim = 2;
    vector<double> dimensions(spaceDim,2.0); // 2x2 square domain
    vector<int> elementCounts(spaceDim,1); // 1 x 1 mesh
    vector<double> x0(spaceDim,-1.0);
    MeshTopologyPtr meshTopo = MeshFactory::rectilinearMeshTopology(dimensions, elementCounts, x0);
    double Re = 1.0;
    int fieldPolyOrder = 3, delta_k = 1;
    
    FunctionPtr x = Function::xn(1);
    FunctionPtr y = Function::yn(1);
    
    FunctionPtr u1 = Function::constant(1.0); // Function::zero(); // x * x * y;
    FunctionPtr u2 = x; // Function::zero(); //-x * y * y;
    FunctionPtr p = Function::zero();
    
    FunctionPtr forcingFunction = NavierStokesVGPFormulation::forcingFunction(spaceDim, Re, Function::vectorize(u1,u2), p);
    NavierStokesVGPFormulation form(meshTopo, Re, fieldPolyOrder, delta_k, forcingFunction);
    
    form.addInflowCondition(SpatialFilter::allSpace(), Function::vectorize(u1, u2));
    form.addZeroMeanPressureCondition();
    
    FunctionPtr sigma1 = (1.0 / Re) * u1->grad();
    FunctionPtr sigma2 = (1.0 / Re) * u2->grad();
    
    LinearTermPtr t1_n_lt = form.tn_hat(1)->termTraced();
    LinearTermPtr t2_n_lt = form.tn_hat(2)->termTraced();
    
    map<int, FunctionPtr> exactMap;
    // fields:
    exactMap[form.u(1)->ID()] = u1;
    exactMap[form.u(2)->ID()] = u2;
    exactMap[form.p()->ID() ] =  p;
    exactMap[form.sigma(1)->ID()] = sigma1;
    exactMap[form.sigma(2)->ID()] = sigma2;
    
    // fluxes:
    // use the exact field variable solution together with the termTraced to determine the flux traced
    FunctionPtr t1_n = t1_n_lt->evaluate(exactMap);
    FunctionPtr t2_n = t2_n_lt->evaluate(exactMap);
    exactMap[form.tn_hat(1)->ID()] = t1_n;
    exactMap[form.tn_hat(2)->ID()] = t2_n;
    
    // traces:
    exactMap[form.u_hat(1)->ID()] = u1;
    exactMap[form.u_hat(2)->ID()] = u2;
    
    map<int, FunctionPtr> zeroMap;
    for (map<int, FunctionPtr>::iterator exactMapIt = exactMap.begin(); exactMapIt != exactMap.end(); exactMapIt++) {
      zeroMap[exactMapIt->first] = Function::zero();
    }

    RHSPtr rhsWithoutBoundaryTerms = form.solutionIncrement()->rhs();
    RHSPtr rhsWithBoundaryTerms = form.rhs(forcingFunction, false); // false: *include* boundary terms in the RHS -- important for computing energy error correctly

    double tol = 1e-12;
    
    // sanity/consistency check: is the energy error for a zero solutionIncrement zero?
    form.solutionIncrement()->projectOntoMesh(zeroMap);
    form.solution()->projectOntoMesh(exactMap);
    form.solutionIncrement()->setRHS(rhsWithBoundaryTerms);
    double energyError = form.solutionIncrement()->energyErrorTotal();

    TEST_COMPARE(energyError, <, tol);
    
    // change RHS back for solve below:
    form.solutionIncrement()->setRHS(rhsWithoutBoundaryTerms);
    
    // first real test: with exact background flow, if we solve, do we maintain zero energy error?
    form.solveAndAccumulate();
    form.solutionIncrement()->setRHS(rhsWithBoundaryTerms);
    form.solutionIncrement()->projectOntoMesh(zeroMap); // zero out since we've accumulated
    energyError = form.solutionIncrement()->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
    // change RHS back for solve below:
    form.solutionIncrement()->setRHS(rhsWithoutBoundaryTerms);
    
    // next test: try starting from a zero initial guess
    form.solution()->projectOntoMesh(zeroMap);
    
    SolutionPtr solnIncrement = form.solutionIncrement();
    
    FunctionPtr u1_incr = Function::solution(form.u(1), solnIncrement);
    FunctionPtr u2_incr = Function::solution(form.u(2), solnIncrement);
    FunctionPtr p_incr = Function::solution(form.p(), solnIncrement);
    
    FunctionPtr l2_incr = u1_incr * u1_incr + u2_incr * u2_incr + p_incr * p_incr;
    
    double l2_norm_incr = 0.0;
    double nonlinearTol = 1e-10;
    int maxIters = 10;
    do {
      form.solveAndAccumulate();
      l2_norm_incr = sqrt(l2_incr->integrate(solnIncrement->mesh()));
      out << "iteration " << form.nonlinearIterationCount() << ", L^2 norm of increment: " << l2_norm_incr << endl;
    } while ((l2_norm_incr > nonlinearTol) && (form.nonlinearIterationCount() < maxIters));
    
    form.solutionIncrement()->setRHS(rhsWithBoundaryTerms);
    form.solutionIncrement()->projectOntoMesh(zeroMap); // zero out since we've accumulated
    energyError = form.solutionIncrement()->energyErrorTotal();
    TEST_COMPARE(energyError, <, tol);
    
//    if (energyError >= tol) {
//      HDF5Exporter::exportSolution("/tmp", "NSVGP_background_flow",form.solution());
//      HDF5Exporter::exportSolution("/tmp", "NSVGP_soln_increment",form.solutionIncrement());
//    }
  }
} // namespace