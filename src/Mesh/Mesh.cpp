// @HEADER
//
// Original Version Copyright © 2011 Sandia Corporation. All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are 
// permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of 
// conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of 
// conditions and the following disclaimer in the documentation and/or other materials 
// provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products derived from 
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY 
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR 
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
// OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Nate Roberts (nate@nateroberts.com).
//
// @HEADER 

/*
 *  Mesh.cpp
 *
 *  Created by Nathan Roberts on 6/27/11.
 *
 */

#include "Mesh.h"
#include "BilinearFormUtility.h"
#include "ElementType.h"
#include "DofOrderingFactory.h"
#include "BasisFactory.h"
#include "BasisCache.h"

#include "Solution.h"

#include "MeshTransformationFunction.h"

#include "CamelliaCellTools.h"

// Teuchos includes
#include "Teuchos_RCP.hpp"
#ifdef HAVE_MPI
#include <Teuchos_GlobalMPISession.hpp>
#endif

using namespace Intrepid;

map<int,int> Mesh::_emptyIntIntMap;

Mesh::Mesh(const vector<vector<double> > &vertices, vector< vector<unsigned> > &elementVertices,
           Teuchos::RCP< BilinearForm > bilinearForm, int H1Order, int pToAddTest, bool useConformingTraces,
           map<int,int> trialOrderEnhancements, map<int,int> testOrderEnhancements)
: _dofOrderingFactory(bilinearForm, trialOrderEnhancements,testOrderEnhancements) {

  MeshGeometryPtr meshGeometry = Teuchos::rcp( new MeshGeometry(vertices,elementVertices) );
  _meshTopology = Teuchos::rcp( new MeshTopology(meshGeometry) );
  
  DofOrderingFactoryPtr dofOrderingFactoryPtr = Teuchos::rcp( &_dofOrderingFactory, false);
  
  
  _useConformingTraces = useConformingTraces;
  _usePatchBasis = false;
  _enforceMBFluxContinuity = false;  
  _partitionPolicy = Teuchos::rcp( new MeshPartitionPolicy() );

  _maximumRule2D = Teuchos::rcp( new GDAMaximumRule2D(_meshTopology, bilinearForm->varFactory(), dofOrderingFactoryPtr,
                                                      _partitionPolicy, H1Order, pToAddTest, _enforceMBFluxContinuity) );
  
#ifdef HAVE_MPI
  _numPartitions = Teuchos::GlobalMPISession::getNProc();
#else
  _numPartitions = 1;
#endif
  
  // DEBUGGING: check how we did:
  int numVertices = vertices.size();
  for (int vertexIndex=0; vertexIndex<numVertices; vertexIndex++ ) {
    vector<double> vertex = _meshTopology->getVertex(vertexIndex);

    unsigned assignedVertexIndex;
    bool vertexFound = _meshTopology->getVertexIndex(vertex, assignedVertexIndex);
    
    if (!vertexFound) {
      cout << "INTERNAL ERROR: vertex not found by vertex lookup.\n";
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "internal error");
    }
    
    if (assignedVertexIndex != vertexIndex) {
      cout << "INTERNAL ERROR: assigned vertex index is incorrect.\n";
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "internal error");
    }
  }
                                      
  _bilinearForm = bilinearForm;
  _boundary.setMesh(this);
  
  _pToAddToTest = pToAddTest;
  int pTest = H1Order + pToAddTest;
  
  Teuchos::RCP<shards::CellTopology> triTopoPtr, quadTopoPtr;
  quadTopoPtr = Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<4> >() ));
  triTopoPtr = Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Triangle<3> >() ));
  
  Teuchos::RCP<DofOrdering> quadTestOrderPtr = _dofOrderingFactory.testOrdering(pTest, *(quadTopoPtr.get()));
  Teuchos::RCP<DofOrdering> quadTrialOrderPtr = _dofOrderingFactory.trialOrdering(H1Order, *(quadTopoPtr.get()), _useConformingTraces);
  ElementTypePtr quadElemTypePtr = _elementTypeFactory.getElementType(quadTrialOrderPtr, quadTestOrderPtr, quadTopoPtr );
  _nullPtr = Teuchos::rcp( new Element(-1,quadElemTypePtr,-1) );
  
  vector< vector<unsigned> >::iterator elemIt;
  for (elemIt = elementVertices.begin(); elemIt != elementVertices.end(); elemIt++) {
    vector<unsigned> thisElementVertices = *elemIt;
    if (thisElementVertices.size() == 3) {
      Teuchos::RCP<DofOrdering> triTestOrderPtr = _dofOrderingFactory.testOrdering(pTest, *(triTopoPtr.get()));
      Teuchos::RCP<DofOrdering> triTrialOrderPtr = _dofOrderingFactory.trialOrdering(H1Order, *(triTopoPtr.get()), _useConformingTraces);
      ElementTypePtr triElemTypePtr = _elementTypeFactory.getElementType(triTrialOrderPtr, triTestOrderPtr, triTopoPtr );
      addElement(thisElementVertices,triElemTypePtr);
    } else if (thisElementVertices.size() == 4) {
      addElement(thisElementVertices,quadElemTypePtr);
    } else {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Only elements with 3 or 4 vertices are supported.");
    }
  }
  rebuildLookups();
  _numInitialElements = (int)numElements();
}

int Mesh::numInitialElements(){
  return _numInitialElements;
}

int Mesh::activeCellOffset() {
  return _activeCellOffset;
}

vector< Teuchos::RCP< Element > > & Mesh::activeElements() {
  return _activeElements;
}

Teuchos::RCP<Mesh> Mesh::readMsh(string filePath, Teuchos::RCP< BilinearForm > bilinearForm, int H1Order, int pToAdd)
{
  ifstream mshFile;
  mshFile.open(filePath.c_str());
  TEUCHOS_TEST_FOR_EXCEPTION(mshFile == NULL, std::invalid_argument, "Could not open msh file");
  string line;
  getline(mshFile, line);
  while (line != "$Nodes")
  {
    getline(mshFile, line);
  }
  int numNodes;
  mshFile >> numNodes;
  vector<vector<double> > vertices;
  int dummy;
  for (int i=0; i < numNodes; i++)
  {
    vector<double> vertex(2);
    mshFile >> dummy;
    mshFile >> vertex[0] >> vertex[1] >> dummy;
    vertices.push_back(vertex);
  }
  while (line != "$Elements")
  {
    getline(mshFile, line);
  }
  int numElems;
  mshFile >> numElems;
  int elemType;
  int numTags;
  vector< vector<unsigned> > elementIndices;
  for (int i=0; i < numElems; i++)
  {
    mshFile >> dummy >> elemType >> numTags;
    for (int j=0; j < numTags; j++)
      mshFile >> dummy;
    if (elemType == 2)
    {
      vector<unsigned> elemIndices(3);
      mshFile >> elemIndices[0] >> elemIndices[1] >> elemIndices[2];
      elemIndices[0]--;
      elemIndices[1]--;
      elemIndices[2]--;
      elementIndices.push_back(elemIndices);
    }
    if (elemType == 4)
    {
      vector<unsigned> elemIndices(3);
      mshFile >> elemIndices[0] >> elemIndices[1] >> elemIndices[2];
      elemIndices[0]--;
      elemIndices[1]--;
      elemIndices[2]--;
      elementIndices.push_back(elemIndices);
    }
    else
    {
      getline(mshFile, line);
    }
  }
  mshFile.close();

  Teuchos::RCP<Mesh> mesh = Teuchos::rcp( new Mesh(vertices, elementIndices, bilinearForm, H1Order, pToAdd) );  
  return mesh;
}

Teuchos::RCP<Mesh> Mesh::readTriangle(string filePath, Teuchos::RCP< BilinearForm > bilinearForm, int H1Order, int pToAdd)
{
  ifstream nodeFile;
  ifstream eleFile;
  string nodeFileName = filePath+".node";
  string eleFileName = filePath+".ele";
  nodeFile.open(nodeFileName.c_str());
  eleFile.open(eleFileName.c_str());
  TEUCHOS_TEST_FOR_EXCEPTION(nodeFile == NULL, std::invalid_argument, "Could not open node file: "+nodeFileName);
  TEUCHOS_TEST_FOR_EXCEPTION(eleFile == NULL, std::invalid_argument, "Could not open ele file: "+eleFileName);
  // Read node file
  string line;
  int numNodes;
  nodeFile >> numNodes;
  getline(nodeFile, line);
  vector<vector<double> > vertices;
  int dummy;
  int spaceDim = 2;
  vector<double> pt(spaceDim);
  for (int i=0; i < numNodes; i++)
  {
    nodeFile >> dummy >> pt[0] >> pt[1];
    getline(nodeFile, line);
    vertices.push_back(pt);
  }
  nodeFile.close();
  // Read ele file
  int numElems;
  eleFile >> numElems;
  getline(eleFile, line);
  vector< vector<unsigned> > elementIndices;
  vector<unsigned> el(3);
  for (int i=0; i < numElems; i++)
  {
    eleFile >> dummy >> el[0] >> el[1] >> el[2];
    el[0]--;
    el[1]--;
    el[2]--;
    elementIndices.push_back(el);
  }
  eleFile.close();

  Teuchos::RCP<Mesh> mesh = Teuchos::rcp( new Mesh(vertices, elementIndices, bilinearForm, H1Order, pToAdd) );  
  return mesh;
}

Teuchos::RCP<Mesh> Mesh::buildQuadMesh(const FieldContainer<double> &quadBoundaryPoints, 
                                       int horizontalElements, int verticalElements,
                                       Teuchos::RCP< BilinearForm > bilinearForm, 
                                       int H1Order, int pTest, bool triangulate, bool useConformingTraces,
                                       map<int,int> trialOrderEnhancements,
                                       map<int,int> testOrderEnhancements) {
//  if (triangulate) cout << "Mesh: Triangulating\n" << endl;
  int pToAddToTest = pTest - H1Order;
  int spaceDim = 2;
  // rectBoundaryPoints dimensions: (4,2) -- and should be in counterclockwise order
  
  vector<vector<double> > vertices;
  vector< vector<unsigned> > allElementVertices;
  
  TEUCHOS_TEST_FOR_EXCEPTION( ( quadBoundaryPoints.dimension(0) != 4 ) || ( quadBoundaryPoints.dimension(1) != 2 ),
                     std::invalid_argument,
                     "quadBoundaryPoints should be dimensions (4,2), points in ccw order.");
  
  int numElements = horizontalElements * verticalElements;
  if (triangulate) numElements *= 2;
  int numDimensions = 2;
  
  double southWest_x = quadBoundaryPoints(0,0),
  southWest_y = quadBoundaryPoints(0,1),
  southEast_x = quadBoundaryPoints(1,0), 
  southEast_y = quadBoundaryPoints(1,1),
  northEast_x = quadBoundaryPoints(2,0),
  northEast_y = quadBoundaryPoints(2,1),
  northWest_x = quadBoundaryPoints(3,0),
  northWest_y = quadBoundaryPoints(3,1);
  
  double elemWidth = (southEast_x - southWest_x) / horizontalElements;
  double elemHeight = (northWest_y - southWest_y) / verticalElements;
    
  // set up vertices:
  // vertexIndices is for easy vertex lookup by (x,y) index for our Cartesian grid:
  vector< vector<int> > vertexIndices(horizontalElements+1, vector<int>(verticalElements+1));
  for (int i=0; i<=horizontalElements; i++) {
    for (int j=0; j<=verticalElements; j++) {
      vertexIndices[i][j] = vertices.size();
      vector<double> vertex(spaceDim);
      vertex[0] = southWest_x + elemWidth*i;
      vertex[1] = southWest_y + elemHeight*j;
      vertices.push_back(vertex);
//      cout << "Mesh: vertex " << vertices.size() - 1 << ": (" << vertex(0) << "," << vertex(1) << ")\n";
    }
  }
  
  if ( ! triangulate ) {
    int SOUTH = 0, EAST = 1, NORTH = 2, WEST = 3;
    for (int i=0; i<horizontalElements; i++) {
      for (int j=0; j<verticalElements; j++) {
        vector<unsigned> elemVertices;
        elemVertices.push_back(vertexIndices[i][j]);
        elemVertices.push_back(vertexIndices[i+1][j]);
        elemVertices.push_back(vertexIndices[i+1][j+1]);
        elemVertices.push_back(vertexIndices[i][j+1]);
        allElementVertices.push_back(elemVertices);
      }
    }
  } else {
    int SIDE1 = 0, SIDE2 = 1, SIDE3 = 2;
    for (int i=0; i<horizontalElements; i++) {
      for (int j=0; j<verticalElements; j++) {
        // TEST CODE FOR LESZEK: match Leszek's meshes by setting diagonalUp = true here...
        bool diagonalUp = true;
//        bool diagonalUp = (i%2 == j%2); // criss-cross pattern
        
        vector<unsigned> elemVertices1, elemVertices2;
        if (diagonalUp) {
          // elem1 is SE of quad, elem2 is NW
          elemVertices1.push_back(vertexIndices[i][j]);     // SIDE1 is SOUTH side of quad
          elemVertices1.push_back(vertexIndices[i+1][j]);   // SIDE2 is EAST
          elemVertices1.push_back(vertexIndices[i+1][j+1]); // SIDE3 is diagonal
          elemVertices2.push_back(vertexIndices[i][j+1]);   // SIDE1 is WEST
          elemVertices2.push_back(vertexIndices[i][j]);     // SIDE2 is diagonal
          elemVertices2.push_back(vertexIndices[i+1][j+1]); // SIDE3 is NORTH
        } else {
          // elem1 is SW of quad, elem2 is NE
          elemVertices1.push_back(vertexIndices[i][j]);     // SIDE1 is SOUTH side of quad
          elemVertices1.push_back(vertexIndices[i+1][j]);   // SIDE2 is diagonal
          elemVertices1.push_back(vertexIndices[i][j+1]);   // SIDE3 is WEST
          elemVertices2.push_back(vertexIndices[i][j+1]);   // SIDE1 is diagonal
          elemVertices2.push_back(vertexIndices[i+1][j]);   // SIDE2 is EAST
          elemVertices2.push_back(vertexIndices[i+1][j+1]); // SIDE3 is NORTH
        }
        
        allElementVertices.push_back(elemVertices1);
        allElementVertices.push_back(elemVertices2);
      }
    }
  }
  return Teuchos::rcp( new Mesh(vertices,allElementVertices,bilinearForm,H1Order,pToAddToTest,useConformingTraces,
                                trialOrderEnhancements,testOrderEnhancements));
}

