#ifndef HDF5EXPORTER_H
#define HDF5EXPORTER_H

/*
 *  HDF5Exporter.h
 *
 *  Created by Truman Ellis on 6/24/2014.
 *
 */


#include "EpetraExt_ConfigDefs.h"
#ifdef HAVE_EPETRAEXT_HDF5

#include "Solution.h"
#include "Mesh.h"
#include "MeshTopology.h"
#include "VarFactory.h"

#include <Teuchos_XMLObject.hpp>

class HDF5Exporter {
private:
  string _dirName;
  string _dirSuperPath;
  MeshPtr _mesh;
  XMLObject _fieldXdmf;
  XMLObject _traceXdmf;
  XMLObject _fieldDomain;
  XMLObject _traceDomain;
  XMLObject _fieldGrids;
  XMLObject _traceGrids;
  set<double> _fieldTimeVals;
  set<double> _traceTimeVals;
public:
  HDF5Exporter(MeshPtr mesh, string outputDirName="output", string outputDirSuperPath = ".");
  ~HDF5Exporter();
  void setMesh(MeshPtr mesh) {_mesh = mesh;}
  void exportFunction(FunctionPtr function, string functionName="function", double timeVal=0,
    unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>());
  void exportFunction(vector<FunctionPtr> functions, vector<string> functionNames, double timeVal=0,
    unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>());
  void exportSolution(SolutionPtr solution, double timeVal=0, unsigned int defaultNum1DPts=4,
                      map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>());
  void exportTimeSlab(FunctionPtr function, string functionName="function", double tInit=0, double tFinal=1, unsigned int numSlices=2,
    unsigned int sliceH1Order=2, unsigned int defaultNum1DPts=4);
  void exportTimeSlab(vector<FunctionPtr> functions, vector<string> functionNames, double tInit=0, double tFinal=1, unsigned int numSlices=2,
    unsigned int sliceH1Order=2, unsigned int defaultNum1DPts=4);

  // DEPRECATED METHOD:
  void exportSolution(SolutionPtr solution, VarFactory varFactory, double timeVal=0,
                      unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>());
  
  // allows one-line export without storing an exporter object
  static void exportFunction(string directoryPath, string functionName, FunctionPtr function, MeshPtr mesh); 
  // allows one-line export without storing an exporter object 
  static void exportSolution(string directoryPath, string solutionName, SolutionPtr solution); 
};

// creates a map from cell index to number of 1D points (number of subdivisions + 1)
// num1DPts = max(subdivisionFactor*(polyOrder-1) + 1, 2)
map<int,int> cellIDToSubdivision(MeshPtr mesh, unsigned int subdivisionFactor=2, set<GlobalIndexType> cellIndices=set<GlobalIndexType>());

#else

/* DUMMY (NO-OP) IMPLEMENTATION FOR WHEN HDF5 IS UNAVAILABLE */
class HDF5Exporter {
private:
public:
  HDF5Exporter(MeshPtr mesh, string saveDirectory="output") {}
  ~HDF5Exporter() {}
  void exportFunction(FunctionPtr function, string functionName="function", double timeVal=0,
                      unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>()) {}
  void exportFunction(vector<FunctionPtr> functions, vector<string> functionNames, double timeVal=0,
                      unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>()) {}
  void exportSolution(SolutionPtr solution, VarFactory varFactory, double timeVal=0,
                      unsigned int defaultNum1DPts=4, map<int, int> cellIDToNum1DPts=map<int,int>(), set<GlobalIndexType> cellIndices=set<GlobalIndexType>()) {}
  static void exportSolution(string saveDirectory, SolutionPtr solution) {}
};
/* END OF DUMMY IMPLEMENTATION */

#endif

#endif /* end of include guard: HDF5EXPORTER_H */
