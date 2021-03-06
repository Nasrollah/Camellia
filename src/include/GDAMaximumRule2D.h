//
//  GDAMaximumRule2D.h
//  Camellia-debug
//
//  Created by Nate Roberts on 1/21/14.
//
//

#ifndef __Camellia_debug__GDAMaximumRule2D__
#define __Camellia_debug__GDAMaximumRule2D__

#include <iostream>

#include "GlobalDofAssignment.h"
#include "ElementTypeFactory.h"

class Solution;

class GDAMaximumRule2D : public GlobalDofAssignment {
  // much of this code copied from Mesh    
  
  // keep track of upgrades to the sides of cells since the last rebuild:
  // (used to remap solution coefficients)
  map< GlobalIndexType, pair< ElementTypePtr, ElementTypePtr > > _cellSideUpgrades; // cellID --> (oldType, newType)
  
  map< pair<GlobalIndexType,IndexType>, pair<GlobalIndexType,IndexType> > _dofPairingIndex; // key/values are (cellID,localDofIndex)
  // note that the FieldContainer for cellSideParities has dimensions (numCellsForType,numSidesForType),
  // and that the values are 1.0 or -1.0.  These are weights to account for the fact that fluxes are defined in
  // terms of an outward normal, and thus one cell's idea about the flux is the negative of its neighbor's.
  // We decide parity by cellID: the neighbor with the lower cellID gets +1, the higher gets -1.
  
  // call buildTypeLookups to rebuild the elementType data structures:
  map< ElementType*, map<IndexType, GlobalIndexType> > _globalCellIndexToCellID;
  vector< vector< ElementTypePtr > > _elementTypesForPartition;
  vector< ElementTypePtr > _elementTypeList;
  
  map<GlobalIndexType, PartitionIndexType> _partitionForGlobalDofIndex;
  map<GlobalIndexType, IndexType> _partitionLocalIndexForGlobalDofIndex;
  vector< map< ElementType*, FieldContainer<double> > > _partitionedPhysicalCellNodesForElementType;
  vector< map< ElementType*, FieldContainer<double> > > _partitionedCellSideParitiesForElementType;
  map< ElementType*, FieldContainer<double> > _physicalCellNodesForElementType; // for uniform mesh, just a single entry..
  vector< set<GlobalIndexType> > _partitionedGlobalDofIndices;
  map<GlobalIndexType, IndexType> _partitionLocalCellIndices; // keys are cellIDs; index is relative to both MPI node and ElementType
  map<GlobalIndexType, IndexType> _globalCellIndices; // keys are cellIDs; index is relative to ElementType
  
  map< pair<GlobalIndexType,IndexType>, GlobalIndexType> _localToGlobalMap; // pair<cellID, localDofIndex> --> globalDofIndex
    
  map<unsigned, GlobalIndexType> getGlobalVertexIDs(const FieldContainer<double> &vertexCoordinates);

  void addDofPairing(GlobalIndexType cellID1, IndexType dofIndex1, GlobalIndexType cellID2, IndexType dofIndex2);
  void buildTypeLookups();
  void buildLocalToGlobalMap();
  void determineDofPairings();
  void determinePartitionDofIndices();

  void getMultiBasisOrdering(DofOrderingPtr &originalNonParentOrdering, CellPtr parent, unsigned sideOrdinal, unsigned parentSideOrdinalInNeighbor, CellPtr nonParent);
  void matchNeighbor(GlobalIndexType cellID, int sideOrdinal);
  map< int, BasisPtr > multiBasisUpgradeMap(CellPtr parent, unsigned sideOrdinal, unsigned bigNeighborPolyOrder);
  
  void verticesForCells(FieldContainer<double>& vertices, vector<GlobalIndexType> &cellIDs);
  void verticesForCell(FieldContainer<double>& vertices, GlobalIndexType cellID);
  
  bool _enforceMBFluxContinuity;
  