Teuchos::RCP<Mesh> Mesh::buildQuadMeshHybrid(const FieldContainer<double> &quadBoundaryPoints, 
                                       int horizontalElements, int verticalElements,
                                       Teuchos::RCP< BilinearForm > bilinearForm, 
                                       int H1Order, int pTest, bool useConformingTraces) {
  int pToAddToTest = pTest - H1Order;
  int spaceDim = 2;
  // rectBoundaryPoints dimensions: (4,2) -- and should be in counterclockwise order
  
  vector<vector<double> > vertices;
  vector< vector<unsigned> > allElementVertices;
  
  TEUCHOS_TEST_FOR_EXCEPTION( ( quadBoundaryPoints.dimension(0) != 4 ) || ( quadBoundaryPoints.dimension(1) != 2 ),
                     std::invalid_argument,
                     "quadBoundaryPoints should be dimensions (4,2), points in ccw order.");
  
  int numDimensions = 2;

  double southWest_x = quadBoundaryPoints(0,0),
  southWest_y = quadBoundaryPoints(0,1),
  southEast_x = quadBoundaryPoints(1,0), 
  southEast_y = quadBoundaryPoints(1,1),
  northEast_x = quadBoundaryPoints(2,0),
  northEast_y = quadBoundaryPoints(2,1),
  northWest_x = quadBoundaryPoints(3,0),
  northWest_y = quadBoundaryPoints(3,1);
  
  double elemWidth = (southEast_x - southWest_x) / horizontalElements;
  double elemHeight = (northWest_y - southWest_y) / verticalElements;
  
  int cellID = 0;
  
  // set up vertices:
  // vertexIndices is for easy vertex lookup by (x,y) index for our Cartesian grid:
  vector< vector<int> > vertexIndices(horizontalElements+1, vector<int>(verticalElements+1));
  for (int i=0; i<=horizontalElements; i++) {
    for (int j=0; j<=verticalElements; j++) {
      vertexIndices[i][j] = vertices.size();
      vector<double> vertex(spaceDim);
      vertex[0] = southWest_x + elemWidth*i;
      vertex[1] = southWest_y + elemHeight*j;
      vertices.push_back(vertex);
    }
  }
  
  int SOUTH = 0, EAST = 1, NORTH = 2, WEST = 3;
  int SIDE1 = 0, SIDE2 = 1, SIDE3 = 2;
  for (int i=0; i<horizontalElements; i++) {
    for (int j=0; j<verticalElements; j++) {
      bool triangulate = (i >= horizontalElements / 2); // triangles on right half of mesh
      if ( ! triangulate ) {
        vector<unsigned> elemVertices;
        elemVertices.push_back(vertexIndices[i][j]);
        elemVertices.push_back(vertexIndices[i+1][j]);
        elemVertices.push_back(vertexIndices[i+1][j+1]);
        elemVertices.push_back(vertexIndices[i][j+1]);
        allElementVertices.push_back(elemVertices);
      } else {
        vector<unsigned> elemVertices1, elemVertices2; // elem1 is SE of quad, elem2 is NW
        elemVertices1.push_back(vertexIndices[i][j]);     // SIDE1 is SOUTH side of quad
        elemVertices1.push_back(vertexIndices[i+1][j]);   // SIDE2 is EAST
        elemVertices1.push_back(vertexIndices[i+1][j+1]); // SIDE3 is diagonal
        elemVertices2.push_back(vertexIndices[i][j+1]);   // SIDE1 is WEST
        elemVertices2.push_back(vertexIndices[i][j]);     // SIDE2 is diagonal
        elemVertices2.push_back(vertexIndices[i+1][j+1]); // SIDE3 is NORTH
        
        allElementVertices.push_back(elemVertices1);
        allElementVertices.push_back(elemVertices2);          
      }
    }
  }
  return Teuchos::rcp( new Mesh(vertices,allElementVertices,bilinearForm,H1Order,pToAddToTest,useConformingTraces));
}

void Mesh::quadMeshCellIDs(FieldContainer<int> &cellIDs, 
                           int horizontalElements, int verticalElements,
                           bool useTriangles) {
  // populates cellIDs with either (h,v) or (h,v,2)
  // where h: horizontalElements (indexed by i, below)
  //       v: verticalElements   (indexed by j)
  //       2: triangles per quad (indexed by k)
  
  TEUCHOS_TEST_FOR_EXCEPTION(cellIDs.dimension(0)!=horizontalElements,
                     std::invalid_argument,
                     "cellIDs should have dimensions: (horizontalElements, verticalElements) or (horizontalElements, verticalElements,2)");
  TEUCHOS_TEST_FOR_EXCEPTION(cellIDs.dimension(1)!=verticalElements,
                     std::invalid_argument,
                     "cellIDs should have dimensions: (horizontalElements, verticalElements) or (horizontalElements, verticalElements,2)");
  if (useTriangles) {
    TEUCHOS_TEST_FOR_EXCEPTION(cellIDs.dimension(2)!=2,
                       std::invalid_argument,
                       "cellIDs should have dimensions: (horizontalElements, verticalElements,2)");
    TEUCHOS_TEST_FOR_EXCEPTION(cellIDs.rank() != 3,
                       std::invalid_argument,
                       "cellIDs should have dimensions: (horizontalElements, verticalElements,2)");
  } else {
    TEUCHOS_TEST_FOR_EXCEPTION(cellIDs.rank() != 2,
                       std::invalid_argument,
                       "cellIDs should have dimensions: (horizontalElements, verticalElements)");
  }
  
  int cellID = 0;
  for (int i=0; i<horizontalElements; i++) {
    for (int j=0; j<verticalElements; j++) {
      if (useTriangles) {
        cellIDs(i,j,0) = cellID++;
        cellIDs(i,j,1) = cellID++;
      } else {
        cellIDs(i,j) = cellID++;
      }
    }
  }
}

void Mesh::addDofPairing(int cellID1, int dofIndex1, int cellID2, int dofIndex2) {
  int firstCellID, firstDofIndex;
  int secondCellID, secondDofIndex;
  if (cellID1 < cellID2) {
    firstCellID = cellID1;
    firstDofIndex = dofIndex1;
    secondCellID = cellID2;
    secondDofIndex = dofIndex2;
  } else if (cellID1 > cellID2) {
    firstCellID = cellID2;
    firstDofIndex = dofIndex2;
    secondCellID = cellID1;
    secondDofIndex = dofIndex1;
  } else { // cellID1 == cellID2
    firstCellID = cellID1;
    secondCellID = cellID1;
    if (dofIndex1 < dofIndex2) {
      firstDofIndex = dofIndex1;
      secondDofIndex = dofIndex2;
    } else if (dofIndex1 > dofIndex2) {
      firstDofIndex = dofIndex2;
      secondDofIndex = dofIndex1;
    } else {
      // if we get here, the following test will throw an exception:
      TEUCHOS_TEST_FOR_EXCEPTION( ( dofIndex1 == dofIndex2 ) && ( cellID1 == cellID2 ),
                         std::invalid_argument,
                         "attempt to identify (cellID1, dofIndex1) with itself.");
      return; // to appease the compiler, which otherwise warns that variables will be used unset...
    }
  }
  pair<int,int> key = make_pair(secondCellID,secondDofIndex);
  pair<int,int> value = make_pair(firstCellID,firstDofIndex);
  if ( _dofPairingIndex.find(key) != _dofPairingIndex.end() ) {
    // we already have an entry for this key: need to fix the linked list so it goes from greatest to least
    pair<int,int> existing = _dofPairingIndex[key];
    pair<int,int> entry1, entry2, entry3;
    entry3 = key; // know this is the greatest of the three
    int existingCellID = existing.first;
    int newCellID = value.first;
    int existingDofIndex = existing.second;
    int newDofIndex = value.second;
    
    if (existingCellID < newCellID) {
      entry1 = existing;
      entry2 = value;
    } else if (existingCellID > newCellID) {
      entry1 = value;
      entry2 = existing;
    } else { // cellID1 == cellID2
      if (existingDofIndex < newDofIndex) {
        entry1 = existing;
        entry2 = value;
      } else if (existingDofIndex > newDofIndex) {
        entry1 = value;
        entry2 = existing;
      } else {
        // existing == value --> we're trying to create an entry that already exists
        return;
      }
    }
    // go entry3 -> entry2 -> entry1 -> whatever entry1 pointed at before
    _dofPairingIndex[entry3] = entry2;
//    cout << "Added DofPairing (cellID,dofIndex) --> (earlierCellID,dofIndex) : (" << entry3.first; 
//    cout << "," << entry3.second << ") --> (";
//    cout << entry2.first << "," << entry2.second << ")." << endl;
    // now, there may already be an entry for entry2, so best to use recursion:
    addDofPairing(entry2.first,entry2.second,entry1.first,entry1.second);
  } else {
    _dofPairingIndex[key] = value;
//    cout << "Added DofPairing (cellID,dofIndex) --> (earlierCellID,dofIndex) : (";
//    cout << secondCellID << "," << secondDofIndex << ") --> (";
//    cout << firstCellID << "," << firstDofIndex << ")." << endl;
  }
}

void Mesh::addChildren(ElementPtr parent, vector< vector<unsigned> > &children, vector< vector< pair< unsigned, unsigned> > > &childrenForSides) {
  // childrenForSides: key is parent's side index, value is (child index in parent, child's side index for the same side)
  
  // probably the first step is to remove the parent's edges.  We will add them back in through the children...
  // second step is to iterate through the children, calling addElement for each once we figure out the right type
  // third step: for neighbors adjacent to more than one child, either assign a multi-basis, or make those neighbor's children our neighbors...
  // probably we want to return the cellIDs for the children, by assigning them in the appropriate vector argument.  But maybe this is not necessary, if we do the job right...
  
  // this assumes 2D...
  // remove parent edges:
  vector<unsigned> parentVertices = this->vertexIndicesForCell(parent->cellID());
  int numVertices = parentVertices.size();
  for (int i=0; i<numVertices; i++) {
    // in 2D, numVertices==numSides, so we can use the vertex index to iterate over sides...
    // check whether parent has just one child on this side: then we need to delete the edge
    // because the child will take over the neighbor relationship
    if ( childrenForSides[i].size() == 1 ) {
      pair<int,int> edge = make_pair(parentVertices[i], parentVertices[ (i+1) % numVertices]);
      // remove this entry from _edgeToCellIDs:
      _edgeToCellIDs.erase(edge);
//      cout << "deleting edge " << edge.first << " --> " << edge.second << endl;
    }
    // also delete this edge from boundary if it happens to be there
    _boundary.deleteElement(parent->cellID(),i);
  }
  
  // work out the correct ElementTypePtr for each child.
  Teuchos::RCP< DofOrdering > parentTrialOrdering = parent->elementType()->trialOrderPtr;
  int pTrial = _dofOrderingFactory.trialPolyOrder(parentTrialOrdering);
  shards::CellTopology parentTopo = * ( parent->elementType()->cellTopoPtr );
  Teuchos::RCP<shards::CellTopology> triTopoPtr, quadTopoPtr;
  quadTopoPtr = Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<4> >() ));
  triTopoPtr = Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Triangle<3> >() ));
  
  // we know that all the children have the same poly. orders on the inside, so
  // the edges shared by the children need not change.  Instead, we change according to 
  // what the parent's neighbor has along the side the child shares with the parent...
  
  int numChildren = children.size();
  Teuchos::RCP<DofOrdering> *childTrialOrders = new Teuchos::RCP<DofOrdering>[numChildren];
  Teuchos::RCP<shards::CellTopology> *childTopos = new Teuchos::RCP<shards::CellTopology>[numChildren];
  Teuchos::RCP<DofOrdering> *childTestOrders = new Teuchos::RCP<DofOrdering>[numChildren];
  vector< vector<int> >::iterator childIt;
  for (int childIndex=0; childIndex<numChildren; childIndex++) {
    if (children[childIndex].size() == 3) {
      childTopos[childIndex] = triTopoPtr;
    } else if (children[childIndex].size() == 4) {
      childTopos[childIndex] = quadTopoPtr;
    } else {
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"child with unhandled # of vertices (not 3 or 4)");
    }
    Teuchos::RCP<DofOrdering> basicTrialOrdering = _dofOrderingFactory.trialOrdering(pTrial, *(childTopos[childIndex]), _useConformingTraces);
    childTrialOrders[childIndex] = basicTrialOrdering; // we'll upgrade as needed below
  }
  
  int numSides = childrenForSides.size();
  
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    vector< pair< unsigned, unsigned> > childrenForSide = childrenForSides[sideIndex];
    
    vector< pair< unsigned, unsigned> >::iterator entryIt;
    int childIndexInParentSide = 0;
    for ( entryIt=childrenForSide.begin(); entryIt != childrenForSide.end(); entryIt++) {
      int childIndex = (*entryIt).first;
      int childSideIndex = (*entryIt).second;
      _dofOrderingFactory.childMatchParent(childTrialOrders[childIndex],childSideIndex,*(childTopos[childIndex]),
                                           childIndexInParentSide,
                                           parentTrialOrdering,sideIndex,parentTopo);
      childIndexInParentSide++;
    }
  }
  
  // determine test ordering for each child
  for (int childIndex=0; childIndex<numChildren; childIndex++) {
    int maxDegree = childTrialOrders[childIndex]->maxBasisDegree();
    int testOrder = max(pTrial,maxDegree) + _pToAddToTest;
    
    childTestOrders[childIndex] = _dofOrderingFactory.testOrdering(testOrder, *(childTopos[childIndex]));
  }

  vector<int> childCellIDs;
  // create ElementTypePtr for each child, and add child to mesh...
  for (int childIndex=0; childIndex<numChildren; childIndex++) {
    int maxDegree = childTrialOrders[childIndex]->maxBasisDegree();
    int testOrder = max(pTrial,maxDegree) + _pToAddToTest;
    
    childTestOrders[childIndex] = _dofOrderingFactory.testOrdering(testOrder, *(childTopos[childIndex]));

    ElementTypePtr childType = _elementTypeFactory.getElementType(childTrialOrders[childIndex], 
                                                                  childTestOrders[childIndex], 
                                                                  childTopos[childIndex] );
    ElementPtr child = addElement(children[childIndex], childType);
    childCellIDs.push_back( child->cellID() );
    parent->addChild(child);
  }
  
  // check parent's neighbors along each side: if they are unbroken, then we need to assign an appropriate MultiBasis
  // (this is a job for DofOrderingFactory)
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    vector< pair< unsigned, unsigned> > childrenForSide = childrenForSides[sideIndex];
    vector< pair< unsigned, unsigned> >::iterator entryIt;
    for ( entryIt=childrenForSide.begin(); entryIt != childrenForSide.end(); entryIt++) {
      int childIndex = (*entryIt).first;
      int childSideIndex = (*entryIt).second;
      int childCellID = parent->getChild(childIndex)->cellID();
      if (parent->getChild(childIndex)->getNeighborCellID(childSideIndex) == -1) {
        // child does not yet have a neighbor
        // inherit the parity of parent: (this can be reversed child gets a neighbor)
        _cellSideParitiesForCellID[childCellID][childSideIndex] = _cellSideParitiesForCellID[parent->cellID()][sideIndex];
//        cout << "addChildren: set cellSideParity for cell " << childCellID << ", sideIndex " << childSideIndex << ": ";
//        cout << _cellSideParitiesForCellID[parent->cellID()][sideIndex] << endl;
      }
    }
    
    int neighborSideIndex;
    ElementPtr neighborToMatch = ancestralNeighborForSide(parent,sideIndex,neighborSideIndex);
    
    if (neighborToMatch->cellID() != -1) { // then we have a neighbor to match along that side...
      matchNeighbor(neighborToMatch,neighborSideIndex);
    }
  }
  delete[] childTrialOrders;
  delete[] childTopos;
  delete[] childTestOrders;
}

ElementPtr Mesh::addElement(const vector<unsigned> & vertexIndices, ElementTypePtr elemType) {
  int numDimensions = elemType->cellTopoPtr->getDimension();
  int numVertices = vertexIndices.size();
  if ( numVertices != elemType->cellTopoPtr->getVertexCount(numDimensions,0) ) {
    TEUCHOS_TEST_FOR_EXCEPTION(true,
                       std::invalid_argument,
                       "incompatible number of vertices for cell topology");
  }
  if (numDimensions != 2) {
    TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,
                       "mesh only supports 2D right now...");
  }
  int cellID = _elements.size();
  _elements.push_back(Teuchos::rcp( new Element(cellID, elemType, -1) ) ); // cellIndex undefined for now...
  _cellSideParitiesForCellID.push_back(vector<int>(numVertices)); // placeholder parities
  for (int i=0; i<numVertices; i++ ) {
    // sideIndex is i...
    pair<int,int> edgeReversed = make_pair(vertexIndices[ (i+1) % numVertices ], vertexIndices[i]);
    pair<int,int> edge = make_pair( vertexIndices[i], vertexIndices[ (i+1) % numVertices ]);
    pair<int,int> myEntry = make_pair(cellID, i);
    if ( _edgeToCellIDs.find(edgeReversed) != _edgeToCellIDs.end() ) {
      // there's already an entry for this edge
      if ( _edgeToCellIDs[edgeReversed].size() > 1 ) {
        TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,
                           "In 2D mesh, shouldn't have more than 2 elements per edge...");
      }
      pair<int,int> entry = _edgeToCellIDs[edgeReversed][0];
      int neighborID = entry.first;
      int neighborSideIndex = entry.second;
      setNeighbor(_elements[cellID], i, _elements[neighborID], neighborSideIndex);
      // if there's just one, say howdy to our neighbor (and remove its edge from the boundary)
      // TODO: check to make sure this change is correct (was (cellID, i), but I don't see how that could be right--still, we passed tests....)
      _boundary.deleteElement(neighborID, neighborSideIndex);
    } else {
      setNeighbor(_elements[cellID], i, _nullPtr, i);
      _boundary.addElement(cellID, i);
    }
    if ( _edgeToCellIDs.find(edge) != _edgeToCellIDs.end() ) {
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,
                         "Either a duplicate edge (3 elements with a single edge), or element vertices not setup in CCW order.");
    }
