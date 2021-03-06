//
//  GDAMinimumRule.h
//  Camellia-debug
//
//  Created by Nate Roberts on 1/21/14.
//
//

#ifndef __Camellia_debug__GDAMinimumRule__
#define __Camellia_debug__GDAMinimumRule__

#include <iostream>

#include "GlobalDofAssignment.h"

#include "LocalDofMapper.h"

#include "BasisReconciliation.h"

#include "GDAMinimumRuleConstraints.h"

struct OwnershipInfo {
  GlobalIndexType cellID;
  GlobalIndexType owningSubcellEntityIndex;
  unsigned dimension;
};

struct CellConstraints {
  vector< vector< AnnotatedEntity > > subcellConstraints; // outer: subcell dim, inner: subcell ordinal in cell
  vector< vector< OwnershipInfo > > owningCellIDForSubcell; // outer vector indexed by subcell dimension; inner vector indexed by subcell ordinal in cell.  Pairs are (CellID, subcellIndex in MeshTopology)
};

class GDAMinimumRule : public GlobalDofAssignment {
  BasisReconciliation _br;
  map<GlobalIndexType, IndexType> _cellDofOffsets; // (cellID -> first partition-local dof index for that cell)  within the partition, offsets for the owned dofs in cell
  map<GlobalIndexType, GlobalIndexType> _globalCellDofOffsets; // (cellID -> first global dof index for that cell)
  GlobalIndexType _partitionDofOffset; // add to partition-local dof indices to get a global dof index
  GlobalIndexType _partitionDofCount; // how many dofs belong to the local partition
  FieldContainer<IndexType> _partitionDofCounts; // how many dofs belong to each MPI rank.
  GlobalIndexType _globalDofCount;
  
  set<IndexType> _partitionFluxIndexOffsets;
  set<IndexType> _partitionTraceIndexOffsets; // field indices are the complement of the other two
  
  map<int,set<IndexType> > _partitionIndexOffsetsForVarID; // TODO: factor out _partitionFluxIndexOffsets and _partitionTraceIndexOffsets using this container.
  
  typedef map<int, vector<GlobalIndexType> > VarIDToDofIndices; // key: varID
  typedef map<unsigned, VarIDToDofIndices> SubCellOrdinalToMap; // key: subcell ordinal
  typedef vector< SubCellOrdinalToMap > SubCellDofIndexInfo; // index to vector: subcell dimension

  map< GlobalIndexType, CellConstraints > _constraintsCache;
  map< GlobalIndexType, LocalDofMapperPtr > _dofMapperCache;
  map< GlobalIndexType, map<int, map<int, LocalDofMapperPtr> > > _dofMapperForVariableOnSideCache; // cellID --> side --> variable --> LocalDofMapper
  map< GlobalIndexType, SubCellDofIndexInfo> _ownedGlobalDofIndicesCache; // (cellID --> SubCellDofIndexInfo)
  
  vector<unsigned> allBasisDofOrdinalsVector(int basisCardinality);
  
  void filterSubBasisConstraintData(set<unsigned> &basisDofOrdinals,vector<GlobalIndexType> &globalDofOrdinals,
                                    FieldContainer<double> &constraintMatrixSideInterior, FieldContainer<bool> &processedDofs,
                                    DofOrderingPtr trialOrdering, VarPtr var, int sideOrdinal = 0);
  
  typedef vector< SubBasisDofMapperPtr > BasisMap;
  BasisMap getBasisMap(GlobalIndexType cellID, SubCellDofIndexInfo& dofOwnershipInfo, VarPtr var);
  BasisMap getBasisMapOld(GlobalIndexType cellID, SubCellDofIndexInfo& dofOwnershipInfo, VarPtr var, int sideOrdinal);

  BasisMap getBasisMap(GlobalIndexType cellID, SubCellDofIndexInfo& dofOwnershipInfo, VarPtr var, int sideOrdinal);
  