  GlobalIndexType _numGlobalDofs;
public:
  GDAMaximumRule2D(MeshPtr mesh, VarFactory varFactory, DofOrderingFactoryPtr dofOrderingFactory, MeshPartitionPolicyPtr partitionPolicy,
                   unsigned initialH1OrderTrial, unsigned testOrderEnhancement, bool enforceMBFluxContinuity = false);
  
//  GlobalIndexType cellID(ElementTypePtr elemType, IndexType cellIndex, PartitionIndexType partitionNumber);
  FieldContainer<double> & cellSideParities( ElementTypePtr elemTypePtr );
  
  GlobalDofAssignmentPtr deepCopy();
  
  void didHRefine(const set<GlobalIndexType> &parentCellIDs);
  void didPRefine(const set<GlobalIndexType> &cellIDs, int deltaP);
  void didHUnrefine(const set<GlobalIndexType> &parentCellIDs);
  
  void didChangePartitionPolicy();
  
  ElementTypePtr elementType(GlobalIndexType cellID);
  vector< ElementTypePtr > elementTypes(PartitionIndexType partitionNumber);
  
  bool enforceConformityLocally() { return true; }
  
  int getH1Order(GlobalIndexType cellID);
    
  GlobalIndexType globalDofIndex(GlobalIndexType cellID, IndexType localDofIndex);
  set<GlobalIndexType> globalDofIndicesForCell(GlobalIndexType cellID);
  set<GlobalIndexType> globalDofIndicesForPartition(PartitionIndexType partitionNumber);

  GlobalIndexType globalDofCount();
  void interpretLocalData(GlobalIndexType cellID, const FieldContainer<double> &localDofs, FieldContainer<double> &globalDofs, FieldContainer<GlobalIndexType> &globalDofIndices);
  void interpretLocalBasisCoefficients(GlobalIndexType cellID, int varID, int sideOrdinal, const FieldContainer<double> &basisCoefficients,
                                       FieldContainer<double> &globalCoefficients, FieldContainer<GlobalIndexType> &globalDofIndices);
  void interpretGlobalCoefficients(GlobalIndexType cellID, FieldContainer<double> &localCoefficients, const Epetra_MultiVector &globalCoefficients);
  IndexType localDofCount(); // local to the MPI node
    
  IndexType partitionLocalCellIndex(GlobalIndexType cellID);
  GlobalIndexType globalCellIndex(GlobalIndexType cellID);
  
  PartitionIndexType partitionForGlobalDofIndex( GlobalIndexType globalDofIndex );
  GlobalIndexType partitionLocalIndexForGlobalDofIndex( GlobalIndexType globalDofIndex );
  
  set<GlobalIndexType> partitionOwnedGlobalFieldIndices();
  set<GlobalIndexType> partitionOwnedGlobalFluxIndices();
  set<GlobalIndexType> partitionOwnedGlobalTraceIndices();
  set<GlobalIndexType> partitionOwnedIndicesForVariables(set<int> varIDs);
  
  FieldContainer<double> & physicalCellNodes( ElementTypePtr elemTypePtr );
  FieldContainer<double> & physicalCellNodesGlobal( ElementTypePtr elemTypePtr );
  
  void rebuildLookups();
  
  // used in some tests:
  int cellPolyOrder(GlobalIndexType cellID);
  void setElementType(GlobalIndexType cellID, ElementTypePtr newType, bool sideUpgradeOnly);
  const map< pair<GlobalIndexType,IndexType>, GlobalIndexType>& getLocalToGlobalMap() { // pair<cellID, localDofIndex> --> globalDofIndex
    return _localToGlobalMap;
  }
  
  // used by MultiBasis:
  static int neighborChildPermutation(int childIndex, int numChildrenInSide);
  static IndexType neighborDofPermutation(IndexType dofIndex, IndexType numDofsForSide);
};

#endif /* defined(__Camellia_debug__GDAMaximumRule2D__) */