//    cout << "adding entry for edge " << edge.first << " --> " << edge.second << ": cellID " << cellID << endl;
    _edgeToCellIDs[edge].push_back(myEntry);
  }
  return _elements[cellID];
}

ElementPtr Mesh::ancestralNeighborForSide(ElementPtr elem, int sideIndex, int &elemSideIndexInNeighbor) {
  // the commented-out code here is an effort to move the logic for this into element, where it belongs
  // (but there's a bug either here or in ancestralNeighborCellIDForSide(sideIndex), which causes tests to
  //   fail with exceptions indicating that multi-basis isn't applied where it should be...)
//  pair<int,int> neighborCellAndSide = elem->ancestralNeighborCellIDForSide(sideIndex);
//  elemSideIndexInNeighbor = neighborCellAndSide.second;
//  int neighborCellID = neighborCellAndSide.first;
//  if (neighborCellID == -1) {
//    return _nullPtr;
//  } else {
//    return _elements[neighborCellID];
//  }
  // returns neighbor for side, or the neighbor of the ancestor who has a neighbor along shared side
  while (elem->getNeighborCellID(sideIndex) == -1) {
    if ( ! elem->isChild() ) {
      //TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "No ancestor has an active neighbor along shared side");
      elemSideIndexInNeighbor = -1;
      return _nullPtr;
    }
    sideIndex = elem->parentSideForSideIndex(sideIndex);
    if (sideIndex == -1) return _nullPtr;
    elem = _elements[elem->getParent()->cellID()];
  }
  // once we get here, we have the appropriate ancestor:
  elemSideIndexInNeighbor = elem->getSideIndexInNeighbor(sideIndex);
  if (elemSideIndexInNeighbor >= 4) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "elemSideIndex >= 4");
  }
  return _elements[elem->getNeighborCellID(sideIndex)];
}

void Mesh::buildLocalToGlobalMap() {
  _localToGlobalMap.clear();
  vector<ElementPtr>::iterator elemIterator;
  
  // debug code:
//  for (elemIterator = _activeElements.begin(); elemIterator != _activeElements.end(); elemIterator++) {
//    ElementPtr elemPtr = *(elemIterator);
//    cout << "cellID " << elemPtr->cellID() << "'s trialOrdering:\n";
//    cout << *(elemPtr->elementType()->trialOrderPtr);
//  }
  
  determineDofPairings();
  
  int globalIndex = 0;
  vector< int > trialIDs = _bilinearForm->trialIDs();
  
  for (elemIterator = _activeElements.begin(); elemIterator != _activeElements.end(); elemIterator++) {
    ElementPtr elemPtr = *(elemIterator);
    int cellID = elemPtr->cellID();
    ElementTypePtr elemTypePtr = elemPtr->elementType();
    for (vector<int>::iterator trialIt=trialIDs.begin(); trialIt != trialIDs.end(); trialIt++) {
      int trialID = *(trialIt);
      
      if (! _bilinearForm->isFluxOrTrace(trialID) ) {
        // then all these dofs are interior, so there's no overlap with other elements...
        int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(trialID,0);
        for (int dofOrdinal=0; dofOrdinal<numDofs; dofOrdinal++) {
          int localDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,dofOrdinal,0);
          _localToGlobalMap[make_pair(cellID,localDofIndex)] = globalIndex;
//          cout << "added localToGlobalMap(cellID=" << cellID << ", localDofIndex=" << localDofIndex;
//          cout << ") = " << globalIndex << "\n";
          globalIndex++;
        }
      } else {
        int numSides = elemPtr->numSides();
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(trialID,sideIndex);
          for (int dofOrdinal=0; dofOrdinal<numDofs; dofOrdinal++) {
            int myLocalDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,dofOrdinal,sideIndex);
            pair<int, int> myKey = make_pair(cellID,myLocalDofIndex);
            pair<int, int> myValue;
            if ( _dofPairingIndex.find(myKey) != _dofPairingIndex.end() ) {
              int earlierCellID = _dofPairingIndex[myKey].first;
              int earlierLocalDofIndex = _dofPairingIndex[myKey].second;
              if (_localToGlobalMap.find(_dofPairingIndex[myKey]) == _localToGlobalMap.end() ) {
                // error: we haven't processed the earlier key yet...
                TEUCHOS_TEST_FOR_EXCEPTION( true,
                                   std::invalid_argument,
                                   "global indices are being processed out of order (should be by cellID, then localDofIndex).");
              } else {
                _localToGlobalMap[myKey] = _localToGlobalMap[_dofPairingIndex[myKey]];
//                cout << "added flux/trace localToGlobalMap(cellID,localDofIndex)=(" << cellID << ", " << myLocalDofIndex;
//                cout << ") = " << _localToGlobalMap[myKey] << "\n";
              }
            } else {
              // this test is necessary because for conforming traces, getDofIndex() may not be 1-1
              // and therefore we might have already assigned a globalDofIndex to myKey....
              if ( _localToGlobalMap.find(myKey) == _localToGlobalMap.end() ) {
                _localToGlobalMap[myKey] = globalIndex;
//                cout << "added flux/trace localToGlobalMap(cellID,localDofIndex)=(" << cellID << ", " << myLocalDofIndex;
//                cout << ") = " << globalIndex << "\n";
                globalIndex++;
              }
            }
          }
        }
      }
    }
  }
  _numGlobalDofs = globalIndex;
}

void Mesh::buildTypeLookups() {
  _elementTypes.clear();
  _elementTypesForPartition.clear();
  _cellIDsForElementType.clear();
  _globalCellIndexToCellID.clear();
  _partitionedCellSideParitiesForElementType.clear();
  _partitionedPhysicalCellNodesForElementType.clear();
  set< ElementType* > elementTypeSet; // keep track of which ones we've seen globally (avoid duplicates in _elementTypes)
  map< ElementType*, int > globalCellIndices;
  
  for (int partitionNumber=0; partitionNumber < _numPartitions; partitionNumber++) {
    _cellIDsForElementType.push_back( map< ElementType*, vector<int> >() );
    _elementTypesForPartition.push_back( vector< ElementTypePtr >() );
    _partitionedPhysicalCellNodesForElementType.push_back( map< ElementType*, FieldContainer<double> >() );
    _partitionedCellSideParitiesForElementType.push_back( map< ElementType*, FieldContainer<double> >() );
    vector< ElementPtr >::iterator elemIterator;

    // this should loop over the elements in the partition instead
    for (elemIterator=_partitions[partitionNumber].begin(); 
         elemIterator != _partitions[partitionNumber].end(); elemIterator++) {
      ElementPtr elem = *elemIterator;
      ElementTypePtr elemTypePtr = elem->elementType();
      if ( _cellIDsForElementType[partitionNumber].find( elemTypePtr.get() ) == _cellIDsForElementType[partitionNumber].end() ) {
        _elementTypesForPartition[partitionNumber].push_back(elemTypePtr);
      }
      if (elementTypeSet.find( elemTypePtr.get() ) == elementTypeSet.end() ) {
        elementTypeSet.insert( elemTypePtr.get() );
        _elementTypes.push_back( elemTypePtr );
      }
      _cellIDsForElementType[partitionNumber][elemTypePtr.get()].push_back(elem->cellID());
    }
    
    // now, build cellSideParities and physicalCellNodes lookups
    vector< ElementTypePtr >::iterator elemTypeIt;
    for (elemTypeIt=_elementTypesForPartition[partitionNumber].begin(); elemTypeIt != _elementTypesForPartition[partitionNumber].end(); elemTypeIt++) {
      //ElementTypePtr elemType = _elementTypeFactory.getElementType((*elemTypeIt)->trialOrderPtr,
  //                                                                 (*elemTypeIt)->testOrderPtr,
  //                                                                 (*elemTypeIt)->cellTopoPtr);
      ElementTypePtr elemType = *elemTypeIt; // don't enforce uniquing here (if we wanted to, we
                                             //   would also need to call elem.setElementType for 
                                             //   affected elements...)
      int spaceDim = elemType->cellTopoPtr->getDimension();
      int numSides = elemType->cellTopoPtr->getSideCount();
      int numNodes = elemType->cellTopoPtr->getNodeCount();
      vector<int> cellIDs = _cellIDsForElementType[partitionNumber][elemType.get()];
      int numCells = cellIDs.size();
      FieldContainer<double> physicalCellNodes( numCells, numNodes, spaceDim ) ;
      FieldContainer<double> cellSideParities( numCells, numSides );
      vector<int>::iterator cellIt;
      int cellIndex = 0;
      
      // store physicalCellNodes:
      verticesForCells(physicalCellNodes, cellIDs);
      for (cellIt = cellIDs.begin(); cellIt != cellIDs.end(); cellIt++) {
        int cellID = *cellIt;
        ElementPtr elem = _elements[cellID];
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          cellSideParities(cellIndex,sideIndex) = _cellSideParitiesForCellID[cellID][sideIndex];
        }
        elem->setCellIndex(cellIndex++);
        elem->setGlobalCellIndex(globalCellIndices[elemType.get()]++);
        _globalCellIndexToCellID[elemType.get()][elem->globalCellIndex()] = cellID;
        TEUCHOS_TEST_FOR_EXCEPTION( elem->cellID() != _globalCellIndexToCellID[elemType.get()][elem->globalCellIndex()],
                           std::invalid_argument, "globalCellIndex -> cellID inconsistency detected" );
      }
      _partitionedPhysicalCellNodesForElementType[partitionNumber][elemType.get()] = physicalCellNodes;
      _partitionedCellSideParitiesForElementType[partitionNumber][elemType.get()] = cellSideParities;
    }
  }
  // finally, build _physicalCellNodesForElementType and _cellSideParitiesForElementType:
  _physicalCellNodesForElementType.clear();
  for (vector< ElementTypePtr >::iterator elemTypeIt = _elementTypes.begin();
       elemTypeIt != _elementTypes.end(); elemTypeIt++) {
    ElementType* elemType = elemTypeIt->get();
    int numCells = globalCellIndices[elemType];
    int spaceDim = elemType->cellTopoPtr->getDimension();
    int numSides = elemType->cellTopoPtr->getSideCount();
    _physicalCellNodesForElementType[elemType] = FieldContainer<double>(numCells,numSides,spaceDim);
  }
  // copy from the local (per-partition) FieldContainers to the global ones
  for (int partitionNumber=0; partitionNumber < _numPartitions; partitionNumber++) {
    vector< ElementTypePtr >::iterator elemTypeIt;
    for (elemTypeIt  = _elementTypesForPartition[partitionNumber].begin(); 
         elemTypeIt != _elementTypesForPartition[partitionNumber].end(); elemTypeIt++) {
      ElementType* elemType = elemTypeIt->get();
      FieldContainer<double> partitionedPhysicalCellNodes = _partitionedPhysicalCellNodesForElementType[partitionNumber][elemType];
      FieldContainer<double> partitionedCellSideParities = _partitionedCellSideParitiesForElementType[partitionNumber][elemType];
      
      int numCells = partitionedPhysicalCellNodes.dimension(0);
      int numSides = partitionedPhysicalCellNodes.dimension(1);
      int spaceDim = partitionedPhysicalCellNodes.dimension(2);
      
      // this copying can be made more efficient by copying a whole FieldContainer at a time
      // (but it's probably not worth it, for now)
      for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
        int cellID = _cellIDsForElementType[partitionNumber][elemType][cellIndex];
        int globalCellIndex = _elements[cellID]->globalCellIndex();
        TEUCHOS_TEST_FOR_EXCEPTION( cellID != _globalCellIndexToCellID[elemType][globalCellIndex],
                           std::invalid_argument, "globalCellIndex -> cellID inconsistency detected" );
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          for (int dim=0; dim<spaceDim; dim++) {
            _physicalCellNodesForElementType[elemType](globalCellIndex,sideIndex,dim) 
              = partitionedPhysicalCellNodes(cellIndex,sideIndex,dim);
          }
        }
      }
    }
  }
}

Teuchos::RCP<BilinearForm> Mesh::bilinearForm() { 
  return _bilinearForm; 
}

void Mesh::setBilinearForm( Teuchos::RCP<BilinearForm> bf) {
  // must match the original in terms of variable IDs, etc...
  _bilinearForm = bf;
}

Boundary & Mesh::boundary() {
  return _boundary; 
}

int Mesh::cellID(Teuchos::RCP< ElementType > elemTypePtr, int cellIndex, int partitionNumber) {
  if (partitionNumber == -1){ 
    if (( _globalCellIndexToCellID.find( elemTypePtr.get() ) != _globalCellIndexToCellID.end() )
        && 
        ( _globalCellIndexToCellID[elemTypePtr.get()].find( cellIndex )
         !=
          _globalCellIndexToCellID[elemTypePtr.get()].end()
         )
        )
      return _globalCellIndexToCellID[elemTypePtr.get()][ cellIndex ];
    else
      return -1;
  } else {
    if ( ( _cellIDsForElementType[partitionNumber].find( elemTypePtr.get() ) != _cellIDsForElementType[partitionNumber].end() )
        &&
         (_cellIDsForElementType[partitionNumber][elemTypePtr.get()].size() > cellIndex ) ) {
           return _cellIDsForElementType[partitionNumber][elemTypePtr.get()][cellIndex];
    } else return -1;
  }
}

const vector< int > & Mesh::cellIDsOfType(ElementTypePtr elemType) {
  int rank = 0;
  int numProcs = 1;
#ifdef HAVE_MPI
  rank     = Teuchos::GlobalMPISession::getRank();
#else
#endif
  return cellIDsOfType(rank,elemType);
}

const vector< int > & Mesh::cellIDsOfType(int partitionNumber, ElementTypePtr elemTypePtr) {
  // returns the cell IDs for a given partition and element type
  static vector<int> emptyVector;
  if (_cellIDsForElementType[partitionNumber].find(elemTypePtr.get()) != _cellIDsForElementType[partitionNumber].end())
    return _cellIDsForElementType[partitionNumber][elemTypePtr.get()];
  else
    return emptyVector;
}

vector< int > Mesh::cellIDsOfTypeGlobal(ElementTypePtr elemTypePtr) {
  vector< int > cellIDs;
  for (int partitionNumber=0; partitionNumber<_numPartitions; partitionNumber++) {
    cellIDs.insert(cellIDs.end(),
                   cellIDsOfType(partitionNumber,elemTypePtr).begin(),
                   cellIDsOfType(partitionNumber,elemTypePtr).end());
  }
  return cellIDs;
}

int Mesh::cellPolyOrder(int cellID) {
  return _dofOrderingFactory.trialPolyOrder(_elements[cellID]->elementType()->trialOrderPtr);
}