  SubCellDofIndexInfo getOwnedGlobalDofIndices(GlobalIndexType cellID, CellConstraints &cellConstraints);
  SubCellDofIndexInfo getGlobalDofIndices(GlobalIndexType cellID, CellConstraints &cellConstraints);
  
  set<GlobalIndexType> getFittableGlobalDofIndices(GlobalIndexType cellID, CellConstraints &constraints, int sideOrdinal); // returns the global dof indices for basis functions which have support on the given side (i.e. their support intersected with the side has positive measure).  This is determined by taking the union of the global dof indices defined on all the constraining sides for the given side (the constraining sides are by definition unconstrained).
  
  int H1Order(GlobalIndexType cellID, unsigned sideOrdinal); // this is meant to track the cell's interior idea of what the H^1 order is along that side.  We're isotropic for now, but eventually we might want to allow anisotropy in p...
  
  RefinementBranch volumeRefinementsForSideEntity(IndexType sideEntityIndex);
  
public:
  // these are public just for easier testing:
  CellConstraints getCellConstraints(GlobalIndexType cellID);
  LocalDofMapperPtr getDofMapper(GlobalIndexType cellID, CellConstraints &constraints, int varIDToMap = -1, int sideOrdinalToMap = -1);
  
  set<GlobalIndexType> getGlobalDofIndicesForIntegralContribution(GlobalIndexType cellID, int sideOrdinal); // assuming an integral is being done over the whole mesh skeleton, returns either an empty set or the global dof indices associated with the given side, depending on whether the cell "owns" the side for the purpose of such contributions.
public:
  GDAMinimumRule(MeshPtr mesh, VarFactory varFactory, DofOrderingFactoryPtr dofOrderingFactory, MeshPartitionPolicyPtr partitionPolicy,
                 unsigned initialH1OrderTrial, unsigned testOrderEnhancement);
  
  GlobalDofAssignmentPtr deepCopy();
  
  void didHRefine(const set<GlobalIndexType> &parentCellIDs);
  void didPRefine(const set<GlobalIndexType> &cellIDs, int deltaP);
  void didHUnrefine(const set<GlobalIndexType> &parentCellIDs);
  
  void didChangePartitionPolicy();
  
  ElementTypePtr elementType(GlobalIndexType cellID);
  
  int getH1Order(GlobalIndexType cellID);
  GlobalIndexType globalDofCount();
  set<GlobalIndexType> globalDofIndicesForCell(GlobalIndexType cellID);
  set<GlobalIndexType> globalDofIndicesForPartition(PartitionIndexType partitionNumber);
  
  set<GlobalIndexType> ownedGlobalDofIndicesForCell(GlobalIndexType cellID);
  
  set<GlobalIndexType> partitionOwnedGlobalFieldIndices();
  set<GlobalIndexType> partitionOwnedGlobalFluxIndices();
  set<GlobalIndexType> partitionOwnedGlobalTraceIndices();
  set<GlobalIndexType> partitionOwnedIndicesForVariables(set<int> varIDs);
  
  void interpretLocalData(GlobalIndexType cellID, const FieldContainer<double> &localData, FieldContainer<double> &globalData,
                          FieldContainer<GlobalIndexType> &globalDofIndices);
  void interpretLocalBasisCoefficients(GlobalIndexType cellID, int varID, int sideOrdinal, const FieldContainer<double> &basisCoefficients,
                               FieldContainer<double> &globalCoefficients, FieldContainer<GlobalIndexType> &globalDofIndices);
  void interpretGlobalCoefficients(GlobalIndexType cellID, FieldContainer<double> &localCoefficients, const Epetra_MultiVector &globalCoefficients);
  IndexType localDofCount(); // local to the MPI node

  PartitionIndexType partitionForGlobalDofIndex( GlobalIndexType globalDofIndex );
  void printConstraintInfo(GlobalIndexType cellID);
  void printGlobalDofInfo();
  void rebuildLookups();
};

#endif /* defined(__Camellia_debug__GDAMinimumRule__) */