void Mesh::determineDofPairings() {
  _dofPairingIndex.clear();
  vector<ElementPtr>::iterator elemIterator;
  
  vector< int > trialIDs = _bilinearForm->trialIDs();
  
  for (elemIterator = _activeElements.begin(); elemIterator != _activeElements.end(); elemIterator++) {
    ElementPtr elemPtr = *(elemIterator);
    ElementTypePtr elemTypePtr = elemPtr->elementType();
    int cellID = elemPtr->cellID();
    
    if ( elemPtr->isParent() ) {
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"elemPtr is in _activeElements, but is a parent...");
    }
    if ( ! elemPtr->isActive() ) {
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument,"elemPtr is in _activeElements, but is inactive...");
    }
    for (vector<int>::iterator trialIt=trialIDs.begin(); trialIt != trialIDs.end(); trialIt++) {
      int trialID = *(trialIt);
      
      if (_bilinearForm->isFluxOrTrace(trialID) ) {
        int numSides = elemPtr->numSides();
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          int myNumDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(trialID,sideIndex);
          Element* neighborPtr;
          int mySideIndexInNeighbor;
          elemPtr->getNeighbor(neighborPtr, mySideIndexInNeighbor, sideIndex);
          
          int neighborCellID = neighborPtr->cellID(); // may be -1 if it's the boundary
          if (neighborCellID != -1) {
            Teuchos::RCP<Element> neighbor = _elements[neighborCellID];
            // check that the bases agree in #dofs:
            
            bool hasMultiBasis = neighbor->isParent() && !_usePatchBasis;
            
            if ( ! neighbor->isParent() ) {
              int neighborNumDofs = neighbor->elementType()->trialOrderPtr->getBasisCardinality(trialID,mySideIndexInNeighbor);
              if ( !hasMultiBasis && (myNumDofs != neighborNumDofs) ) { // neither a multi-basis, and we differ: a problem
                TEUCHOS_TEST_FOR_EXCEPTION(myNumDofs != neighborNumDofs,
                                   std::invalid_argument,
                                   "Element and neighbor don't agree on basis along shared side.");              
              }
            }
            
            // Here, we need to deal with the possibility that neighbor is a parent, broken along the shared side
            //  -- if so, we have a MultiBasis, and we need to match with each of neighbor's descendants along that side...
            vector< pair<int,int> > descendantsForSide = neighbor->getDescendantsForSide(mySideIndexInNeighbor);
            vector< pair<int,int> >:: iterator entryIt;
            int descendantIndex = -1;
            for (entryIt = descendantsForSide.begin(); entryIt != descendantsForSide.end(); entryIt++) {
              descendantIndex++;
              int descendantSubSideIndexInMe = neighborChildPermutation(descendantIndex, descendantsForSide.size());
              neighborCellID = (*entryIt).first;
              mySideIndexInNeighbor = (*entryIt).second;
              neighbor = _elements[neighborCellID];
              int neighborNumDofs = neighbor->elementType()->trialOrderPtr->getBasisCardinality(trialID,mySideIndexInNeighbor);
              
              for (int dofOrdinal=0; dofOrdinal<neighborNumDofs; dofOrdinal++) {
                int myLocalDofIndex;
                if ( (descendantsForSide.size() > 1) && ( !_usePatchBasis ) ) {
                  // multi-basis
                  myLocalDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,dofOrdinal,sideIndex,descendantSubSideIndexInMe);
                } else {
                  myLocalDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,dofOrdinal,sideIndex);
                }
                
                // neighbor's dofs are in reverse order from mine along each side
                // TODO: generalize this to some sort of permutation for 3D meshes...
                int permutedDofOrdinal = neighborDofPermutation(dofOrdinal,neighborNumDofs);
                
                int neighborLocalDofIndex = neighbor->elementType()->trialOrderPtr->getDofIndex(trialID,permutedDofOrdinal,mySideIndexInNeighbor);
                addDofPairing(cellID, myLocalDofIndex, neighborCellID, neighborLocalDofIndex);
              }
            }
            
            if ( hasMultiBasis && _enforceMBFluxContinuity ) {
              // marry the last node from one MB leaf to first node from the next
              // note that we're doing this for both traces and fluxes, but with traces this is redundant
              BasisPtr basis = elemTypePtr->trialOrderPtr->getBasis(trialID,sideIndex);
              MultiBasis<>* multiBasis = (MultiBasis<> *) basis.get(); // Dynamic cast would be better
              vector< pair<int,int> > adjacentDofOrdinals = multiBasis->adjacentVertexOrdinals();
              for (vector< pair<int,int> >::iterator dofPairIt = adjacentDofOrdinals.begin();
                   dofPairIt != adjacentDofOrdinals.end(); dofPairIt++) {
                int firstOrdinal  = dofPairIt->first;
                int secondOrdinal = dofPairIt->second;
                int firstDofIndex  = elemTypePtr->trialOrderPtr->getDofIndex(trialID,firstOrdinal, sideIndex);
                int secondDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,secondOrdinal,sideIndex);
                addDofPairing(cellID,firstDofIndex, cellID, secondDofIndex);
              }
            }
          }
        }
      }
    }
  }
  // now that we have all the dofPairings, reduce the map so that all the pairings point at the earliest paired index
  for (elemIterator = _activeElements.begin(); elemIterator != _activeElements.end(); elemIterator++) {
    ElementPtr elemPtr = *(elemIterator);
    int cellID = elemPtr->cellID();
    ElementTypePtr elemTypePtr = elemPtr->elementType();
    for (vector<int>::iterator trialIt=trialIDs.begin(); trialIt != trialIDs.end(); trialIt++) {
      int trialID = *(trialIt);
      if (_bilinearForm->isFluxOrTrace(trialID) ) {
        int numSides = elemPtr->numSides();
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(trialID,sideIndex);
          for (int dofOrdinal=0; dofOrdinal<numDofs; dofOrdinal++) {
            int myLocalDofIndex = elemTypePtr->trialOrderPtr->getDofIndex(trialID,dofOrdinal,sideIndex);
            pair<int, int> myKey = make_pair(cellID,myLocalDofIndex);
            if (_dofPairingIndex.find(myKey) != _dofPairingIndex.end()) {
              pair<int, int> myValue = _dofPairingIndex[myKey];
              while (_dofPairingIndex.find(myValue) != _dofPairingIndex.end()) {
                myValue = _dofPairingIndex[myValue];
              }
              _dofPairingIndex[myKey] = myValue;
            }
          }
        }
      }
    }
  }  
}

vector<ElementPtr> Mesh::elementsForPoints(const FieldContainer<double> &physicalPoints) {
//  if (_edgeToCurveMap.size() > 0) {
//    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "elementsForPoints does not support curvilinear meshes");
//  }
  
  // returns a vector of an active element per point, or null if there is no element including that point
  vector<ElementPtr> elemsForPoints;
//  cout << "entered elementsForPoints: \n" << physicalPoints;
  int numPoints = physicalPoints.dimension(0);
  // TODO: work out what to do here for 3D
  // figure out the last element of the original mesh:
  int lastCellID = 0;
  while ((_elements.size() > lastCellID) && ! _elements[lastCellID]->isChild()) {
    lastCellID++;
  }
  // NOTE: the above does depend on the domain of the mesh remaining fixed after refinements begin.
  
  for (int pointIndex=0; pointIndex<numPoints; pointIndex++) {
    double x = physicalPoints(pointIndex,0);
    double y = physicalPoints(pointIndex,1);
    // find the element from the original mesh that contains this point
    ElementPtr elem;
    for (int cellID = 0; cellID<lastCellID; cellID++) {
      if (elementContainsPoint(_elements[cellID],x,y)) {
        elem = _elements[cellID];
        break;
      }
    }
    if (elem.get() != NULL) {
      while ( elem->isParent() ) {
        int numChildren = elem->numChildren();
        bool foundMatchingChild = false;
        for (int childIndex = 0; childIndex < numChildren; childIndex++) {
          ElementPtr child = elem->getChild(childIndex);
          if ( elementContainsPoint(child,x,y) ) {
            elem = child;
            foundMatchingChild = true;
            break;
          }
        }
        if (!foundMatchingChild) {
          cout << "parent matches, but none of its children do... will return nearest cell centroid\n";
          int numVertices = elem->numSides();
          int spaceDim = 2;
          FieldContainer<double> vertices(numVertices,spaceDim);
          verticesForCell(vertices, elem->cellID());
          cout << "parent vertices:\n" << vertices;
          double minDistance = numeric_limits<double>::max();
          int childSelected = -1;
          for (int childIndex = 0; childIndex < numChildren; childIndex++) {
            ElementPtr child = elem->getChild(childIndex);
            verticesForCell(vertices, child->cellID());
            cout << "child " << childIndex << ", vertices:\n" << vertices;
            vector<double> cellCentroid = getCellCentroid(child->cellID());
            double d = sqrt((cellCentroid[0] - x) * (cellCentroid[0] - x) + (cellCentroid[1] - y) * (cellCentroid[1] - y));
            if (d < minDistance) {
              minDistance = d;
              childSelected = childIndex;
            }
          }
          elem = elem->getChild(childSelected);
        }
      }
    }
    elemsForPoints.push_back(elem);
  }
//  cout << "Returning from elementsForPoints\n";
  return elemsForPoints;
}

bool Mesh::elementContainsPoint(ElementPtr elem, double x, double y) {
  // 12-18-13 revision: to use Intrepid's facilities and to allow curvilinear geometry
  // note that this design, with a single point being passed in, will be quite inefficient
  // if there are many points.  TODO: revise to allow multiple points (returning vector<bool>, maybe)
  int numCells = 1, numPoints=1, spaceDim = 2;
  FieldContainer<double> physicalPoints(numCells,numPoints,spaceDim);
  physicalPoints(0,0,0) = x;
  physicalPoints(0,0,1) = y;
//  cout << "cell " << elem->cellID() << ": (" << x << "," << y << ") --> ";
  FieldContainer<double> refPoints(numCells,numPoints,spaceDim);
  MeshPtr thisPtr = Teuchos::rcp(this,false);
  CamelliaCellTools::mapToReferenceFrame(refPoints, physicalPoints, thisPtr, elem->cellID());
//  cout << "(" << refPoints[0] << "," << refPoints[1] << ")\n";
  
  int result = CellTools<double>::checkPointInclusion(&refPoints[0], spaceDim, *(elem->elementType()->cellTopoPtr));
  return result == 1;
}

void Mesh::enforceOneIrregularity() {
  int rank = 0;
  int numProcs = 1;
#ifdef HAVE_MPI
  rank     = Teuchos::GlobalMPISession::getRank();
  numProcs = Teuchos::GlobalMPISession::getNProc();
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif
  
  bool meshIsNotRegular = true; // assume it's not regular and check elements
  while (meshIsNotRegular) {
    vector <int> irregularTriangleCells;
    vector <int> irregularQuadCells;
    vector< Teuchos::RCP< Element > > newActiveElements = activeElements();
    vector< Teuchos::RCP< Element > >::iterator newElemIt;
    
    for (newElemIt = newActiveElements.begin(); newElemIt != newActiveElements.end(); newElemIt++) {
      Teuchos::RCP< Element > current_element = *(newElemIt);
      bool isIrregular = false;
      for (int sideIndex=0; sideIndex < current_element->numSides(); sideIndex++) {
        int mySideIndexInNeighbor;
        Element* neighbor; // may be a parent
        current_element->getNeighbor(neighbor, mySideIndexInNeighbor, sideIndex);
        int numNeighborsOnSide = neighbor->getDescendantsForSide(mySideIndexInNeighbor).size();
        if (numNeighborsOnSide > 2) isIrregular=true;
      }
      
      if (isIrregular){
        if ( 3 == current_element->numSides() ) {
          irregularTriangleCells.push_back(current_element->cellID());
        }
        else if (4 == current_element->numSides() ) {
          irregularQuadCells.push_back(current_element->cellID());
        }
	/*
	  if (rank==0){
          cout << "cell " << current_element->cellID() << " refined to maintain regularity" << endl;
        }
	*/
      }
    }
    if ((irregularQuadCells.size()>0) || (irregularTriangleCells.size()>0)) {
      hRefine(irregularTriangleCells,RefinementPattern::regularRefinementPatternTriangle());
      hRefine(irregularQuadCells,RefinementPattern::regularRefinementPatternQuad());
      irregularTriangleCells.clear();
      irregularQuadCells.clear();
    } else {
      meshIsNotRegular=false;
    }
  }
}

// commented this out because it appears to be unused
//Epetra_Map Mesh::getCellIDPartitionMap(int rank, Epetra_Comm* Comm){
//  int indexBase = 0; // 0 for cpp, 1 for fortran
//  int numActiveElements = activeElements().size();
//  
//  // determine cell IDs for this partition
//  vector< ElementPtr > elemsInPartition = elementsInPartition(rank);
//  int numElemsInPartition = elemsInPartition.size();
//  int *partitionLocalElems;
//  if (numElemsInPartition!=0){
//    partitionLocalElems = new int[numElemsInPartition];
//  }else{
//    partitionLocalElems = NULL;
//  }  
//  
//  // set partition-local cell IDs
//  for (int activeCellIndex=0; activeCellIndex<numElemsInPartition; activeCellIndex++) {
//    ElementPtr elemPtr = elemsInPartition[activeCellIndex];
//    int cellIndex = elemPtr->globalCellIndex();
//    partitionLocalElems[activeCellIndex] = cellIndex;
//  }
//  Epetra_Map partMap(numActiveElements, numElemsInPartition, partitionLocalElems, indexBase, *Comm);
//}

FieldContainer<double> & Mesh::cellSideParities( ElementTypePtr elemTypePtr ) {
#ifdef HAVE_MPI
  int partitionNumber     = Teuchos::GlobalMPISession::getRank();
#else
  int partitionNumber     = 0;
#endif
  return _partitionedCellSideParitiesForElementType[ partitionNumber ][ elemTypePtr.get() ];
}

FieldContainer<double> Mesh::cellSideParitiesForCell( int cellID ) {
  vector<int> parities = _cellSideParitiesForCellID[cellID];
  int numSides = parities.size();
  FieldContainer<double> cellSideParities(1,numSides);
  for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
    cellSideParities(0,sideIndex) = parities[sideIndex];
  }
  return cellSideParities;
}

vector<double> Mesh::getCellCentroid(int cellID){
  int numVertices = _elements[cellID]->numSides();
  int spaceDim = 2;
  FieldContainer<double> vertices(numVertices,spaceDim);
  verticesForCell(vertices, cellID);    
  //average vertex positions together to get a centroid (avoids using vertex in case local enumeration overlaps)
  vector<double> coords(spaceDim,0.0);
  for (int k=0;k<spaceDim;k++){
    for (int j=0;j<numVertices;j++){
      coords[k] += vertices(j,k);
    }
    coords[k] = coords[k]/((double)(numVertices));
  }
  return coords;
}

void Mesh::determineActiveElements() {
#ifdef HAVE_MPI
  int partitionNumber     = Teuchos::GlobalMPISession::getRank();
#else
  int partitionNumber     = 0;
#endif
  
  _activeElements.clear();
  vector<ElementPtr>::iterator elemIterator;
  
  for (elemIterator = _elements.begin(); elemIterator != _elements.end(); elemIterator++) {
    ElementPtr elemPtr = *(elemIterator);
    if ( elemPtr->isActive() ) {
      _activeElements.push_back(elemPtr);
    }
  }
  _partitions.clear();
  _partitionForCellID.clear();
  FieldContainer<int> partitionedMesh(_numPartitions,_activeElements.size());
  _partitionPolicy->partitionMesh(_meshTopology.get(),_numPartitions,partitionedMesh);
  _activeCellOffset = 0;
  for (int i=0; i<_numPartitions; i++) {
    vector<ElementPtr> partition;
    for (int j=0; j<_activeElements.size(); j++) {
      if (partitionedMesh(i,j) < 0) break; // no more elements in this partition
      int cellID = partitionedMesh(i,j);
      partition.push_back( _elements[cellID] );
      _partitionForCellID[cellID] = i;
    }
    _partitions.push_back( partition );
    if (partitionNumber > i) {
      _activeCellOffset += partition.size();
    }
  }
}

void Mesh::determinePartitionDofIndices() {
  _partitionedGlobalDofIndices.clear();
  _partitionForGlobalDofIndex.clear();
  _partitionLocalIndexForGlobalDofIndex.clear();
  set<int> dofIndices;
  set<int> previouslyClaimedDofIndices;
  for (int i=0; i<_numPartitions; i++) {
    dofIndices.clear();
    vector< ElementPtr >::iterator elemIterator;
    for (elemIterator =  _partitions[i].begin(); elemIterator != _partitions[i].end(); elemIterator++) {
      ElementPtr elem = *elemIterator;
      ElementTypePtr elemTypePtr = elem->elementType();
      int numLocalDofs = elemTypePtr->trialOrderPtr->totalDofs();
      int cellID = elem->cellID();
      for (int localDofIndex=0; localDofIndex < numLocalDofs; localDofIndex++) {
        pair<int,int> key = make_pair(cellID, localDofIndex);
        map< pair<int,int>, int >::iterator mapEntryIt = _localToGlobalMap.find(key);
        if ( mapEntryIt == _localToGlobalMap.end() ) {
          TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "entry not found.");
        }
        int dofIndex = (*mapEntryIt).second;
        if ( previouslyClaimedDofIndices.find( dofIndex ) == previouslyClaimedDofIndices.end() ) {
          dofIndices.insert( dofIndex );
          _partitionForGlobalDofIndex[ dofIndex ] = i;
          previouslyClaimedDofIndices.insert(dofIndex);
        }
      }
    }
    _partitionedGlobalDofIndices.push_back( dofIndices );
    int partitionDofIndex = 0;
    for (set<int>::iterator dofIndexIt = dofIndices.begin();
         dofIndexIt != dofIndices.end(); dofIndexIt++) {
      int globalDofIndex = *dofIndexIt;
      _partitionLocalIndexForGlobalDofIndex[globalDofIndex] = partitionDofIndex++;
    }
  }
}

vector< Teuchos::RCP< Element > > & Mesh::elements() { 
  return _elements; 
}

vector< ElementPtr > Mesh::elementsInPartition(int partitionNumber){
  int rank     = Teuchos::GlobalMPISession::getRank();
  if (partitionNumber == -1) {
    partitionNumber = rank;
  }
  return _partitions[partitionNumber];
}

vector< ElementPtr > Mesh::elementsOfType(int partitionNumber, ElementTypePtr elemTypePtr) {
  // returns the elements for a given partition and element type
  vector< ElementPtr > elementsOfType;
  vector<int> cellIDs = _cellIDsForElementType[partitionNumber][elemTypePtr.get()];
  int numCells = cellIDs.size();
  for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
    elementsOfType.push_back(_elements[cellIDs[cellIndex]]);
  }
  return elementsOfType;
}

vector< ElementPtr > Mesh::elementsOfTypeGlobal(ElementTypePtr elemTypePtr) {
  vector< ElementPtr > elementsOfTypeVector;
  for (int partitionNumber=0; partitionNumber<_numPartitions; partitionNumber++) {
    vector< ElementPtr > elementsOfTypeForPartition = elementsOfType(partitionNumber,elemTypePtr);
    elementsOfTypeVector.insert(elementsOfTypeVector.end(),elementsOfTypeForPartition.begin(),elementsOfTypeForPartition.end());
  }
  return elementsOfTypeVector;
}

vector< Teuchos::RCP< ElementType > > Mesh::elementTypes(int partitionNumber) {
  if ((partitionNumber >= 0) && (partitionNumber < _numPartitions)) {
    return _elementTypesForPartition[partitionNumber];
  } else if (partitionNumber < 0) {
    return _elementTypes;
  } else {
    vector< Teuchos::RCP< ElementType > > noElementTypes;
    return noElementTypes;
  }
}

set<int> Mesh::getActiveCellIDs() {
  set<int> cellIDs;
  for (int i=0; i<_activeElements.size(); i++) {
    cellIDs.insert(_activeElements[i]->cellID());
  }
  return cellIDs;
}

ElementPtr Mesh::getActiveElement(int index) {
  return _activeElements[index];
}

int Mesh::getDimension() {
  return 2; // all that's supported for now
}

DofOrderingFactory & Mesh::getDofOrderingFactory() {
  return _dofOrderingFactory;
}

ElementPtr Mesh::getElement(int cellID) {
  return _elements[cellID];
}

ElementTypeFactory & Mesh::getElementTypeFactory() {
  return _elementTypeFactory;
}

long Mesh::getVertexIndex(double x, double y, double tol) {
  vector<double> vertex;
  vertex.push_back(x);
  vertex.push_back(y);
  
  unsigned vertexIndex;
  if (! _meshTopology->getVertexIndex(vertex, vertexIndex) ) {
    return -1;
  } else {
    return vertexIndex;
  }
}

map<unsigned, unsigned> Mesh::getGlobalVertexIDs(const FieldContainer<double> &vertices) {
  double tol = 1e-12; // tolerance for vertex equality
  
  map<unsigned, unsigned> localToGlobalVertexIndex;
  int numVertices = vertices.dimension(0);
  for (int i=0; i<numVertices; i++) {
    localToGlobalVertexIndex[i] = getVertexIndex(vertices(i,0), vertices(i,1),tol);
  }
  return localToGlobalVertexIndex;
}

void Mesh::getMultiBasisOrdering(DofOrderingPtr &originalNonParentOrdering,
                                 ElementPtr parent, int sideIndex, int parentSideIndexInNeighbor,
                                 ElementPtr nonParent) {
  map< int, BasisPtr > varIDsToUpgrade = multiBasisUpgradeMap(parent,sideIndex);
  originalNonParentOrdering = _dofOrderingFactory.upgradeSide(originalNonParentOrdering,
                                                              *(nonParent->elementType()->cellTopoPtr),
                                                              varIDsToUpgrade,parentSideIndexInNeighbor);
}

// this is likely cruft: there's a version of this in Solution...
Epetra_Map Mesh::getPartitionMap() {
  int rank = 0;
#ifdef HAVE_MPI
  rank     = Teuchos::GlobalMPISession::getRank();
  Epetra_MpiComm Comm(MPI_COMM_WORLD);
#else
  Epetra_SerialComm Comm;
#endif
  
  // returns map for current processor's local-to-global dof indices
  // determine the local dofs we have, and what their global indices are:
  int localDofsSize;

  set<int> myGlobalIndicesSet = globalDofIndicesForPartition(rank);
  
  localDofsSize = myGlobalIndicesSet.size();
  
  int *myGlobalIndices;
  if (localDofsSize!=0){
    myGlobalIndices = new int[ localDofsSize ];      
  }else{
    myGlobalIndices = NULL;
  }
  
  // copy from set object into the allocated array
  int offset = 0;
  for ( set<int>::iterator indexIt = myGlobalIndicesSet.begin();
       indexIt != myGlobalIndicesSet.end();
       indexIt++ ){
    myGlobalIndices[offset++] = *indexIt;
  }
  
  int numGlobalDofs = this->numGlobalDofs();
  int indexBase = 0;
  //cout << "process " << rank << " about to construct partMap.\n";
  //Epetra_Map partMap(-1, localDofsSize, myGlobalIndices, indexBase, Comm);
  Epetra_Map partMap(numGlobalDofs, localDofsSize, myGlobalIndices, indexBase, Comm);
  
  if (localDofsSize!=0){
    delete[] myGlobalIndices;
  }
  return partMap;
}

void Mesh::getPatchBasisOrdering(DofOrderingPtr &originalChildOrdering, ElementPtr child, int sideIndex) {
  DofOrderingPtr parentTrialOrdering = child->getParent()->elementType()->trialOrderPtr;
//  cout << "Adding PatchBasis for element " << child->cellID() << " along side " << sideIndex << "\n";
//  
//  cout << "parent is cellID " << child->getParent()->cellID() << "; parent trialOrdering:\n";
//  cout << *parentTrialOrdering;
//  
//  cout << "original childTrialOrdering:\n" << *originalChildOrdering;
  
  //cout << "Adding PatchBasis for element " << child->cellID() << ":\n" << physicalCellNodesForCell(child->cellID());
  int parentSideIndex = child->parentSideForSideIndex(sideIndex);
  int childIndexInParentSide = child->indexInParentSide(parentSideIndex);
  map< int, BasisPtr > varIDsToUpgrade = _dofOrderingFactory.getPatchBasisUpgradeMap(originalChildOrdering, sideIndex,
                                                                                     parentTrialOrdering, parentSideIndex,
                                                                                     childIndexInParentSide);
  originalChildOrdering = _dofOrderingFactory.upgradeSide(originalChildOrdering,
                                                          *(child->elementType()->cellTopoPtr),
                                                          varIDsToUpgrade,parentSideIndex);
  
//  cout << "childTrialOrdering after upgrading side:\n" << *originalChildOrdering;
}

FunctionPtr Mesh::getTransformationFunction() {
  // will be NULL for meshes without edge curves defined
  
  // for now, we recompute the transformation function each time the edge curves get updated
  // we might later want to do something lazier, updating/creating it here if it's out of date
  
  return _meshTopology->transformationFunction();
}

int Mesh::globalDofIndex(int cellID, int localDofIndex) {
  pair<int,int> key = make_pair(cellID, localDofIndex);
  map< pair<int,int>, int >::iterator mapEntryIt = _localToGlobalMap.find(key);
  if ( mapEntryIt == _localToGlobalMap.end() ) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "entry not found.");
  }
  return (*mapEntryIt).second;
}

set<int> Mesh::globalDofIndicesForPartition(int partitionNumber) {
  return _partitionedGlobalDofIndices[partitionNumber];
}

//void Mesh::hRefine(vector<int> cellIDs, Teuchos::RCP<RefinementPattern> refPattern) {
//  hRefine(cellIDs,refPattern,vector< Teuchos::RCP<Solution> >()); 
//}

void Mesh::hRefine(const vector<int> &cellIDs, Teuchos::RCP<RefinementPattern> refPattern) {
  set<int> cellSet(cellIDs.begin(),cellIDs.end());
  hRefine(cellSet,refPattern);
}

void Mesh::hRefine(const set<int> &cellIDs, Teuchos::RCP<RefinementPattern> refPattern) {
  // refine any registered meshes
  for (vector< Teuchos::RCP<RefinementObserver> >::iterator meshIt = _registeredObservers.begin();
       meshIt != _registeredObservers.end(); meshIt++) {
    (*meshIt)->hRefine(cellIDs,refPattern);
  }
  
  set<int>::const_iterator cellIt;
  
  for (cellIt = cellIDs.begin(); cellIt != cellIDs.end(); cellIt++) {
    int cellID = *cellIt;
    
    _meshTopology->refineCell(cellID, refPattern);
    
    // NOTE: much of the logic below really belongs to an earlier version of Mesh.  At this point, MeshTopology has
    // taken over much of this.  I'm leaving the excision of this logic for later, since I'd need to revise Mesh::addChildren as well...
    
//    cout << "refining cellID " << cellID << endl;
    ElementPtr elem = _elements[cellID];
    ElementTypePtr elemType;
    
    if (_cellSideUpgrades.find(cellID) != _cellSideUpgrades.end()) {
      // this cell has had its sides upgraded, so the elemType Solution knows about is stored in _cellSideUpgrades
      elemType = _cellSideUpgrades[cellID].first;
    } else {
      elemType = elem->elementType();
    }
    
    int spaceDim = elemType->cellTopoPtr->getDimension();
    int vertexCount = elemType->cellTopoPtr->getVertexCount();
    
    FieldContainer<double> cellVertices(vertexCount,spaceDim);
    vector<unsigned> vertexIndices = _meshTopology->getCell(cellID)->vertices();
    for (int vertex=0; vertex<vertexCount; vertex++) {
      unsigned vertexIndex = vertexIndices[vertex];
      for (int i=0; i<spaceDim; i++) {
        cellVertices(vertex,i) = _meshTopology->getVertex(vertexIndex)[i];
      }
    }
    
    FieldContainer<double> vertices = refPattern->verticesForRefinement(cellVertices);
    if (_meshTopology->transformationFunction().get()) {
      bool changedVertices = _meshTopology->transformationFunction()->mapRefCellPointsUsingExactGeometry(vertices, refPattern->verticesOnReferenceCell(), cellID);
    }
    
    map<unsigned, unsigned> localToGlobalVertexIndex = getGlobalVertexIDs(vertices); // key: index in vertices; value: index in _meshTopology
    
    // get the children, as vectors of vertex indices:
    vector< vector<unsigned> > children = refPattern->children(localToGlobalVertexIndex);
    
    // get the (child, child side) pairs for each side of the parent
    vector< vector< pair< unsigned, unsigned> > > childrenForSides = refPattern->childrenForSides(); // outer vector: indexed by parent's sides; inner vector: (child index in children, index of child's side shared with parent)
    
    _elements[cellID]->setRefinementPattern(refPattern);
    addChildren(_elements[cellID],children,childrenForSides);
    
    // let transformation function know about the refinement that just took place
    if (_meshTopology->transformationFunction().get()) {
      set<int> cellIDset;
      cellIDset.insert(cellID);
      _meshTopology->transformationFunction()->didHRefine(cellIDset);
    }
    
    for (vector< Solution* >::iterator solutionIt = _registeredSolutions.begin();
         solutionIt != _registeredSolutions.end(); solutionIt++) {
       // do projection
      int numChildren = _elements[cellID]->numChildren();
      vector<int> childIDs;
      for (int i=0; i<numChildren; i++) {
        childIDs.push_back(_elements[cellID]->getChild(i)->cellID());
      }
      (*solutionIt)->processSideUpgrades(_cellSideUpgrades,cellIDs); // cellIDs argument: skip these...
      (*solutionIt)->projectOldCellOntoNewCells(cellID,elemType,childIDs);
    }
    // with the exception of the cellIDs upgrades, _cellSideUpgrades have been processed,
    // so we delete everything except those
    // (there's probably a more sophisticated way to delete these from the map, b
    map< int, pair< ElementTypePtr, ElementTypePtr > > remainingCellSideUpgrades;
    for (set<int>::iterator cellIt=cellIDs.begin(); cellIt != cellIDs.end(); cellIt++) {
      int cellID = *cellIt;
      if (_cellSideUpgrades.find(cellID) != _cellSideUpgrades.end()) {
        remainingCellSideUpgrades[cellID] = _cellSideUpgrades[cellID];
      }
    }
    _cellSideUpgrades = remainingCellSideUpgrades;
  }
  rebuildLookups();
  // now discard any old coefficients
  for (vector< Solution* >::iterator solutionIt = _registeredSolutions.begin();
       solutionIt != _registeredSolutions.end(); solutionIt++) {
    (*solutionIt)->discardInactiveCellCoefficients();
  }
}

void Mesh::hUnrefine(const set<int> &cellIDs) {
  // refine any registered meshes
  for (vector< Teuchos::RCP<RefinementObserver> >::iterator meshIt = _registeredObservers.begin();
       meshIt != _registeredObservers.end(); meshIt++) {
    (*meshIt)->hUnrefine(cellIDs);
  }
  
  set<int>::const_iterator cellIt;
  set< pair<int, int> > affectedNeighborSides; // (cellID, sideIndex)
  set< int > deletedCellIDs;
  
  for (cellIt = cellIDs.begin(); cellIt != cellIDs.end(); cellIt++) {
    int cellID = *cellIt;
    ElementPtr elem = _elements[cellID];
    elem->deleteChildrenFromMesh(affectedNeighborSides, deletedCellIDs);
  }
  
  set<int> affectedNeighbors;
  // for each nullified neighbor relationship, need to figure out the correct element type...
  for ( set< pair<int, int> >::iterator neighborIt = affectedNeighborSides.begin();
       neighborIt != affectedNeighborSides.end(); neighborIt++) {
    ElementPtr elem = _elements[ neighborIt->first ];
    if (elem->isActive()) {
      matchNeighbor( elem, neighborIt->second );
    }
  }
  
  // delete any boundary entries for deleted elements
  for (set<int>::iterator cellIt = deletedCellIDs.begin(); cellIt != deletedCellIDs.end(); cellIt++) {
    int cellID = *cellIt;
    ElementPtr elem = _elements[cellID];
    for (int sideIndex=0; sideIndex<elem->numSides(); sideIndex++) {
      // boundary allows us to delete even combinations that weren't there to begin with...
      _boundary.deleteElement(cellID, sideIndex);
    }
  }
  
  // add in any new boundary elements:
  for (cellIt = cellIDs.begin(); cellIt != cellIDs.end(); cellIt++) {
    int cellID = *cellIt;
    ElementPtr elem = _elements[cellID];
    if (elem->isActive()) {
      int elemSideIndexInNeighbor;
      for (int sideIndex=0; sideIndex<elem->numSides(); sideIndex++) {
        if (ancestralNeighborForSide(elem, sideIndex, elemSideIndexInNeighbor)->cellID() == -1) {
          // boundary
          _boundary.addElement(cellID, sideIndex);
        }
      }
    }
  }

  // added by Jesse to try to fix bug
  for (set<int>::iterator cellIt = deletedCellIDs.begin(); cellIt != deletedCellIDs.end(); cellIt++) {
    // erase from _elements list
    for (int i = 0; i<_elements.size();i++){
      if (_elements[i]->cellID()==(*cellIt)){
        _elements.erase(_elements.begin()+i);
        break;
      }
    }
    // erase any pairs from _edgeToCellIDs having to do with deleted cellIDs
    for (map<pair<int,int>, vector<pair<int,int> > >::iterator mapIt = _edgeToCellIDs.begin(); mapIt!=_edgeToCellIDs.end();mapIt++){
      vector<pair<int,int> > cellIDSideIndices = mapIt->second;
      bool eraseEntry = false;
      for (int i = 0;i<cellIDSideIndices.size();i++){
        int cellID = cellIDSideIndices[i].first;
        if (cellID==(*cellIt)){
          eraseEntry = true;
        }
        if (eraseEntry)
          break;
      }
      if (eraseEntry){
        _edgeToCellIDs.erase(mapIt);
//        cout << "deleting edge to cell entry " << mapIt->first.first << " --> " << mapIt->first.second << endl;
      }
    }
  }

  rebuildLookups();
  
  // now discard any old coefficients
  for (vector< Solution* >::iterator solutionIt = _registeredSolutions.begin();
       solutionIt != _registeredSolutions.end(); solutionIt++) {
    (*solutionIt)->discardInactiveCellCoefficients();
  }

  // TODO: modify _edgeToCellID (exception thrown) and delete terms in _elements

}

int Mesh::neighborChildPermutation(int childIndex, int numChildrenInSide) {
  // we'll need to be more sophisticated in 3D, but for now we just reverse the order
  return numChildrenInSide - childIndex - 1;
}

int Mesh::neighborDofPermutation(int dofIndex, int numDofsForSide) {
  // we'll need to be more sophisticated in 3D, but for now we just reverse the order
  return numDofsForSide - dofIndex - 1;
}

void Mesh::matchNeighbor(const ElementPtr &elem, int sideIndex) {
  // sets new ElementType to match elem to neighbor on side sideIndex
  
  const shards::CellTopology cellTopo = *(elem->elementType()->cellTopoPtr.get());
  Element* neighbor;
  int mySideIndexInNeighbor;
  elem->getNeighbor(neighbor, mySideIndexInNeighbor, sideIndex);
  
  int neighborCellID = neighbor->cellID(); // may be -1 if it's the boundary
  if (neighborCellID < 0) {
    return; // no change
  }
  ElementPtr neighborRCP = _elements[neighborCellID];
  // h-refinement handling:
  bool neighborIsBroken = (neighbor->isParent() && (neighbor->childIndicesForSide(mySideIndexInNeighbor).size() > 1));
  bool elementIsBroken  = (elem->isParent() && (elem->childIndicesForSide(sideIndex).size() > 1));
  if ( neighborIsBroken || elementIsBroken ) {
    bool bothBroken = ( neighborIsBroken && elementIsBroken );
    ElementPtr nonParent, parent; // for the case that one is a parent and the other isn't
    int parentSideIndexInNeighbor, neighborSideIndexInParent;
    if ( !bothBroken ) {
      if (! elementIsBroken ) {
        nonParent = elem;
        parent = neighborRCP;
        parentSideIndexInNeighbor = sideIndex;
        neighborSideIndexInParent = mySideIndexInNeighbor;
      } else {
        nonParent = neighborRCP;
        parent = elem;
        parentSideIndexInNeighbor = mySideIndexInNeighbor;
        neighborSideIndexInParent = sideIndex;
      }
    }
    
    if (bothBroken) {
      // match all the children -- we assume RefinementPatterns are compatible (e.g. divisions always by 1/2s)
      vector< pair<unsigned,unsigned> > childrenForSide = elem->childIndicesForSide(sideIndex);
      for (int childIndexInSide=0; childIndexInSide < childrenForSide.size(); childIndexInSide++) {
        int childIndex = childrenForSide[childIndexInSide].first;
        int childSideIndex = childrenForSide[childIndexInSide].second;
        matchNeighbor(elem->getChild(childIndex),childSideIndex);
      }
      // all our children matched => we're done:
      return;
    } else { // one broken
      vector< pair< unsigned, unsigned> > childrenForSide = parent->childIndicesForSide(neighborSideIndexInParent);
      
      if ( childrenForSide.size() > 1 ) { // then parent is broken along side, and neighbor isn't...
        vector< pair< int, int> > descendantsForSide;
        if ( !_usePatchBasis ) { // MultiBasis
          descendantsForSide = parent->getDescendantsForSide(neighborSideIndexInParent);
          
          Teuchos::RCP<DofOrdering> nonParentTrialOrdering = nonParent->elementType()->trialOrderPtr;

          getMultiBasisOrdering( nonParentTrialOrdering, parent, neighborSideIndexInParent,
                                parentSideIndexInNeighbor, nonParent );
          ElementTypePtr nonParentType = _elementTypeFactory.getElementType(nonParentTrialOrdering, 
                                                                            nonParent->elementType()->testOrderPtr, 
                                                                            nonParent->elementType()->cellTopoPtr );
          setElementType(nonParent->cellID(), nonParentType, true); // true: only a side upgrade
          //nonParent->setElementType(nonParentType);
          // debug code:
          if ( nonParentTrialOrdering->hasSideVarIDs() ) { // then we can check whether there is a multi-basis
            if ( ! _dofOrderingFactory.sideHasMultiBasis(nonParentTrialOrdering, parentSideIndexInNeighbor) ) {
              TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "failed to add multi-basis to neighbor");
            }
          }
        } else { // PatchBasis
          // check to see if non-parent needs a p-upgrade
          int maxPolyOrder, minPolyOrder; 
          this->maxMinPolyOrder(maxPolyOrder, minPolyOrder, nonParent,parentSideIndexInNeighbor);
          DofOrderingPtr nonParentTrialOrdering = nonParent->elementType()->trialOrderPtr;
          DofOrderingPtr    parentTrialOrdering =    parent->elementType()->trialOrderPtr;
          
          int    parentPolyOrder = _dofOrderingFactory.trialPolyOrder(   parentTrialOrdering);
          int nonParentPolyOrder = _dofOrderingFactory.trialPolyOrder(nonParentTrialOrdering);
          
          if (maxPolyOrder > nonParentPolyOrder) {
            // upgrade p along the side in non-parent
            nonParentTrialOrdering = _dofOrderingFactory.setSidePolyOrder(nonParentTrialOrdering, parentSideIndexInNeighbor, maxPolyOrder, false);
            ElementTypePtr nonParentType = _elementTypeFactory.getElementType(nonParentTrialOrdering, 
                                                                              nonParent->elementType()->testOrderPtr, 
                                                                              nonParent->elementType()->cellTopoPtr );
            setElementType(nonParent->cellID(), nonParentType, true); // true: only a side upgrade

          }
          // now, importantly, do the same thing in the parent:
          if (maxPolyOrder > parentPolyOrder) {
            parentTrialOrdering = _dofOrderingFactory.setSidePolyOrder(parentTrialOrdering, neighborSideIndexInParent, maxPolyOrder, false);
            ElementTypePtr parentType = _elementTypeFactory.getElementType(parentTrialOrdering,
                                                                           parent->elementType()->testOrderPtr,
                                                                           parent->elementType()->cellTopoPtr);
            setElementType(parent->cellID(), parentType, true); // true: only a side upgrade            
          }

          // get all descendants, not just leaf nodes, for the PatchBasis upgrade
          // could make more efficient by checking whether we really need to upgrade the whole hierarchy, but 
          // this is probably more trouble than it's worth, unless we end up with some *highly* irregular meshes.
          descendantsForSide = parent->getDescendantsForSide(neighborSideIndexInParent, false);
          vector< pair< int, int> >::iterator entryIt;
          for ( entryIt=descendantsForSide.begin(); entryIt != descendantsForSide.end(); entryIt++) {
            int childCellID = (*entryIt).first;
            ElementPtr child = _elements[childCellID];
            int childSideIndex = (*entryIt).second;
            DofOrderingPtr childTrialOrdering = child->elementType()->trialOrderPtr;
            getPatchBasisOrdering(childTrialOrdering,child,childSideIndex);
            ElementTypePtr childType = _elementTypeFactory.getElementType(childTrialOrdering, 
                                                                          child->elementType()->testOrderPtr, 
                                                                          child->elementType()->cellTopoPtr );
            setElementType(childCellID,childType,true); //true: sideUpgradeOnly
            //child->setElementType(childType);
          }
        }
        
        vector< pair< int, int> >::iterator entryIt;
        for ( entryIt=descendantsForSide.begin(); entryIt != descendantsForSide.end(); entryIt++) {
          int childCellID = (*entryIt).first;
          int childSideIndex = (*entryIt).second;
          _boundary.deleteElement(childCellID, childSideIndex);
        }
        // by virtue of having assigned the patch- or multi-basis, we've already matched p-order ==> we're done
        return;
      }
    }
  }
  // p-refinement handling:
  const shards::CellTopology neighborTopo = *(neighbor->elementType()->cellTopoPtr.get());
  Teuchos::RCP<DofOrdering> elemTrialOrdering = elem->elementType()->trialOrderPtr;
  Teuchos::RCP<DofOrdering> elemTestOrdering = elem->elementType()->testOrderPtr;
  
  Teuchos::RCP<DofOrdering> neighborTrialOrdering = neighbor->elementType()->trialOrderPtr;
  Teuchos::RCP<DofOrdering> neighborTestOrdering = neighbor->elementType()->testOrderPtr;
  
  int changed = _dofOrderingFactory.matchSides(elemTrialOrdering, sideIndex, cellTopo,
                                               neighborTrialOrdering, mySideIndexInNeighbor, neighborTopo);
  // changed == 1 for me, 2 for neighbor, 0 for neither, -1 for PatchBasis
  if (changed==1) {
    TEUCHOS_TEST_FOR_EXCEPTION(_bilinearForm->trialBoundaryIDs().size() == 0,
                       std::invalid_argument,
                       "BilinearForm has no traces or fluxes, but somehow element was upgraded...");
    int boundaryVarID = _bilinearForm->trialBoundaryIDs()[0];
    int neighborSidePolyOrder = BasisFactory::basisPolyOrder(neighborTrialOrdering->getBasis(boundaryVarID,mySideIndexInNeighbor));
    int mySidePolyOrder = BasisFactory::basisPolyOrder(elemTrialOrdering->getBasis(boundaryVarID,sideIndex));
    TEUCHOS_TEST_FOR_EXCEPTION(mySidePolyOrder != neighborSidePolyOrder,
                       std::invalid_argument,
                       "After matchSides(), the appropriate sides don't have the same order.");
    int testPolyOrder = _dofOrderingFactory.testPolyOrder(elemTestOrdering);
    if (testPolyOrder < mySidePolyOrder + _pToAddToTest) {
      elemTestOrdering = _dofOrderingFactory.testOrdering( mySidePolyOrder + _pToAddToTest, cellTopo);
    }
    ElementTypePtr newType = _elementTypeFactory.getElementType(elemTrialOrdering, elemTestOrdering, 
                                                                elem->elementType()->cellTopoPtr );
    setElementType(elem->cellID(), newType, true); // true: 
//    elem->setElementType( _elementTypeFactory.getElementType(elemTrialOrdering, elemTestOrdering, 
//                                                             elem->elementType()->cellTopoPtr ) );
    //return ELEMENT_NEEDED_NEW;
  } else if (changed==2) {
    // if need be, upgrade neighborTestOrdering as well.
    TEUCHOS_TEST_FOR_EXCEPTION(_bilinearForm->trialBoundaryIDs().size() == 0,
                       std::invalid_argument,
                       "BilinearForm has no traces or fluxes, but somehow neighbor was upgraded...");
    TEUCHOS_TEST_FOR_EXCEPTION(neighborTrialOrdering.get() == neighbor->elementType()->trialOrderPtr.get(),
                       std::invalid_argument,
                       "neighborTrialOrdering was supposed to be upgraded, but remains unchanged...");
    int boundaryVarID = _bilinearForm->trialBoundaryIDs()[0];
    int sidePolyOrder = BasisFactory::basisPolyOrder(neighborTrialOrdering->getBasis(boundaryVarID,mySideIndexInNeighbor));
    int mySidePolyOrder = BasisFactory::basisPolyOrder(elemTrialOrdering->getBasis(boundaryVarID,sideIndex));
    TEUCHOS_TEST_FOR_EXCEPTION(mySidePolyOrder != sidePolyOrder,
                       std::invalid_argument,
                       "After matchSides(), the appropriate sides don't have the same order.");
    int testPolyOrder = _dofOrderingFactory.testPolyOrder(neighborTestOrdering);
    if (testPolyOrder < sidePolyOrder + _pToAddToTest) {
      neighborTestOrdering = _dofOrderingFactory.testOrdering( sidePolyOrder + _pToAddToTest, neighborTopo);
    }
    ElementTypePtr newType = _elementTypeFactory.getElementType(neighborTrialOrdering, neighborTestOrdering, 
                                                                neighbor->elementType()->cellTopoPtr );
    setElementType( neighbor->cellID(), newType, true); // true: sideUpgradeOnly
    //return NEIGHBOR_NEEDED_NEW;
  } else if (changed == -1) { // PatchBasis
    // if we get here, these are the facts:
    // 1. both element and neighbor are unbroken--leaf nodes.
    // 2. one of element or neighbor has a PatchBasis
    
    TEUCHOS_TEST_FOR_EXCEPTION(_bilinearForm->trialBoundaryIDs().size() == 0,
                       std::invalid_argument,
                       "BilinearForm has no traces or fluxes, but somehow neighbor was upgraded...");
    int boundaryVarID = _bilinearForm->trialBoundaryIDs()[0];
    
    // So what we need to do is figure out the right p-order for the side and set both bases accordingly.
    // determine polyOrder for each side--take the maximum
    int neighborPolyOrder = _dofOrderingFactory.trialPolyOrder(neighborTrialOrdering);
    int myPolyOrder = _dofOrderingFactory.trialPolyOrder(elemTrialOrdering);
    
    int polyOrder = max(neighborPolyOrder,myPolyOrder); // "maximum" rule
    
    // upgrade element
    elemTrialOrdering = _dofOrderingFactory.setSidePolyOrder(elemTrialOrdering,sideIndex,polyOrder,true);
    int sidePolyOrder = BasisFactory::basisPolyOrder(elemTrialOrdering->getBasis(boundaryVarID,mySideIndexInNeighbor));
    int testPolyOrder = _dofOrderingFactory.testPolyOrder(elemTestOrdering);
    if (testPolyOrder < sidePolyOrder + _pToAddToTest) {
      elemTestOrdering = _dofOrderingFactory.testOrdering( sidePolyOrder + _pToAddToTest, cellTopo );
    }
    ElementTypePtr newElemType = _elementTypeFactory.getElementType(elemTrialOrdering, elemTestOrdering, 
                                                                    elem->elementType()->cellTopoPtr );
    setElementType( elem->cellID(), newElemType, true); // true: sideUpgradeOnly
    
    // upgrade neighbor
    neighborTrialOrdering = _dofOrderingFactory.setSidePolyOrder(neighborTrialOrdering,mySideIndexInNeighbor,polyOrder,true);
    testPolyOrder = _dofOrderingFactory.testPolyOrder(neighborTestOrdering);
    if (testPolyOrder < sidePolyOrder + _pToAddToTest) {
      neighborTestOrdering = _dofOrderingFactory.testOrdering( sidePolyOrder + _pToAddToTest, neighborTopo);
    }
    ElementTypePtr newNeighborType = _elementTypeFactory.getElementType(neighborTrialOrdering, neighborTestOrdering, 
                                                                        neighbor->elementType()->cellTopoPtr );
    setElementType( neighbor->cellID(), newNeighborType, true); // true: sideUpgradeOnly
    
    // TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "PatchBasis support still a work in progress!");
  } else {
    //return NEITHER_NEEDED_NEW;
  }
}

void Mesh::maxMinPolyOrder(int &maxPolyOrder, int &minPolyOrder, ElementPtr elem, int sideIndex) {
  // returns maximum polyOrder (on interior/field variables) of this element and any that border it on the given side
  int mySideIndexInNeighbor;
  ElementPtr neighbor = ancestralNeighborForSide(elem, sideIndex, mySideIndexInNeighbor);
  if ((neighbor.get() == NULL) || (neighbor->cellID() < 0)) { // as presently implemented, neighbor won't be NULL, but an "empty" elementPtr, _nullPtr, with cellID -1.  I'm a bit inclined to think a NULL would be better.  The _nullPtr conceit comes from the early days of the Mesh class, and seems a bit weird now.
    minPolyOrder = _dofOrderingFactory.trialPolyOrder(elem->elementType()->trialOrderPtr);
    maxPolyOrder = minPolyOrder;
    return;
  }
  maxPolyOrder = max(_dofOrderingFactory.trialPolyOrder(neighbor->elementType()->trialOrderPtr),
                     _dofOrderingFactory.trialPolyOrder(elem->elementType()->trialOrderPtr));
  minPolyOrder = min(_dofOrderingFactory.trialPolyOrder(neighbor->elementType()->trialOrderPtr),
                     _dofOrderingFactory.trialPolyOrder(elem->elementType()->trialOrderPtr));
  int ancestorSideIndex;
  Element* ancestor;
  neighbor->getNeighbor(ancestor,ancestorSideIndex,mySideIndexInNeighbor);
  Element* parent = NULL;
  int parentSideIndex;
  if (ancestor->isParent()) {
    parent = ancestor;
    parentSideIndex = ancestorSideIndex;
  } else if (neighbor->isParent()) {
    parent = neighbor.get();
    parentSideIndex = mySideIndexInNeighbor;
  }
  if (parent != NULL) {
    vector< pair< int, int> > descendantSides = parent->getDescendantsForSide(parentSideIndex);
    vector< pair< int, int> >::iterator sideIt;
    for (sideIt = descendantSides.begin(); sideIt != descendantSides.end(); sideIt++) {
      int descendantID = sideIt->first;
      int descOrder = _dofOrderingFactory.trialPolyOrder(_elements[descendantID]->elementType()->trialOrderPtr);
      maxPolyOrder = max(maxPolyOrder,descOrder);
      minPolyOrder = min(minPolyOrder,descOrder);
    }
  }
}

map< int, BasisPtr > Mesh::multiBasisUpgradeMap(ElementPtr parent, int sideIndex, int bigNeighborPolyOrder) {
  if (bigNeighborPolyOrder==-1) {
    // assumption is that we're at the top level: so parent's neighbor on sideIndex exists/is a peer
    int bigNeighborCellID = parent->getNeighborCellID(sideIndex);
    ElementPtr bigNeighbor = getElement(bigNeighborCellID);
    bigNeighborPolyOrder = _dofOrderingFactory.trialPolyOrder( bigNeighbor->elementType()->trialOrderPtr );
  }
  vector< pair< unsigned, unsigned> > childrenForSide = parent->childIndicesForSide(sideIndex);
  map< int, BasisPtr > varIDsToUpgrade;
  vector< map< int, BasisPtr > > childVarIDsToUpgrade;
  vector< pair< unsigned, unsigned> >::iterator entryIt;
  for ( entryIt=childrenForSide.begin(); entryIt != childrenForSide.end(); entryIt++) {
    int childCellIndex = (*entryIt).first;
    int childSideIndex = (*entryIt).second;
    ElementPtr childCell = parent->getChild(childCellIndex);
    
    // new code 2-16-13:
    while (childCell->isParent() && (childCell->childIndicesForSide(childSideIndex).size() == 1) ) {
      pair<int, int> childEntry = childCell->childIndicesForSide(childSideIndex)[0];
      childSideIndex = childEntry.second;
      childCell = childCell->getChild(childEntry.first);
    } // end new code 2-16-13
    
    if ( childCell->isParent() && (childCell->childIndicesForSide(childSideIndex).size() > 1)) {
      childVarIDsToUpgrade.push_back( multiBasisUpgradeMap(childCell,childSideIndex,bigNeighborPolyOrder) );
    } else {
      DofOrderingPtr childTrialOrder = childCell->elementType()->trialOrderPtr;
      
      int childPolyOrder = _dofOrderingFactory.trialPolyOrder( childTrialOrder );
      
      if (bigNeighborPolyOrder > childPolyOrder) {
        // upgrade child p along side
        childTrialOrder = _dofOrderingFactory.setSidePolyOrder(childTrialOrder, childSideIndex, bigNeighborPolyOrder, false);
        ElementTypePtr newChildType = _elementTypeFactory.getElementType(childTrialOrder, 
                                                                         childCell->elementType()->testOrderPtr, 
                                                                         childCell->elementType()->cellTopoPtr );
        setElementType(childCell->cellID(), newChildType, true); // true: only a side upgrade
      }
      
      pair< DofOrderingPtr,int > entry = make_pair(childTrialOrder,childSideIndex);
      vector< pair< DofOrderingPtr,int > > childTrialOrdersForSide;
      childTrialOrdersForSide.push_back(entry);
      childVarIDsToUpgrade.push_back( _dofOrderingFactory.getMultiBasisUpgradeMap(childTrialOrdersForSide) );
    }
  }
  map< int, BasisPtr >::iterator varMapIt;
  for (varMapIt=childVarIDsToUpgrade[0].begin(); varMapIt != childVarIDsToUpgrade[0].end(); varMapIt++) {
    int varID = (*varMapIt).first;
    vector< BasisPtr > bases;
    int numChildrenInSide = childVarIDsToUpgrade.size();
    for (int childIndex = 0; childIndex<numChildrenInSide; childIndex++) {
      //permute child index (this is from neighbor's point of view)
      int permutedChildIndex = neighborChildPermutation(childIndex,numChildrenInSide);
      if (! childVarIDsToUpgrade[permutedChildIndex][varID].get()) {
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "null basis");
      }
      bases.push_back(childVarIDsToUpgrade[permutedChildIndex][varID]);
    }
    BasisPtr multiBasis = BasisFactory::getMultiBasis(bases);
    // debugging:
//    ((MultiBasis*)multiBasis.get())->printInfo();
    varIDsToUpgrade[varID] = multiBasis;
  }
  return varIDsToUpgrade;
}

int Mesh::numActiveElements() {
  return _activeElements.size();
}

int Mesh::numElements() {
  return _elements.size();
}

int Mesh::numElementsOfType( Teuchos::RCP< ElementType > elemTypePtr ) {
  // returns the global total (across all MPI nodes)
  int numElements = 0;
  for (int partitionNumber=0; partitionNumber<_numPartitions; partitionNumber++) {
    if (   _partitionedPhysicalCellNodesForElementType[partitionNumber].find( elemTypePtr.get() )
        != _partitionedPhysicalCellNodesForElementType[partitionNumber].end() ) {
      numElements += _partitionedPhysicalCellNodesForElementType[partitionNumber][ elemTypePtr.get() ].dimension(0);
    }
  }
  return numElements;
}

int Mesh::numFluxDofs(){
  return numGlobalDofs()-numFieldDofs();
}

int Mesh::numFieldDofs(){
  int numFieldDofs = 0;  
  int numActiveElems = numActiveElements();
  for (int cellIndex = 0; cellIndex < numActiveElems; cellIndex++){
    ElementPtr elemPtr = getActiveElement(cellIndex);
    int cellID = elemPtr->cellID();
    int numSides = elemPtr->numSides();
    ElementTypePtr elemTypePtr = elemPtr->elementType();
    vector< int > fieldIDs = _bilinearForm->trialVolumeIDs();
    vector< int >::iterator fieldIDit;
    for (fieldIDit = fieldIDs.begin(); fieldIDit != fieldIDs.end() ; fieldIDit++){    
      int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(*fieldIDit,0);
      numFieldDofs += numDofs;
    }
  }
  return numFieldDofs; // don't count double - each side is shared by two elems, we count it twice
}


int Mesh::numGlobalDofs() {
  return _numGlobalDofs;
}

int Mesh::parityForSide(int cellID, int sideIndex) {
  ElementPtr elem = _elements[cellID];
  if ( elem->isParent() ) {
    // then it's not an active element, so we should use _cellSideParitiesForCellID
    // (we could always use this, but for tests I'd rather expose what's actually used in computation)
    return _cellSideParitiesForCellID[cellID][sideIndex];
  }
  // if we get here, then we have an active element...
  ElementTypePtr elemType = elem->elementType();
  int cellIndex = elem->cellIndex();
  int partitionNumber = partitionForCellID(cellID);
  int parity = _partitionedCellSideParitiesForElementType[partitionNumber][elemType.get()](cellIndex,sideIndex);
  
  if (_cellSideParitiesForCellID[cellID][sideIndex] != parity ) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "parity lookups don't match");
  }
  return parity;
}

int Mesh::partitionForCellID( int cellID ) {
  return _partitionForCellID[ cellID ];
}

int Mesh::partitionForGlobalDofIndex( int globalDofIndex ) {
  if ( _partitionForGlobalDofIndex.find( globalDofIndex ) == _partitionForGlobalDofIndex.end() ) {
    return -1;
  }
  return _partitionForGlobalDofIndex[ globalDofIndex ];
}

int Mesh::partitionLocalIndexForGlobalDofIndex( int globalDofIndex ) {
  return _partitionLocalIndexForGlobalDofIndex[ globalDofIndex ];
}

FieldContainer<double> & Mesh::physicalCellNodes( Teuchos::RCP< ElementType > elemTypePtr) {
#ifdef HAVE_MPI
  int partitionNumber     = Teuchos::GlobalMPISession::getRank();
#else
  int partitionNumber     = 0;
#endif
  return _partitionedPhysicalCellNodesForElementType[ partitionNumber ][ elemTypePtr.get() ];
}

FieldContainer<double> Mesh::physicalCellNodesForCell( int cellID ) {
  ElementPtr elem = _elements[cellID];
  int vertexCount = elem->elementType()->cellTopoPtr->getVertexCount();
  int spaceDim = elem->elementType()->cellTopoPtr->getDimension();
  int numCells = 1;
  FieldContainer<double> physicalCellNodes(numCells,vertexCount,spaceDim);
  
  FieldContainer<double> cellVertices(vertexCount,spaceDim);
  vector<unsigned> vertexIndices = _meshTopology->getCell(cellID)->vertices();
  for (int vertex=0; vertex<vertexCount; vertex++) {
    unsigned vertexIndex = vertexIndices[vertex];
    for (int i=0; i<spaceDim; i++) {
      physicalCellNodes(0,vertex,i) = _meshTopology->getVertex(vertexIndex)[i];
    }
  }
  return physicalCellNodes;
}

FieldContainer<double> & Mesh::physicalCellNodesGlobal( Teuchos::RCP< ElementType > elemTypePtr ) {
  return _physicalCellNodesForElementType[ elemTypePtr.get() ];
}

void Mesh::printLocalToGlobalMap() {
  for (map< pair<int,int>, int>::iterator entryIt = _localToGlobalMap.begin();
       entryIt != _localToGlobalMap.end(); entryIt++) {
    int cellID = entryIt->first.first;
    int localDofIndex = entryIt->first.second;
    int globalDofIndex = entryIt->second;
    cout << "(" << cellID << "," << localDofIndex << ") --> " << globalDofIndex << endl;
  }
}

void Mesh::printVertices() {
  cout << "Vertices:\n";
  unsigned vertexDim = 0;
  unsigned vertexCount = _meshTopology->getEntityCount(vertexDim);
  for (unsigned vertexIndex=0; vertexIndex<vertexCount; vertexIndex++) {
    vector<double> vertex = _meshTopology->getVertex(vertexIndex);
    cout << vertexIndex << ": (" << vertex[0] << ", " << vertex[1] << ")\n";
  }
}

void Mesh::rebuildLookups() {
  _cellSideUpgrades.clear();
  determineActiveElements();
  buildTypeLookups(); // build data structures for efficient lookup by element type
  buildLocalToGlobalMap();
  determinePartitionDofIndices();
  _boundary.buildLookupTables();
  //cout << "Mesh.numGlobalDofs: " << numGlobalDofs() << endl;
}

void Mesh::registerObserver(Teuchos::RCP<RefinementObserver> observer) {
  _registeredObservers.push_back(observer);
}

void Mesh::registerSolution(Teuchos::RCP<Solution> solution) {
  _registeredSolutions.push_back( solution.get() );
}

void Mesh::unregisterObserver(RefinementObserver* observer) {
  for (vector< Teuchos::RCP<RefinementObserver> >::iterator meshIt = _registeredObservers.begin();
       meshIt != _registeredObservers.end(); meshIt++) {
    if ( (*meshIt).get() == observer ) {
      _registeredObservers.erase(meshIt);
      return;
    }
  }
  cout << "WARNING: Mesh::unregisterObserver: Observer not found.\n";
}

void Mesh::unregisterObserver(Teuchos::RCP<RefinementObserver> mesh) {
  this->unregisterObserver(mesh.get());
}

void Mesh::unregisterSolution(Teuchos::RCP<Solution> solution) {
  for (vector< Solution* >::iterator solnIt = _registeredSolutions.begin();
       solnIt != _registeredSolutions.end(); solnIt++) {
    if ( *solnIt == solution.get() ) {
      _registeredSolutions.erase(solnIt);
      return;
    }
  }
  cout << "Mesh::unregisterSolution: Solution not found.\n";
}

void Mesh::pRefine(const vector<int> &cellIDsForPRefinements) {
  set<int> cellSet;
  for (vector<int>::const_iterator cellIt=cellIDsForPRefinements.begin();
       cellIt != cellIDsForPRefinements.end(); cellIt++) {
    cellSet.insert(*cellIt);
  }
  pRefine(cellSet);
}

void Mesh::pRefine(const set<int> &cellIDsForPRefinements){
  pRefine(cellIDsForPRefinements,1);
}

void Mesh::pRefine(const set<int> &cellIDsForPRefinements, int pToAdd) {
  // refine any registered meshes
  for (vector< Teuchos::RCP<RefinementObserver> >::iterator meshIt = _registeredObservers.begin();
       meshIt != _registeredObservers.end(); meshIt++) {
    (*meshIt)->pRefine(cellIDsForPRefinements);
  }
  
  // p-refinements:
  // 1. Loop through cellIDsForPRefinements:
  //   a. create new DofOrderings for trial and test
  //   b. create and set new element type
  //   c. Loop through sides, calling matchNeighbor for each
  
  // 1. Loop through cellIDsForPRefinements:
  set<int>::const_iterator cellIt;
  map<int, ElementTypePtr > oldTypes;
  for (cellIt=cellIDsForPRefinements.begin(); cellIt != cellIDsForPRefinements.end(); cellIt++) {
    int cellID = *cellIt;
    oldTypes[cellID] = _elements[cellID]->elementType();
  }
  for (cellIt=cellIDsForPRefinements.begin(); cellIt != cellIDsForPRefinements.end(); cellIt++) {
    int cellID = *cellIt;
    ElementPtr elem = _elements[cellID];
    ElementTypePtr oldElemType = oldTypes[cellID];
    const shards::CellTopology cellTopo = *(elem->elementType()->cellTopoPtr.get());
    //   a. create new DofOrderings for trial and test
    Teuchos::RCP<DofOrdering> currentTrialOrdering, currentTestOrdering;
    currentTrialOrdering = elem->elementType()->trialOrderPtr;
    currentTestOrdering  = elem->elementType()->testOrderPtr;
    Teuchos::RCP<DofOrdering> newTrialOrdering = _dofOrderingFactory.pRefineTrial(currentTrialOrdering,
                                                                                  cellTopo,pToAdd);
    Teuchos::RCP<DofOrdering> newTestOrdering;
    // determine what newTestOrdering should be:
    int trialPolyOrder = _dofOrderingFactory.trialPolyOrder(newTrialOrdering);
    int testPolyOrder = _dofOrderingFactory.testPolyOrder(currentTestOrdering);
    if (testPolyOrder < trialPolyOrder + _pToAddToTest) {
      newTestOrdering = _dofOrderingFactory.testOrdering( trialPolyOrder + _pToAddToTest, cellTopo);
    } else {
      newTestOrdering = currentTestOrdering;
    }

    ElementTypePtr newType = _elementTypeFactory.getElementType(newTrialOrdering, newTestOrdering, 
                                                                elem->elementType()->cellTopoPtr );
    setElementType(cellID,newType,false); // false: *not* sideUpgradeOnly
    
//    elem->setElementType( _elementTypeFactory.getElementType(newTrialOrdering, newTestOrdering, 
//                                                             elem->elementType()->cellTopoPtr ) );
    
    int numSides = elem->numSides();
    for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
      // get and match the big neighbor along the side, if we're a small element…
      int neighborSideIndex;
      ElementPtr neighborToMatch = ancestralNeighborForSide(elem,sideIndex,neighborSideIndex);
      
      if (neighborToMatch->cellID() != -1) { // then we have a neighbor to match along that side...
        matchNeighbor(neighborToMatch,neighborSideIndex);
      }
    }
    
    // let transformation function know about the refinement that just took place
    if (_meshTopology->transformationFunction().get()) {
      set<int> cellIDset;
      cellIDset.insert(cellID);
      _meshTopology->transformationFunction()->didPRefine(cellIDset);
    }
    
    for (vector< Solution* >::iterator solutionIt = _registeredSolutions.begin();
         solutionIt != _registeredSolutions.end(); solutionIt++) {
      // do projection: for p-refinements, the "child" is the same cell
      vector<int> childIDs(1,cellID);
      (*solutionIt)->processSideUpgrades(_cellSideUpgrades,cellIDsForPRefinements);
      (*solutionIt)->projectOldCellOntoNewCells(cellID,oldElemType,childIDs);
    }
    _cellSideUpgrades.clear(); // these have been processed by all solutions that will ever have a chance to process them.
  }
  rebuildLookups();
    
  // now discard any old coefficients
  for (vector< Solution* >::iterator solutionIt = _registeredSolutions.begin();
       solutionIt != _registeredSolutions.end(); solutionIt++) {
    (*solutionIt)->discardInactiveCellCoefficients();
  }
}

int Mesh::condensedRowSizeUpperBound() {
  // includes multiplicity
  vector< Teuchos::RCP< ElementType > >::iterator elemTypeIt;
  int maxRowSize = 0;
  for (int partitionNumber=0; partitionNumber < _numPartitions; partitionNumber++) {
    for (elemTypeIt = _elementTypesForPartition[partitionNumber].begin(); 
         elemTypeIt != _elementTypesForPartition[partitionNumber].end();
         elemTypeIt++) {
      ElementTypePtr elemTypePtr = *elemTypeIt;
      int numSides = elemTypePtr->cellTopoPtr->getSideCount();
      vector< int > fluxIDs = _bilinearForm->trialBoundaryIDs();
      vector< int >::iterator fluxIDIt;
      int numFluxDofs = 0;
      for (fluxIDIt = fluxIDs.begin(); fluxIDIt != fluxIDs.end(); fluxIDIt++) {
        int fluxID = *fluxIDIt;
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(fluxID,sideIndex);
          numFluxDofs += numDofs;
        }
      }
      int maxPossible = numFluxDofs * 2 + numSides*fluxIDs.size();  // a side can be shared by 2 elements, and vertices can be shared
      maxRowSize = max(maxPossible, maxRowSize);
    }
  }
  return maxRowSize;
}

int Mesh::rowSizeUpperBound() {
  // includes multiplicity
  vector< Teuchos::RCP< ElementType > >::iterator elemTypeIt;
  int maxRowSize = 0;
  for (int partitionNumber=0; partitionNumber < _numPartitions; partitionNumber++) {
    for (elemTypeIt = _elementTypesForPartition[partitionNumber].begin(); 
         elemTypeIt != _elementTypesForPartition[partitionNumber].end();
         elemTypeIt++) {
      ElementTypePtr elemTypePtr = *elemTypeIt;
      int numSides = elemTypePtr->cellTopoPtr->getSideCount();
      vector< int > fluxIDs = _bilinearForm->trialBoundaryIDs();
      vector< int >::iterator fluxIDIt;
      int numFluxDofs = 0;
      for (fluxIDIt = fluxIDs.begin(); fluxIDIt != fluxIDs.end(); fluxIDIt++) {
        int fluxID = *fluxIDIt;
        for (int sideIndex=0; sideIndex<numSides; sideIndex++) {
          int numDofs = elemTypePtr->trialOrderPtr->getBasisCardinality(fluxID,sideIndex);
          numFluxDofs += numDofs;
        }
      }
      int numFieldDofs = elemTypePtr->trialOrderPtr->totalDofs() - numFluxDofs;
      int maxPossible = numFluxDofs * 2 + numSides*fluxIDs.size() + numFieldDofs;  // a side can be shared by 2 elements, and vertices can be shared
      maxRowSize = max(maxPossible, maxRowSize);
    }
  }
  return maxRowSize;
}

vector< ParametricCurvePtr > Mesh::parametricEdgesForCell(int cellID, bool neglectCurves) {
  return _meshTopology->parametricEdgesForCell(cellID, neglectCurves);
}

// TODO: consider adding/moving this logic into MeshTopology
void Mesh::setEdgeToCurveMap(const map< pair<int, int>, ParametricCurvePtr > &edgeToCurveMap) {
  MeshPtr thisPtr = Teuchos::rcp(this, false);
  _meshTopology->setEdgeToCurveMap(edgeToCurveMap, thisPtr);
}

void Mesh::setElementType(int cellID, ElementTypePtr newType, bool sideUpgradeOnly) {
  ElementPtr elem = _elements[cellID];
  if (sideUpgradeOnly) { // need to track in _cellSideUpgrades
    ElementTypePtr oldType;
    map<int, pair<ElementTypePtr, ElementTypePtr> >::iterator existingEntryIt = _cellSideUpgrades.find(cellID);
    if (existingEntryIt != _cellSideUpgrades.end() ) {
      oldType = (existingEntryIt->second).first;
    } else {
      oldType = elem->elementType();
      if (oldType.get() == newType.get()) {
        // no change is actually happening
        return;
      }
    }
//    cout << "setting element type for cellID " << cellID << " (sideUpgradeOnly=" << sideUpgradeOnly << ")\n";
//    cout << "trialOrder old size: " << oldType->trialOrderPtr->totalDofs() << endl;
//    cout << "trialOrder new size: " << newType->trialOrderPtr->totalDofs() << endl;
    _cellSideUpgrades[cellID] = make_pair(oldType,newType);
  }
  elem->setElementType(newType);
}

void Mesh::setEnforceMultiBasisFluxContinuity( bool value ) {
  _enforceMBFluxContinuity = value;
}

void Mesh::setNeighbor(ElementPtr elemPtr, int elemSide, ElementPtr neighborPtr, int neighborSide) {
  elemPtr->setNeighbor(elemSide,neighborPtr,neighborSide);
  neighborPtr->setNeighbor(neighborSide,elemPtr,elemSide);
  double parity;
  if (neighborPtr->cellID() < 0) {
    // boundary
    parity = 1.0;
  } else if (elemPtr->cellID() < neighborPtr->cellID()) {
    parity = 1.0;
  } else {
    parity = -1.0;
  }
  _cellSideParitiesForCellID[elemPtr->cellID()][elemSide] = parity;
//  cout << "setNeighbor: set cellSideParity for cell " << elemPtr->cellID() << ", sideIndex " << elemSide << ": ";
//  cout << _cellSideParitiesForCellID[elemPtr->cellID()][elemSide] << endl;
  
  if (neighborPtr->cellID() > -1) {
    _cellSideParitiesForCellID[neighborPtr->cellID()][neighborSide] = -parity;
    if (neighborPtr->isParent()) { // then we need to set its children accordingly, too
      vector< pair< int, int> > descendantSides = neighborPtr->getDescendantsForSide(neighborSide);
      vector< pair< int, int> >::iterator sideIt;
      for (sideIt = descendantSides.begin(); sideIt != descendantSides.end(); sideIt++) {
        int descendantID = sideIt->first;
        int descendantSide = sideIt->second;
        _cellSideParitiesForCellID[descendantID][descendantSide] = -parity;
      }
    }
//    cout << "setNeighbor: set cellSideParity for cell " << neighborPtr->cellID() << ", sideIndex " << neighborSide << ": ";
//    cout << _cellSideParitiesForCellID[neighborPtr->cellID()][neighborSide] << endl;
  }
//  cout << "set cellID " << elemPtr->cellID() << "'s neighbor for side ";
//  cout << elemSide << " to cellID " << neighborPtr->cellID();
//  cout << " (neighbor's sideIndex: " << neighborSide << ")" << endl;
}

void Mesh::setPartitionPolicy(  Teuchos::RCP< MeshPartitionPolicy > partitionPolicy ) {
  _partitionPolicy = partitionPolicy;
  _maximumRule2D->setPartitionPolicy(partitionPolicy);
  rebuildLookups();
}

void Mesh::setUsePatchBasis( bool value ) {
  // TODO: throw an exception if we've already been refined??
  _usePatchBasis = value;
}

bool Mesh::usePatchBasis() {
  return _usePatchBasis;
}

MeshTopologyPtr Mesh::getTopology() {
  return _meshTopology;
}

vector<unsigned> Mesh::vertexIndicesForCell(int cellID) {
  return _meshTopology->getCell(cellID)->vertices();
}

FieldContainer<double> Mesh::vertexCoordinates(int vertexIndex) {
  int spaceDim = _meshTopology->getSpaceDim();
  FieldContainer<double> vertex(spaceDim);
  for (int d=0; d<spaceDim; d++) {
    vertex(d) = _meshTopology->getVertex(vertexIndex)[d];
  }
  return vertex;
}

void Mesh::verticesForCell(FieldContainer<double>& vertices, int cellID) {
  CellPtr cell = _meshTopology->getCell(cellID);
  vector<unsigned> vertexIndices = cell->vertices();
  int numVertices = vertexIndices.size();
  int spaceDim = _meshTopology->getSpaceDim();

  //vertices.resize(numVertices,dimension);
  for (unsigned vertexIndex = 0; vertexIndex < numVertices; vertexIndex++) {
    for (int d=0; d<spaceDim; d++) {
      vertices(vertexIndex,d) = _meshTopology->getVertex(vertexIndices[vertexIndex])[d];
    }
  }
}

// global across all MPI nodes:
void Mesh::verticesForElementType(FieldContainer<double>& vertices, ElementTypePtr elemTypePtr) {
  int spaceDim = _meshTopology->getSpaceDim();
  int numVertices = elemTypePtr->cellTopoPtr->getNodeCount();
  int numCells = numElementsOfType(elemTypePtr);
  vertices.resize(numCells,numVertices,spaceDim);

  Teuchos::Array<int> dim; // for an individual cell
  dim.push_back(numVertices);
  dim.push_back(spaceDim);
  
  for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
    int cellID = this->cellID(elemTypePtr,cellIndex);
    FieldContainer<double> cellVertices(dim,&vertices(cellIndex,0,0));
    this->verticesForCell(cellVertices, cellID);
  }
}

void Mesh::verticesForCells(FieldContainer<double>& vertices, vector<int> &cellIDs) {
  // all cells represented in cellIDs must have the same topology
  int spaceDim = _meshTopology->getSpaceDim();
  int numCells = cellIDs.size();
  
  if (numCells == 0) {
    vertices.resize(0,0,0);
    return;
  }
  unsigned firstCellID = cellIDs[0];
  int numVertices = _meshTopology->getCell(firstCellID)->vertices().size();
 
  vertices.resize(numCells,numVertices,spaceDim);

  Teuchos::Array<int> dim; // for an individual cell
  dim.push_back(numVertices);
  dim.push_back(spaceDim);

  for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
    int cellID = cellIDs[cellIndex];
    FieldContainer<double> cellVertices(dim,&vertices(cellIndex,0,0));
    this->verticesForCell(cellVertices, cellID);
//    cout << "vertices for cellID " << cellID << ":\n" << cellVertices;
  }
  
//  cout << "all vertices:\n" << vertices;
}

void Mesh::verticesForSide(FieldContainer<double>& vertices, int cellID, int sideIndex) {
  CellPtr cell = _meshTopology->getCell(cellID);
  int spaceDim = _meshTopology->getSpaceDim();
  int sideDim = spaceDim - 1;
  unsigned sideEntityIndex = cell->entityIndex(sideDim, sideIndex);
  vector<unsigned> vertexIndices = _meshTopology->getEntityVertexIndices(sideDim, sideEntityIndex);

  int numVertices = vertexIndices.size();
  vertices.resize(numVertices,spaceDim);
  
  for (unsigned vertexIndex = 0; vertexIndex < numVertices; vertexIndex++) {
    for (int d=0; d<spaceDim; d++) {
      vertices(vertexIndex,d) = _meshTopology->getVertex(vertexIndex)[d];
    }
  }
}

void Mesh::writeMeshPartitionsToFile(const string & fileName){
  ofstream myFile;
  myFile.open(fileName.c_str());
  myFile << "numPartitions="<<_numPartitions<<";"<<endl;

  int maxNumVertices=0;
  int maxNumElems=0;
  int spaceDim = 2;

  //initialize verts
  for (int i=0;i<_numPartitions;i++){
    vector< ElementPtr > elemsInPartition = _partitions[i];
    for (int l=0;l<spaceDim;l++){
      myFile << "verts{"<< i+1 <<","<< l+1 << "} = zeros(" << maxNumVertices << ","<< maxNumElems << ");"<< endl;
      for (int j=0;j<elemsInPartition.size();j++){
        int numVertices = elemsInPartition[j]->elementType()->cellTopoPtr->getVertexCount();
        FieldContainer<double> verts(numVertices,spaceDim); // gets resized inside verticesForCell
        verticesForCell(verts, elemsInPartition[j]->cellID());  //verts(numVertsForCell,dim)
        maxNumVertices = max(maxNumVertices,verts.dimension(0));
        maxNumElems = max(maxNumElems,(int)elemsInPartition.size());
//        spaceDim = verts.dimension(1);
      }
    }
  }  
  cout << "max number of elems = " << maxNumElems << endl;

  for (int i=0;i<_numPartitions;i++){
    vector< ElementPtr > elemsInPartition = _partitions[i];
    for (int l=0;l<spaceDim;l++){
      
      for (int j=0;j<elemsInPartition.size();j++){
        int numVertices = elemsInPartition[j]->elementType()->cellTopoPtr->getVertexCount();
        FieldContainer<double> vertices(numVertices,spaceDim);
        verticesForCell(vertices, elemsInPartition[j]->cellID());  //vertices(numVertsForCell,dim)
        
        // write vertex coordinates to file
        for (int k=0;k<numVertices;k++){
          myFile << "verts{"<< i+1 <<","<< l+1 <<"}("<< k+1 <<","<< j+1 <<") = "<< vertices(k,l) << ";"<<endl; // verts{numPartitions,spaceDim}
        }
      }
      
    }
  }
  myFile.close();
}

double Mesh::getCellMeasure(int cellID)
{
  FieldContainer<double> physicalCellNodes = physicalCellNodesForCell(cellID);
  ElementPtr elem =  elements()[cellID];
  Teuchos::RCP< ElementType > elemType = elem->elementType();
  Teuchos::RCP< shards::CellTopology > cellTopo = elemType->cellTopoPtr;  
  BasisCache basisCache(physicalCellNodes, *cellTopo, 1);
  return basisCache.getCellMeasures()(0);
}

double Mesh::getCellXSize(int cellID){
  ElementPtr elem = getElement(cellID);
  int spaceDim = 2; // assuming 2D
  int numSides = elem->numSides();
  TEUCHOS_TEST_FOR_EXCEPTION(numSides!=4, std::invalid_argument, "Anisotropic cell measures only defined for quads right now.");
  FieldContainer<double> vertices(numSides,spaceDim); 
  verticesForCell(vertices, cellID);
  double xDist = vertices(1,0)-vertices(0,0);
  double yDist = vertices(1,1)-vertices(0,1);
  return sqrt(xDist*xDist + yDist*yDist);
}

double Mesh::getCellYSize(int cellID){
  ElementPtr elem = getElement(cellID);
  int spaceDim = 2; // assuming 2D
  int numSides = elem->numSides();
  TEUCHOS_TEST_FOR_EXCEPTION(numSides!=4, std::invalid_argument, "Anisotropic cell measures only defined for quads right now.");
  FieldContainer<double> vertices(numSides,spaceDim); 
  verticesForCell(vertices, cellID);
  double xDist = vertices(3,0)-vertices(0,0);
  double yDist = vertices(3,1)-vertices(0,1);
  return sqrt(xDist*xDist + yDist*yDist);
}

vector<double> Mesh::getCellOrientation(int cellID){
  ElementPtr elem = getElement(cellID);
  int spaceDim = 2; // assuming 2D
  int numSides = elem->numSides();
  TEUCHOS_TEST_FOR_EXCEPTION(numSides!=4, std::invalid_argument, "Cell orientation only defined for quads right now.");
  FieldContainer<double> vertices(numSides,spaceDim); 
  verticesForCell(vertices, cellID);
  double xDist = vertices(3,0)-vertices(0,0);
  double yDist = vertices(3,1)-vertices(0,1);
  vector<double> orientation;
  orientation.push_back(xDist);
  orientation.push_back(yDist);
  return orientation;
}
