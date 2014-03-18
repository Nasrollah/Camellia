//
//  GDAMinimumRule.cpp
//  Camellia-debug
//
//  Created by Nate Roberts on 1/21/14.
//
//

#include "GDAMinimumRule.h"

#include "MPIWrapper.h"

#include "CamelliaCellTools.h"

#include "SerialDenseWrapper.h"

#include "Teuchos_GlobalMPISession.hpp"

#include "CamelliaDebugUtility.h"

GDAMinimumRule::GDAMinimumRule(MeshTopologyPtr meshTopology, VarFactory varFactory, DofOrderingFactoryPtr dofOrderingFactory, MeshPartitionPolicyPtr partitionPolicy,
                               unsigned initialH1OrderTrial, unsigned testOrderEnhancement)
: GlobalDofAssignment(meshTopology,varFactory,dofOrderingFactory,partitionPolicy, initialH1OrderTrial, testOrderEnhancement, false)
{
  rebuildLookups();
}

vector< map< unsigned, unsigned > > GDAMinimumRule::buildSubsideMap(shards::CellTopology &sideTopo) {
  unsigned sideDim = sideTopo.getDimension();
  unsigned subsideDim = sideDim - 1;

  int subsideCount = (subsideDim > 0) ? sideTopo.getSideCount(): sideTopo.getVertexCount();
  vector< map< unsigned, unsigned > > subsideMap(sideDim); // outer vector indexed by dimension.  map goes from scOrdinalInSide to a subside containing that subcell.  (This is not uniquely defined, but that should be OK.)
  for (int ssOrdinal=0; ssOrdinal<subsideCount; ssOrdinal++) {
    for (int d=0; d<sideDim-1; d++) {
      shards::CellTopology subside = sideTopo.getCellTopologyData(sideDim-1, ssOrdinal);
      unsigned scCount = subside.getSubcellCount(d);
      for (int scOrdinalInSubside=0; scOrdinalInSubside<scCount; scOrdinalInSubside++) {
        unsigned scOrdinalInSide = CamelliaCellTools::subcellOrdinalMap(sideTopo, sideDim-1, ssOrdinal, d, scOrdinalInSubside);
        subsideMap[d][scOrdinalInSide] = ssOrdinal;
//        cout << "subsideMap[" << d << "][" << scOrdinalInSide << "] = " << ssOrdinal << endl;
      }
    }
    // finally, enter the subside itself:
    subsideMap[sideDim-1][ssOrdinal] = ssOrdinal;
//    cout << "subsideMap[" << sideDim-1 << "][" << ssOrdinal << "] = " << ssOrdinal << endl;
  }
  return subsideMap;
}

void GDAMinimumRule::didChangePartitionPolicy() {
  rebuildLookups();
}

void GDAMinimumRule::didHRefine(const set<GlobalIndexType> &parentCellIDs) {
  this->GlobalDofAssignment::didHRefine(parentCellIDs);
  set<GlobalIndexType> neighborsOfNewElements;
  for (set<GlobalIndexType>::const_iterator cellIDIt = parentCellIDs.begin(); cellIDIt != parentCellIDs.end(); cellIDIt++) {
    GlobalIndexType parentCellID = *cellIDIt;
//    cout << "GDAMinimumRule: h-refining " << parentCellID << endl;
    CellPtr parentCell = _meshTopology->getCell(parentCellID);
    vector<IndexType> childIDs = parentCell->getChildIndices();
    int parentH1Order = _cellH1Orders[parentCellID];
    for (vector<IndexType>::iterator childIDIt = childIDs.begin(); childIDIt != childIDs.end(); childIDIt++) {
      GlobalIndexType childCellID = *childIDIt;
      _cellH1Orders[childCellID] = parentH1Order;
      assignInitialElementType(childCellID);
      assignParities(childCellID);
      // determine neighbors, so their parities can be updated below:
      CellPtr childCell = _meshTopology->getCell(childCellID);
      unsigned childSideCount = childCell->topology()->getSideCount();
      for (int childSideOrdinal=0; childSideOrdinal<childSideCount; childSideOrdinal++) {
        GlobalIndexType neighborCellID = childCell->getNeighbor(childSideOrdinal).first;
        if (neighborCellID != -1) {
          neighborsOfNewElements.insert(neighborCellID);
        }
      }
    }
  }
  // this set is not as lean as it might be -- we could restrict to peer neighbors, I think -- but it's a pretty cheap operation.
  for (set<GlobalIndexType>::iterator cellIDIt = neighborsOfNewElements.begin(); cellIDIt != neighborsOfNewElements.end(); cellIDIt++) {
    assignParities(*cellIDIt);
  }
  rebuildLookups();
}

void GDAMinimumRule::didPRefine(const set<GlobalIndexType> &cellIDs, int deltaP) {
  this->GlobalDofAssignment::didPRefine(cellIDs, deltaP);
  for (set<GlobalIndexType>::const_iterator cellIDIt = cellIDs.begin(); cellIDIt != cellIDs.end(); cellIDIt++) {
    assignInitialElementType(*cellIDIt);
  }
  rebuildLookups();
}

void GDAMinimumRule::didHUnrefine(const set<GlobalIndexType> &parentCellIDs) {
  this->GlobalDofAssignment::didHUnrefine(parentCellIDs);
  // TODO: implement this
  cout << "WARNING: GDAMinimumRule::didHUnrefine() unimplemented.\n";
  // will need to treat cell side parities here--probably suffices to redo those in parentCellIDs plus all their neighbors.
  rebuildLookups();
}

ElementTypePtr GDAMinimumRule::elementType(GlobalIndexType cellID) {
  return _elementTypeForCell[cellID];
}

int GDAMinimumRule::getH1Order(GlobalIndexType cellID) {
  return _cellH1Orders[cellID];
}

GlobalIndexType GDAMinimumRule::globalDofCount() {
  // assumes the lookups have been rebuilt since the last change that would affect the count
  
  // TODO: Consider working out a way to guard against a stale value here.  E.g. could have a "dirty" flag that gets set anytime there's a change to the refinements, and cleared when lookups are rebuilt.  If we're dirty when we get here, we rebuild before returning the global dof count.
  return _globalDofCount;
}

set<GlobalIndexType> GDAMinimumRule::globalDofIndicesForPartition(PartitionIndexType partitionNumber) {
  set<GlobalIndexType> globalDofIndices;
  // by construction, our globalDofIndices are contiguously numbered, starting with _partitionDofOffset
  for (GlobalIndexType i=0; i<_partitionDofCount; i++) {
    globalDofIndices.insert(_partitionDofOffset + i);
  }
  
  return globalDofIndices;
}

int GDAMinimumRule::H1Order(GlobalIndexType cellID, unsigned sideOrdinal) {
  // this is meant to track the cell's interior idea of what the H^1 order is along that side.  We're isotropic for now, but eventually we might want to allow anisotropy in p...
  return _cellH1Orders[cellID];
}

void GDAMinimumRule::interpretGlobalData(GlobalIndexType cellID, FieldContainer<double> &localDofs, const Epetra_Vector &globalData, bool accumulate) {
  CellConstraints constraints = getCellConstraints(cellID);
  LocalDofMapperPtr dofMapper = getDofMapper(cellID, constraints);
  vector<GlobalIndexType> globalIndexVector = dofMapper->globalIndices();
  
  FieldContainer<double> globalDataFC(globalIndexVector.size());
  for (int i=0; i<globalIndexVector.size(); i++) {
    GlobalIndexType globalIndex = globalIndexVector[i];
    globalDataFC[i] = globalData[globalIndex];
  }
  
  bool localToGlobal = false; // false: map "backwards" (global to local)
  localDofs = dofMapper->mapData(globalDataFC,localToGlobal,accumulate);
//  cout << "For cellID " << cellID << ", mapping globalData:\n " << globalDataFC;
//  cout << " to localData:\n " << localDofs;
}

void GDAMinimumRule::interpretLocalData(GlobalIndexType cellID, const FieldContainer<double> &localData,
                                        FieldContainer<double> &globalData, FieldContainer<GlobalIndexType> &globalDofIndices, bool accumulate) {
  CellConstraints constraints = getCellConstraints(cellID);
  LocalDofMapperPtr dofMapper = getDofMapper(cellID, constraints);
  
  bool localToGlobal = true;
  globalData = dofMapper->mapData(localData,localToGlobal,accumulate);
  vector<GlobalIndexType> globalIndexVector = dofMapper->globalIndices();
  globalDofIndices.resize(globalIndexVector.size());
  for (int i=0; i<globalIndexVector.size(); i++) {
    globalDofIndices(i) = globalIndexVector[i];
  }
  
//  cout << "localData:\n" << localData;
//  cout << "globalData:\n" << globalData;
//  cout << "globalIndices:\n" << globalDofIndices;
}

void GDAMinimumRule::interpretLocalBasisData(GlobalIndexType cellID, int varID, int sideOrdinal, const FieldContainer<double> &basisDofs,
                             FieldContainer<double> &globalData, FieldContainer<GlobalIndexType> &globalDofIndices) {
  CellConstraints constraints = getCellConstraints(cellID);
  LocalDofMapperPtr dofMapper = getDofMapper(cellID, constraints, varID, sideOrdinal);
  
  bool localToGlobal = true;
  bool accumulate = false; // not really relevant for this variant
  globalData = dofMapper->mapData(basisDofs,localToGlobal,accumulate);
  vector<GlobalIndexType> globalIndexVector = dofMapper->globalIndices();
  globalDofIndices.resize(globalIndexVector.size());
  for (int i=0; i<globalIndexVector.size(); i++) {
    globalDofIndices(i) = globalIndexVector[i];
  }
}

IndexType GDAMinimumRule::localDofCount() {
  // TODO: implement this
  cout << "WARNING: localDofCount() unimplemented.\n";
  return 0;
}

CellConstraints GDAMinimumRule::getCellConstraints(GlobalIndexType cellID) {
  if (_constraintsCache.find(cellID) != _constraintsCache.end()) {
    return _constraintsCache[cellID];
  }
  CellConstraints cellConstraints;
  CellPtr cell = _meshTopology->getCell(cellID);
  CellTopoPtr topo = cell->topology();
  unsigned sideCount = topo->getSideCount();
  unsigned spaceDim = topo->getDimension();
  unsigned sideDim = spaceDim - 1;
  unsigned subsideDim = sideDim - 1;
  vector<IndexType> sideEntityIndices = cell->getEntityIndices(sideDim);
  
  cellConstraints.sideConstraints = vector< ConstrainingCellInfo >(sideCount);
  cellConstraints.subsideConstraints = vector< vector<ConstrainingSubsideInfo> >(sideCount);
  ConstrainingCellInfo sideConstraint;
  ConstrainingSubsideInfo subsideConstraint;
  
  typedef pair< IndexType, unsigned > CellPair;
  
  // determine subcell ownership from the perspective of the cell (for now, we redundantly store it from the perspective of side as well, below)
  cellConstraints.owningCellIDForSubcell = vector< vector< pair<GlobalIndexType,GlobalIndexType> > >(sideDim+1);
  for (int d=0; d<spaceDim; d++) {
    vector<IndexType> scIndices = cell->getEntityIndices(d);
    unsigned scCount = scIndices.size();
    cellConstraints.owningCellIDForSubcell[d] = vector< pair<GlobalIndexType,GlobalIndexType> >(scCount);
    for (int scOrdinal = 0; scOrdinal < scCount; scOrdinal++) {
      IndexType entityIndex = scIndices[scOrdinal];
      IndexType constrainingEntityIndex = _meshTopology->getConstrainingEntityIndex(d, entityIndex);
      pair<GlobalIndexType,GlobalIndexType> owningCellInfo = _meshTopology->leastActiveCellIndexContainingEntityConstrainedByConstrainingEntity(d, constrainingEntityIndex);
      cellConstraints.owningCellIDForSubcell[d][scOrdinal] = owningCellInfo;
    }
  }
  
  for (unsigned sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
    IndexType sideEntityIndex = sideEntityIndices[sideOrdinal];
    
    // determine subcell ownership from the perspective of the side (for now, we redundantly store it from the perspective of cell as well, above)
    sideConstraint.owningCellIDForSideSubcell = vector< vector< pair<GlobalIndexType,GlobalIndexType> > >(subsideDim + 1);
    for (int d=0; d<sideDim; d++) {
      unsigned scCount = _meshTopology->getSubEntityCount(sideDim, sideEntityIndex, d);
      sideConstraint.owningCellIDForSideSubcell[d] = vector< pair<GlobalIndexType,GlobalIndexType> >(scCount);
      for (int scOrdinal = 0; scOrdinal < scCount; scOrdinal++) {
        IndexType entityIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, d, scOrdinal);
        IndexType constrainingEntityIndex = _meshTopology->getConstrainingEntityIndex(d, entityIndex);
        pair<GlobalIndexType,GlobalIndexType> owningCellInfo = _meshTopology->leastActiveCellIndexContainingEntityConstrainedByConstrainingEntity(d, constrainingEntityIndex);
        sideConstraint.owningCellIDForSideSubcell[d][scOrdinal] = owningCellInfo;
      }
    }
    
    // first, establish side constraint
    IndexType constrainingSideEntityIndex = _meshTopology->getConstrainingEntityIndex(sideDim, sideEntityIndex);
    if (sideEntityIndex == constrainingSideEntityIndex) {
      if (_meshTopology->getCellCountForSide(sideEntityIndex) == 1) { // then we know that this cell is the constraining one on the side
        sideConstraint.cellID = cellID;
        sideConstraint.sideOrdinal = sideOrdinal;
      } else { // there are two cells; we need to figure out which has the lower H1 order
        CellPair cell1 = _meshTopology->getFirstCellForSide(sideEntityIndex);
        CellPair cell2 = _meshTopology->getSecondCellForSide(sideEntityIndex);
        unsigned cell1Order = _cellH1Orders[cell1.first];
        unsigned cell2Order = _cellH1Orders[cell1.first];
        
        int whichCell;
        if (cell1Order < cell2Order) {
          whichCell = 1;
        } else if (cell2Order < cell1Order) {
          whichCell = 2;
        } else { // tie -- broken by cellID (lower one is the constraint)
          if (cell1.first < cell2.first) {
            whichCell = 1;
          } else {
            whichCell = 2;
          }
        }
        if (whichCell==1) {
          sideConstraint.cellID = cell1.first;
          sideConstraint.sideOrdinal = cell1.second;
        } else if (whichCell==2) {
          sideConstraint.cellID = cell2.first;
          sideConstraint.sideOrdinal = cell2.second;
        }
      }
    } else {
      // there should be exactly one cell with the specified sideEntityIndex listed in _meshTopology::_cellsForSideEntities.
      if (_meshTopology->getCellCountForSide(sideEntityIndex) != 1) {
        cout << "ERROR while applying GDAMinimumRule: extra cell(s) defined for sideEntityIndex.\n";
      
        cout << "sideEntityIndex " << sideEntityIndex << ", vertices:\n";
        _meshTopology->printEntityVertices(sideDim, sideEntityIndex);
        cout << "constrainingSideEntityIndex " << constrainingSideEntityIndex << ", vertices:\n";
        _meshTopology->printEntityVertices(sideDim, constrainingSideEntityIndex);
        
        CellPair firstCellPair = _meshTopology->getFirstCellForSide(sideEntityIndex);
        CellPair secondCellPair = _meshTopology->getSecondCellForSide(sideEntityIndex);
        
        cout << "first  cellID: " << firstCellPair.first << ", sideOrdinal " << firstCellPair.second << endl;
        cout << "second cellID: " << secondCellPair.first << ", sideOrdinal " << secondCellPair.second << endl;
        
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "ERROR: extra cell(s) defined for sideEntityIndex.");
      }
      CellPair sideCellInfo = _meshTopology->getFirstCellForSide(sideEntityIndex);
      pair<GlobalIndexType, unsigned> neighborInfo = _meshTopology->getCell(sideCellInfo.first)->getNeighbor(sideCellInfo.second);
      
      sideConstraint.cellID = neighborInfo.first;
      sideConstraint.sideOrdinal = neighborInfo.second;
    }
    cellConstraints.sideConstraints[sideOrdinal] = sideConstraint;
    
    // next, establish subside constraints
    shards::CellTopology sideTopo = topo->getCellTopologyData(sideDim, sideOrdinal);
    unsigned subsideCount = (spaceDim != 2) ? sideTopo.getSideCount() : sideTopo.getVertexCount(); // we do want to treat the vertices as subside topologies in 2D
    cellConstraints.subsideConstraints[sideOrdinal] = vector<ConstrainingSubsideInfo>(subsideCount);
    for (unsigned subsideOrdinal = 0; subsideOrdinal < subsideCount; subsideOrdinal++) {
      // determine the subside's entity index
      GlobalIndexType subsideEntityIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, subsideDim, subsideOrdinal);
      
      // determine the constraining entity index
      unsigned constrainingEntityIndex = _meshTopology->getConstrainingEntityIndex(subsideDim, subsideEntityIndex);
      // determine all sides that contain the constraining entity -- find the cell containing such a side with the lowest H^1 order (using cellID to break ties)
      set< CellPair > cellsForSubside = _meshTopology->getCellsContainingEntity(subsideDim, constrainingEntityIndex);
      
      if (subsideDim == 0) { // vertices: we should restrict ourselves to active cells
        set< CellPair > cells = cellsForSubside;
        cellsForSubside.clear();
        set< GlobalIndexType > activeCellIDs = _meshTopology->getActiveCellIndices();
        for (set< CellPair >::iterator cellIt = cells.begin(); cellIt != cells.end(); cellIt++) {
          GlobalIndexType cellID = cellIt->first;
          if (activeCellIDs.find(cellID) != activeCellIDs.end()) {
            cellsForSubside.insert(*cellIt);
          }
        }
      }
      
      unsigned leastH1Order = (unsigned)-1;
      set< CellPair > cellsWithLeastH1Order;
      for (set< CellPair >::iterator subsideCellIt = cellsForSubside.begin(); subsideCellIt != cellsForSubside.end(); subsideCellIt++) {
        IndexType subsideCellID = subsideCellIt->first;
        if (_cellH1Orders[subsideCellID] == leastH1Order) {
          cellsWithLeastH1Order.insert(*subsideCellIt);
        } else if (_cellH1Orders[subsideCellID] < leastH1Order) {
          cellsWithLeastH1Order.clear();
          leastH1Order = _cellH1Orders[subsideCellID];
          cellsWithLeastH1Order.insert(*subsideCellIt);
        }
//        // DEBUGGING/TESTING:
//        {
//          CellPtr cell = _meshTopology->getCell(subsideCellID);
//          unsigned scord = cell->findSubcellOrdinal(subsideDim, constrainingEntityIndex);
//          unsigned sideOrdinal = subsideCellIt->second;
//          unsigned scordinalInSide = CamelliaCellTools::subcellReverseOrdinalMap(*topo, sideDim, sideOrdinal, subsideDim, scord);
//          cout << "scordinalInSide: " << scordinalInSide << endl;
//        }
      }
      if (cellsWithLeastH1Order.size() == 0) {
        cout << "ERROR: No cells found for constraining subside entity.\n";
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "No cells found for constraining subside entity.");
      }
      CellPair constrainingCellPair = *cellsWithLeastH1Order.begin(); // first one will have the earliest cell ID, given the sorting of set/pair.
      subsideConstraint.cellID = constrainingCellPair.first;
      subsideConstraint.sideOrdinal = constrainingCellPair.second;
      
//      {
//        cout << "For cellID " << cellID << ", sideOrdinal " << sideOrdinal << ", subside " << subsideOrdinal << ", constraint:\n";
//        cout << "subsideConstraint.cellID      = " << subsideConstraint.cellID << endl;
//        cout << "subsideConstraint.sideOrdinal = " << subsideConstraint.sideOrdinal << endl;
//        CellPtr cell = _meshTopology->getCell(subsideConstraint.cellID);
//        unsigned scord = cell->findSubcellOrdinal(subsideDim, constrainingEntityIndex);
//        unsigned scordinalInSide = CamelliaCellTools::subcellReverseOrdinalMap(*topo, sideDim, subsideConstraint.sideOrdinal, subsideDim, scord);
//        cout << "constraining entity index     = " << constrainingEntityIndex << endl;
//        cout << "scord in cell                 = " << scord << endl;
//        cout << "scord in side                 = " << scordinalInSide << endl;
//      }
      
      // now, we need to find the subcellOrdinal within cell for the subside:
      CellPtr constrainingCell = _meshTopology->getCell(subsideConstraint.cellID);
      
      shards::CellTopology constrainingSide = constrainingCell->topology()->getCellTopologyData(sideDim, subsideConstraint.sideOrdinal);
      int subsideCountInConstrainingSide = (subsideDim > 0) ? constrainingSide.getSubcellCount(subsideDim) : constrainingSide.getVertexCount();
      constrainingSideEntityIndex = constrainingCell->entityIndex(sideDim, subsideConstraint.sideOrdinal);
      subsideConstraint.subsideOrdinalInSide = -1;
      unsigned constrainingSideOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(sideDim, constrainingSideEntityIndex);
      for (int subsideOrdinalInConstrainingSide = 0; subsideOrdinalInConstrainingSide < subsideCountInConstrainingSide; subsideOrdinalInConstrainingSide++) {
        CellTopoPtr constrainingTopo = constrainingCell->topology();
        unsigned subsideOrdinalInConstrainingCell = CamelliaCellTools::subcellOrdinalMap(*(constrainingTopo.get()), sideDim, constrainingSideOrdinalInConstrainingCell, subsideDim, subsideOrdinalInConstrainingSide);
        IndexType entityIndex = constrainingCell->entityIndex(subsideDim, subsideOrdinalInConstrainingCell);
        // old version, which stored the ordinal relative to the canonical view of the constraining side (as opposed to the constraining cell's view of it):
//        IndexType entityIndex =  _meshTopology->getSubEntityIndex(sideDim, constrainingSideEntityIndex, subsideDim, subsideOrdinalInConstrainingSide);
        if (entityIndex == constrainingEntityIndex) {
          subsideConstraint.subsideOrdinalInSide = subsideOrdinalInConstrainingSide;
        }
      }
      if (subsideConstraint.subsideOrdinalInSide == -1) {
        cout << "ERROR: Could not determine subside ordinal in side for subside constraint.\n";
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Could not determine subside ordinal in side for subside constraint.");
      }
//      cout << "Setting subsideConstraint for cell " << cellID << ", side " << sideOrdinal << ", subside " << subsideOrdinal << ":\n";
//      cout << "sideOrdinal: " << subsideConstraint.sideOrdinal << endl;
//      cout << "subsideOrdinalInSide: " << subsideConstraint.subsideOrdinalInSide << endl;
      cellConstraints.subsideConstraints[sideOrdinal][subsideOrdinal] = subsideConstraint;
    }
  }
  _constraintsCache[cellID] = cellConstraints;
  
  return cellConstraints;
}

typedef map<int, vector<GlobalIndexType> > VarIDToDofIndices; // key: varID
typedef map<unsigned, VarIDToDofIndices> SubCellOrdinalToMap; // key: subcell ordinal
typedef vector< SubCellOrdinalToMap > SubCellDofIndexInfo; // index to vector: subcell dimension

SubCellDofIndexInfo GDAMinimumRule::getOwnedGlobalDofIndices(GlobalIndexType cellID, CellConstraints &constraints) {
  int spaceDim = _meshTopology->getSpaceDim();
  int sideDim = spaceDim - 1;
  
  SubCellDofIndexInfo scInfo(spaceDim+1);
  
  CellTopoPtr topo = _elementTypeForCell[cellID]->cellTopoPtr;
  
  int sideCount = topo->getSideCount();
  
  typedef vector< SubBasisDofMapperPtr > BasisMap;
  
  DofOrderingPtr trialOrdering = _elementTypeForCell[cellID]->trialOrderPtr;
  map<int, VarPtr> trialVars = _varFactory.trialVars();
  
  GlobalIndexType globalDofIndex = _globalCellDofOffsets[cellID]; // our first globalDofIndex
  
  for (map<int, VarPtr>::iterator varIt = trialVars.begin(); varIt != trialVars.end(); varIt++) {
    VarPtr var = varIt->second;
    bool isL2Var = (var->space() == L2) || (var->space() == VECTOR_L2);
    bool varHasSupportOnVolume = (var->varType() == FIELD) || (var->varType() == TEST);
    if ( varHasSupportOnVolume ) {
      BasisPtr basis = trialOrdering->getBasis(var->ID());
      set<unsigned> basisDofOrdinals;
      vector<GlobalIndexType> globalDofOrdinals;
      if (isL2Var) { // unconstrained / local
        int cardinality = basis->getCardinality();
        for (int i=0; i<cardinality; i++) {
          basisDofOrdinals.insert(i);
        }
      } else {
        set<int> ordinalsInt = BasisReconciliation::interiorDofOrdinalsForBasis( basis ); // TODO: change dofOrdinalsForInterior to return set<unsigned>...
        basisDofOrdinals.insert(ordinalsInt.begin(),ordinalsInt.end());
      }
      int count = basisDofOrdinals.size();
      for (int i=0; i<count; i++) {
        globalDofOrdinals.push_back(globalDofIndex++);
      }
      unsigned scdim = spaceDim; // volume
      unsigned scord = 0; // only one volume
      scInfo[scdim][scord][var->ID()] = globalDofOrdinals;
    }
    if ( ! (varHasSupportOnVolume && isL2Var) ) { // if space is L^2 on the volume, we'll have claimed all the dofs above, and we can skip further processing
      vector< set<unsigned> > processedSubcells(sideDim);
      for (int sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
        BasisMap sideBasisMap;
        shards::CellTopology sideTopo = topo->getCellTopologyData(sideDim, sideOrdinal);
        GlobalIndexType owningCellIDForSide = constraints.owningCellIDForSubcell[sideDim][sideOrdinal].first;
        set<unsigned> basisDofOrdinals;
        vector<GlobalIndexType> globalDofOrdinals;
        
        ConstrainingCellInfo sideConstraint = constraints.sideConstraints[sideOrdinal];
        DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[sideConstraint.cellID]->trialOrderPtr;
        if (varHasSupportOnVolume) {
          BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID());
          set<int> sideBasisOrdinals = basis->dofOrdinalsForSubcell(sideDim, sideConstraint.sideOrdinal, sideDim);
          basisDofOrdinals.insert(sideBasisOrdinals.begin(),sideBasisOrdinals.end());
        } else {
          BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID(),sideConstraint.sideOrdinal);
          set<int> sideBasisOrdinals = BasisReconciliation::interiorDofOrdinalsForBasis(basis); // isL2Var ? basis->dofOrdinalsForSubcells(sideDim, true) : basis->dofOrdinalsForInterior(); // if L^2, all dofs are interior...
          basisDofOrdinals.insert(sideBasisOrdinals.begin(),sideBasisOrdinals.end());
        }
        if (owningCellIDForSide == cellID) { // then we're responsible for the assignment of global indices
          int count = basisDofOrdinals.size();
          for (int i=0; i<count; i++) {
            globalDofOrdinals.push_back(globalDofIndex++);
          }
          unsigned scdim = sideDim;
          scInfo[scdim][sideOrdinal][var->ID()] = globalDofOrdinals;
        }
        
        if (!isL2Var) {
          vector< map< unsigned, unsigned > > subsideMap = buildSubsideMap(sideTopo); // outer vector indexed by dimension.  map goes from scOrdinalInSide to a subside containing that subcell.  (This is not uniquely defined, but that should be OK.)
          
          for (int d=0; d<sideDim; d++) {
            unsigned scCount = sideTopo.getSubcellCount(d);
            for (unsigned scOrdinalInSide = 0; scOrdinalInSide < scCount; scOrdinalInSide++) {
              unsigned scOrdinalInCell = CamelliaCellTools::subcellOrdinalMap(*topo, sideDim, sideOrdinal, d, scOrdinalInSide);
              if (processedSubcells[d].find(scOrdinalInCell) == processedSubcells[d].end()) { // haven't processed this one yet
                GlobalIndexType owningCellID = constraints.owningCellIDForSubcell[d][scOrdinalInCell].first;
                
                if (owningCellID == cellID) {
                  CellPtr cell = _meshTopology->getCell(cellID);

                  // determine the subcell index in _meshTopology (will be used below to determine the subcell ordinal of the constraining subcell in the constraining cell)
                  
                  IndexType sideEntityIndex = cell->entityIndex(sideDim, sideOrdinal);
                  IndexType scIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, d, scOrdinalInSide);
                  IndexType constrainingScIndex = _meshTopology->getConstrainingEntityIndex(d, scIndex);

                  if (varHasSupportOnVolume) {
                    ConstrainingCellInfo sideConstraint = constraints.sideConstraints[sideOrdinal];
                    DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[sideConstraint.cellID]->trialOrderPtr;
                    BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID());
                    CellPtr constrainingCell = _meshTopology->getCell(sideConstraint.cellID);
                    
                    unsigned scOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(d,constrainingScIndex);
                    set<int> scBasisOrdinals = basis->dofOrdinalsForSubcell(d, scOrdinalInConstrainingCell, d);
                    
                    globalDofOrdinals.clear();
                    int count = basisDofOrdinals.size();
                    for (int i=0; i<count; i++) {
                      globalDofOrdinals.push_back(globalDofIndex++);
                    }
                    scInfo[d][scOrdinalInCell][var->ID()] = globalDofOrdinals;
                  } else {
                    // here, we are dealing with a subside or one of its constituents.  We want to use the constraint info for that subside.
                    unsigned subsideOrdinal = subsideMap[d][scOrdinalInSide];
                    ConstrainingSubsideInfo subsideConstraint = constraints.subsideConstraints[sideOrdinal][subsideOrdinal];
                    DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[subsideConstraint.cellID]->trialOrderPtr;
                    BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID(),subsideConstraint.sideOrdinal);
                    
                    CellPtr constrainingCell = _meshTopology->getCell(subsideConstraint.cellID);
                    unsigned scOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(d,constrainingScIndex);
                    unsigned scOrdinalInConstrainingSide = CamelliaCellTools::subcellReverseOrdinalMap(*constrainingCell->topology(),
                                                                                                       sideDim, subsideConstraint.sideOrdinal,
                                                                                                       d, scOrdinalInConstrainingCell);
                    
                    set<int> scBasisOrdinals = basis->dofOrdinalsForSubcell(d, scOrdinalInConstrainingSide, d);
                    globalDofOrdinals.clear();
                    int count = scBasisOrdinals.size();
                    for (int i=0; i<count; i++) {
                      globalDofOrdinals.push_back(globalDofIndex++);
                    }
                    scInfo[d][scOrdinalInCell][var->ID()] = globalDofOrdinals;
                  }
                }
                processedSubcells[d].insert(scOrdinalInCell);
              }
            }
          }
        }
      }
    }
  }
  return scInfo;
}

LocalDofMapperPtr GDAMinimumRule::getDofMapper(GlobalIndexType cellID, CellConstraints &constraints, int varIDToMap, int sideOrdinalToMap) {
  // assumes that the _globalCellDofOffsets are up to date
  int spaceDim = _meshTopology->getSpaceDim();
  int sideDim = spaceDim - 1;
  
  CellTopoPtr topo = _elementTypeForCell[cellID]->cellTopoPtr;
  
  CellPtr cell = _meshTopology->getCell(cellID);
  
  int sideCount = topo->getSideCount();
  
  typedef vector< SubBasisDofMapperPtr > BasisMap;
  
  SubCellDofIndexInfo dofIndexInfo = getOwnedGlobalDofIndices(cellID, constraints);
  
  map< GlobalIndexType, SubCellDofIndexInfo > otherDofIndexInfoCache; // local lookup, to avoid a bunch of redundant calls to getOwnedGlobalDofIndices
  
  for (int d=0; d<=spaceDim; d++) {
    int scCount = (d != spaceDim) ? topo->getSubcellCount(d) : 1; // TODO: figure out whether the spaceDim special case is needed...
    for (int scord=0; scord<scCount; scord++) {
      if (dofIndexInfo[d].find(scord) == dofIndexInfo[d].end()) { // this one not yet filled in
        pair<GlobalIndexType, GlobalIndexType> owningCellInfo = constraints.owningCellIDForSubcell[d][scord];
        GlobalIndexType owningCellID = owningCellInfo.first;
        CellConstraints owningConstraints = getCellConstraints(owningCellID);
        if (otherDofIndexInfoCache.find(owningCellID) == otherDofIndexInfoCache.end()) {
          otherDofIndexInfoCache[owningCellID] = getOwnedGlobalDofIndices(owningCellID, owningConstraints);
        }
        GlobalIndexType scEntityIndex = owningCellInfo.second;
        CellPtr owningCell = _meshTopology->getCell(owningCellID);
        unsigned owningCellScord = owningCell->findSubcellOrdinal(d, scEntityIndex);
        SubCellDofIndexInfo owningDofIndexInfo = otherDofIndexInfoCache[owningCellID];
        dofIndexInfo[d][scord] = owningDofIndexInfo[d][owningCellScord];
      }
    }
  }
  
  vector< map<unsigned, GlobalIndexType > > sideEntityForSubcell(spaceDim);
  
  set<GlobalIndexType> sideEntitiesOfInterest;
  
  for (int d=0; d<spaceDim; d++) {
    int scCount = topo->getSubcellCount(d);
    for (int scord=0; scord<scCount; scord++) {
      GlobalIndexType subcellEntityIndex = cell->entityIndex(d, scord);
      set<IndexType> sidesContainingEntity = _meshTopology->getSidesContainingEntity(d, subcellEntityIndex);
      // we're going to look for sides that contain this entity which have as an ancestor a side that contains the constraining entity
      
      GlobalIndexType constrainingEntityIndex = _meshTopology->getConstrainingEntityIndex(d, subcellEntityIndex);
      set<IndexType> sidesContainingConstrainingEntity = _meshTopology->getSidesContainingEntity(d, constrainingEntityIndex);
      
      for (set<IndexType>::iterator entitySideIt = sidesContainingEntity.begin(); entitySideIt != sidesContainingEntity.end(); entitySideIt++) {
        GlobalIndexType sideEntityIndex = *entitySideIt;
        GlobalIndexType constrainingSideEntityIndex = _meshTopology->getConstrainingEntityIndex(sideDim, sideEntityIndex);
        if (sidesContainingConstrainingEntity.find(constrainingSideEntityIndex) != sidesContainingConstrainingEntity.end()) {
          sideEntityForSubcell[d][scord] = sideEntityIndex;
          sideEntitiesOfInterest.insert(sideEntityIndex);
          
          shards::CellTopology constrainingSideTopology = _meshTopology->getEntityTopology(sideDim, constrainingSideEntityIndex);
          break;
        }
      }
    }
  }
  
  map<GlobalIndexType, RefinementBranch > volumeRefinementsForSideEntity;
  map<GlobalIndexType, RefinementBranch > sideRefinementsForSideEntity;
  
  map<GlobalIndexType, map<GlobalIndexType, unsigned> > subsideOrdinalInSideEntityOfInterest;  // outer map key: sideEntityIndex.  Inner map key: subsideEntityIndex.  Inner map value: subside ordinal of the specified subside in the specified side.
  
  for (set<GlobalIndexType>::iterator sideEntityIt = sideEntitiesOfInterest.begin(); sideEntityIt != sideEntitiesOfInterest.end(); sideEntityIt++ ) {
    GlobalIndexType sideEntityIndex = *sideEntityIt;
    pair<IndexType, unsigned> cellInfo = _meshTopology->getFirstCellForSide(sideEntityIndex); // there may be a second cell for this side, but if so, they're compatible neighbors, so that the RefinementBranch will be empty--i.e. if there is a second, we'd get the same result as we do using the first
    GlobalIndexType cellID = cellInfo.first;
    unsigned sideOrdinal = cellInfo.second;
    CellPtr cellForSide = _meshTopology->getCell(cellID);
    RefinementBranch refBranch = cellForSide->refinementBranchForSide(sideOrdinal);
    volumeRefinementsForSideEntity[sideEntityIndex] = refBranch;
    if (refBranch.size() == 0) { // then side refinements also empty
      sideRefinementsForSideEntity[sideEntityIndex] = refBranch;
    } else {
      pair<IndexType, unsigned> neighborInfo = cellForSide->getNeighbor(sideOrdinal);
      unsigned neighborSideOrdinal = neighborInfo.second;
      unsigned ancestorSideOrdinal = _meshTopology->getCell(neighborInfo.first)->getNeighbor(neighborSideOrdinal).second;
      sideRefinementsForSideEntity[sideEntityIndex] = RefinementPattern::sideRefinementBranch(refBranch, ancestorSideOrdinal);
    }
    
    shards::CellTopology sideTopo = _meshTopology->getEntityTopology(sideDim, sideEntityIndex);
    unsigned ssCount = sideTopo.getSubcellCount(sideDim-1);
    for (int subsideOrdinal=0; subsideOrdinal<ssCount; subsideOrdinal++) {
      GlobalIndexType subsideEntityIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, sideDim-1, subsideOrdinal);
      shards::CellTopology subsideTopo = _meshTopology->getEntityTopology(sideDim-1, subsideEntityIndex);
      subsideOrdinalInSideEntityOfInterest[sideEntityIndex][subsideEntityIndex] = subsideOrdinal;
    }
  }
  
  DofOrderingPtr trialOrdering = _elementTypeForCell[cellID]->trialOrderPtr;
  map<int, VarPtr> trialVars = _varFactory.trialVars();
  
  map< int, BasisMap > volumeMap;
  vector< map< int, BasisMap > > sideMaps(sideCount);
  
  for (map<int, VarPtr>::iterator varIt = trialVars.begin(); varIt != trialVars.end(); varIt++) {
    VarPtr var = varIt->second;
    bool isL2Var = (var->space() == L2) || (var->space() == VECTOR_L2);
    bool omitVarEntry = (varIDToMap != -1) && (var->ID() != varIDToMap); // don't skip processing, just omit it from the map
    BasisMap volumeBasisMap;
    bool varHasSupportOnVolume = (var->varType() == FIELD) || (var->varType() == TEST);
    if ( varHasSupportOnVolume ) {
      BasisPtr basis = trialOrdering->getBasis(var->ID());
      set<unsigned> basisDofOrdinals;
      
      if (isL2Var) { // unconstrained / local
        int cardinality = basis->getCardinality();
        for (int i=0; i<cardinality; i++) {
          basisDofOrdinals.insert(i);
        }
      } else {
        set<int> ordinalsInt = BasisReconciliation::interiorDofOrdinalsForBasis(basis); // basis->dofOrdinalsForInterior(); // TODO: change dofOrdinalsForInterior to return set<unsigned>...
        basisDofOrdinals.insert(ordinalsInt.begin(),ordinalsInt.end());
      }
      vector<GlobalIndexType> globalDofOrdinals = dofIndexInfo[spaceDim][0][var->ID()];
      if (!omitVarEntry) {
        if (basisDofOrdinals.size() > 0) {
//          cout << "getDofMapper: for var " << var->ID() << ", adding volume sub-basis dofMapper for " << basisDofOrdinals.size() << "  local dofOrdinals";
//          cout << ", mapping to " << globalDofOrdinals.size() << " global dof ordinals.\n";
//          cout << "( ";
//          for (set<unsigned>::iterator ordIt=basisDofOrdinals.begin(); ordIt != basisDofOrdinals.end(); ordIt++) {
//            cout << *ordIt << " ";
//          }
//          cout << ") ---> ( ";
//          for (vector<unsigned>::iterator ordIt=globalDofOrdinals.begin(); ordIt != globalDofOrdinals.end(); ordIt++) {
//            cout << *ordIt << " ";
//          }
//          cout << ")\n";
          
          volumeBasisMap.push_back(SubBasisDofMapper::subBasisDofMapper(basisDofOrdinals, globalDofOrdinals));
        }
      }
    }
    
    if ( ! (varHasSupportOnVolume && isL2Var) ) { // if space is L^2 on the volume, we'll have claimed all the dofs above, and we can skip further processing
//      vector< set<unsigned> > processedSubcells(sideDim);
      for (int sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
        bool omitSideEntry = (sideOrdinalToMap != -1) && (sideOrdinal != sideOrdinalToMap);
        BasisMap sideBasisMap;
        shards::CellTopology sideTopo = topo->getCellTopologyData(sideDim, sideOrdinal);
        set<unsigned> basisDofOrdinals;
        ConstrainingCellInfo sideConstraint = constraints.sideConstraints[sideOrdinal];
        CellPtr constrainingCellForSide = _meshTopology->getCell(sideConstraint.cellID);
        DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[sideConstraint.cellID]->trialOrderPtr;
        
        pair< GlobalIndexType, unsigned > ancestralCellInfo;
        
        bool constrainingCellIsAncestor = false;  // constraining cell will either be the ancestor or the neighbor of the ancestor of the current cell
        CellPtr currentCell = cell;
        while (currentCell.get() != NULL) {
          if (currentCell->cellIndex() == sideConstraint.cellID) {
            constrainingCellIsAncestor = true;
          }
          currentCell = currentCell->getParent();
        }
        if (constrainingCellIsAncestor) {
          ancestralCellInfo = make_pair(sideConstraint.cellID,sideConstraint.sideOrdinal);
        } else { // if the constraining cell is not the ancestor, then its neighbor is:
          ancestralCellInfo = constrainingCellForSide->getNeighbor(sideConstraint.sideOrdinal);
        }
        
        IndexType constrainingSideEntityIndex = constrainingCellForSide->entityIndex(sideDim, sideConstraint.sideOrdinal);
        shards::CellTopology constrainingSideTopo = _meshTopology->getEntityTopology(sideDim, constrainingSideEntityIndex);
        
        unsigned constrainingSidePermutationInverse, ancestralCellSidePermutation;
        
        if (ancestralCellInfo.first != -1) {
          CellPtr ancestralCell = _meshTopology->getCell(ancestralCellInfo.first);
          unsigned ancestralCellSideOrdinal = ancestralCellInfo.second;
          ancestralCellSidePermutation = ancestralCell->subcellPermutation(sideDim, ancestralCellSideOrdinal);
          unsigned constrainingSidePermutation = _meshTopology->getCell(sideConstraint.cellID)->subcellPermutation(sideDim, sideConstraint.sideOrdinal);
          constrainingSidePermutationInverse = CamelliaCellTools::permutationInverse(constrainingSideTopo, constrainingSidePermutation);
        } else {  // no neighbor
          constrainingSidePermutationInverse = 0;
          ancestralCellSidePermutation = 0;
        }
        
        unsigned composedPermutation = CamelliaCellTools::permutationComposition(constrainingSideTopo, constrainingSidePermutationInverse, ancestralCellSidePermutation);
        
        // DEBUGGING:
//        if (ancestralCellInfo.first != -1) {
//          cout << "on cell " << cellID << ", side " << sideOrdinal << ", composedPermutation = " << composedPermutation << endl;
//        }
        
        RefinementBranch refBranch;
        BasisPtr basis, constrainingBasis;
        
        vector<GlobalIndexType> globalDofOrdinals = dofIndexInfo[sideDim][sideOrdinal][var->ID()];
        
        SubBasisReconciliationWeights subBasisWeights;

        if (varHasSupportOnVolume) {
          constrainingBasis = constrainingTrialOrdering->getBasis(var->ID());
          basis = trialOrdering->getBasis(var->ID());
          refBranch = volumeRefinementsForSideEntity[sideEntityForSubcell[sideDim][sideOrdinal]];
          subBasisWeights = _br.constrainedWeights(basis, sideOrdinal, refBranch, constrainingBasis, sideConstraint.sideOrdinal, composedPermutation);
          
          FieldContainer<double> constraintMatrixSideInterior = BasisReconciliation::subBasisReconciliationWeightsForSubcell(subBasisWeights, sideDim, basis, sideOrdinal,
                                                                                                                             constrainingBasis, sideConstraint.sideOrdinal,
                                                                                                                             basisDofOrdinals);
          if (constraintMatrixSideInterior.size() == 0) {
            cout << "Error: empty constraint matrix encountered.\n";
            TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Error: empty constraint matrix encountered.\n");
          }
          
          if (basisDofOrdinals.size() > 0) {
            if (!omitSideEntry && !omitVarEntry) {
              volumeBasisMap.push_back(SubBasisDofMapper::subBasisDofMapper(basisDofOrdinals, globalDofOrdinals, constraintMatrixSideInterior));
            }
//            cout << "getDofMapper: for var " << var->ID() << " on side " << sideOrdinal << ", adding volume sub-basis dofMapper for " << basisDofOrdinals.size() << "  local dofOrdinals";
//            cout << ", mapping to " << globalDofOrdinals.size() << " global dof ordinals.\n";
//            cout << "( ";
//            for (set<unsigned>::iterator ordIt=basisDofOrdinals.begin(); ordIt != basisDofOrdinals.end(); ordIt++) {
//              cout << *ordIt << " ";
//            }
//            cout << ") ---> ( ";
//            for (vector<unsigned>::iterator ordIt=globalDofOrdinals.begin(); ordIt != globalDofOrdinals.end(); ordIt++) {
//              cout << *ordIt << " ";
//            }
//            cout << ")\n";
          }
        }
        else
        { // basis has support on mesh skeleton
          // first, we want to set up a map for the "interior" of the side (below, we'll deal with subsides and their subcells)
          constrainingBasis = constrainingTrialOrdering->getBasis(var->ID(),sideConstraint.sideOrdinal);
          basis = trialOrdering->getBasis(var->ID(),sideOrdinal);
          
          refBranch = sideRefinementsForSideEntity[sideEntityForSubcell[sideDim][sideOrdinal]];
          set<int> sideBasisOrdinals = BasisReconciliation::interiorDofOrdinalsForBasis(basis); // isL2Var ? basis->dofOrdinalsForSubcells(sideDim, true) : basis->dofOrdinalsForInterior(); // if L^2, all dofs are interior...
          basisDofOrdinals = BasisReconciliation::internalDofIndicesForFinerBasis(basis, refBranch);
          
          if (basisDofOrdinals.size() > 0) { // can be empty (consider a linear H^1 basis, e.g. -- there will be no middle nodes)
            FieldContainer<double> constraintMatrixSide = _br.constrainedWeights(basis, refBranch, constrainingBasis, composedPermutation);
            
            if (constraintMatrixSide.size() == 0) {
              cout << "Error: empty constraint matrix encountered.\n";
              TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Error: empty constraint matrix encountered.\n");
            }
            
            if (constraintMatrixSide.dimension(0) != basisDofOrdinals.size()) {
              cout << "GDAMinimumRule internal error. constraintMatrixSide row dimension does not match basisDofOrdinals size.\n";
              // repeat, so we can walk through in the debugger:
              // computeConstrainedWeights(finerBasis,refinements,coarserBasis,vertexNodePermutation);
              FieldContainer<double> constraintMatrixSide = BasisReconciliation::computeConstrainedWeights(basis, refBranch, constrainingBasis, composedPermutation);
              TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "constraintMatrixSide row dimension does not match basisDofOrdinals size");
            }
          
            if (!omitSideEntry && !omitVarEntry) {
              sideBasisMap.push_back(SubBasisDofMapper::subBasisDofMapper(basisDofOrdinals, globalDofOrdinals, constraintMatrixSide));
            }
//            cout << "getDofMapper: for var " << var->ID() << " on side " << sideOrdinal << ", adding side sub-basis dofMapper for " << basisDofOrdinals.size() << "  local dofOrdinals";
//            cout << ", mapping to " << globalDofOrdinals.size() << " global dof ordinals.\n";
//            cout << "( ";
//            for (set<unsigned>::iterator ordIt=basisDofOrdinals.begin(); ordIt != basisDofOrdinals.end(); ordIt++) {
//              cout << *ordIt << " ";
//            }
//            cout << ") ---> ( ";
//            for (vector<unsigned>::iterator ordIt=globalDofOrdinals.begin(); ordIt != globalDofOrdinals.end(); ordIt++) {
//              cout << *ordIt << " ";
//            }
//            cout << ")\n";
//            cout << "constraintMatrixSide:\n" << constraintMatrixSide;
          }
        }
        
        // if the space is L2, then we will have addressed all the dofs that belong to the subcells...
        if (!isL2Var) {
          vector< map< unsigned, unsigned > > subsideMap = buildSubsideMap(sideTopo);
          
          for (int d=0; d<sideDim; d++) {
            unsigned scCount = sideTopo.getSubcellCount(d);
            for (unsigned scOrdinalInSide = 0; scOrdinalInSide < scCount; scOrdinalInSide++) {
              unsigned scOrdinalInCell = CamelliaCellTools::subcellOrdinalMap(*topo, sideDim, sideOrdinal, d, scOrdinalInSide);
//              if (processedSubcells[d].find(scOrdinalInCell) == processedSubcells[d].end()) { // haven't processed this one yet
                // determine the subcell index in _meshTopology (will be used below to determine the subcell ordinal of the constraining subcell in the constraining cell)
              IndexType sideEntityIndex = cell->entityIndex(sideDim, sideOrdinal);
              IndexType scIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, d, scOrdinalInSide);
              IndexType constrainingScIndex = _meshTopology->getConstrainingEntityIndex(d, scIndex);
                
              set<unsigned> scBasisOrdinals;
              FieldContainer<double> constraintMatrixSubcell;
              
              if (varHasSupportOnVolume) {
                unsigned scOrdinalInConstrainingCell = constrainingCellForSide->findSubcellOrdinal(d,constrainingScIndex);
                constraintMatrixSubcell = BasisReconciliation::subBasisReconciliationWeightsForSubcell(subBasisWeights, d, basis, scOrdinalInCell,
                                                                                                       constrainingBasis, scOrdinalInConstrainingCell, scBasisOrdinals);
              } else {
                // here, we are dealing with a subside or one of its constituents.  We want to use the constraint info for that subside.
                unsigned subsideOrdinal = subsideMap[d][scOrdinalInSide];
                ConstrainingSubsideInfo subsideConstraint = constraints.subsideConstraints[sideOrdinal][subsideOrdinal];
                DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[subsideConstraint.cellID]->trialOrderPtr;
                BasisPtr constrainingBasis = constrainingTrialOrdering->getBasis(var->ID(),subsideConstraint.sideOrdinal);
                
                // by construction, side bases have the same view of the subside (we always match a side with one of its ancestors)-- the permutation is 0, the identity
                unsigned permutation = 0; // I *think* this is right…
                GlobalIndexType sideEntityIndexForReconciliation = sideEntityForSubcell[d][scOrdinalInCell]; // the one that was used in the above call to constrainedWeights()
                GlobalIndexType sideEntityIndexInCell = cell->entityIndex(sideDim, sideOrdinal);
                
                unsigned subsideEntityIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndexInCell, sideDim-1, subsideOrdinal);
                unsigned subsideOrdinalInCellSide = subsideOrdinalInSideEntityOfInterest[sideEntityIndexInCell][subsideEntityIndex];
                unsigned subsideOrdinalInReconciledSideEntity = subsideOrdinalInSideEntityOfInterest[sideEntityIndexForReconciliation][subsideEntityIndex];
                subBasisWeights = _br.constrainedWeights(basis, subsideOrdinalInReconciledSideEntity, refBranch, constrainingBasis, subsideConstraint.subsideOrdinalInSide, permutation);
                
                // Here, we consider the possibility that the subcell orientation (vertex permutation) in the cell's side differs from that used in our constraint computation.
                if (sideEntityIndexForReconciliation != sideEntityIndexInCell) {
                  unsigned cellSubsideEntityPermutation = _meshTopology->getSubEntityPermutation(sideDim, sideEntityIndexInCell, sideDim-1, subsideOrdinalInCellSide);
                  unsigned reconciledSubsideEntityPermutation = _meshTopology->getSubEntityPermutation(sideDim, sideEntityIndexForReconciliation, sideDim-1, subsideOrdinalInReconciledSideEntity);
                  
                  shards::CellTopology subsideTopo = _meshTopology->getEntityTopology(sideDim-1, subsideEntityIndex);
                  
                  unsigned reconciledSubsideEntityPermutationInverse = CamelliaCellTools::permutationInverse(subsideTopo, reconciledSubsideEntityPermutation);
                  unsigned cellSideToSideEntitySubsidePermutation = CamelliaCellTools::permutationComposition(subsideTopo, reconciledSubsideEntityPermutationInverse, cellSubsideEntityPermutation);
                  
                  SubBasisReconciliationWeights cellSideToSideEntityReconciliationWeights = _br.constrainedWeights(basis, subsideOrdinal, basis, subsideOrdinalInReconciledSideEntity, cellSideToSideEntitySubsidePermutation);
                  
                  // now, we need to fold cellSideToSideEntityReconciliationWeights into subBasisWeights
                  // for nodal bases, cellSideToSideEntityReconciliationWeights should be a permutation matrix.
                  // in general, it should be a square matrix that matches the fine basis cardinality in subBasisWeights.
                  int cellSubsideDofCount = cellSideToSideEntityReconciliationWeights.fineOrdinals.size();
                  int reconciledSubsideDofCount = cellSideToSideEntityReconciliationWeights.coarseOrdinals.size();
                  int reconciledSubsideDofCount2 = subBasisWeights.fineOrdinals.size();
                  
                  if (cellSubsideDofCount != reconciledSubsideDofCount) {
                    cout << "(cellSubsideDofCount != reconciledSubsideDofCount) : (" << cellSubsideDofCount << " != " << reconciledSubsideDofCount << ")\n";
                    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "cellSubsideDofCount != reconciledSubsideDofCount");
                  }
                  if (reconciledSubsideDofCount != reconciledSubsideDofCount2) {
                    cout << "(reconciledSubsideDofCount != reconciledSubsideDofCount2) : (" << reconciledSubsideDofCount << " != " << reconciledSubsideDofCount2 << ")\n";
                    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "reconciledSubsideDofCount != reconciledSubsideDofCount2");
                  }
                  
//                  print("subBasisWeights (before composition): fineOrdinals", subBasisWeights.fineOrdinals);
//                  print("subBasisWeights (before composition): coarseOrdinals", subBasisWeights.coarseOrdinals);
//                  
//                  print("cellSideToSideEntityReconciliationWeights: fineOrdinals", cellSideToSideEntityReconciliationWeights.fineOrdinals);
//                  print("cellSideToSideEntityReconciliationWeights: coarseOrdinals", cellSideToSideEntityReconciliationWeights.coarseOrdinals);
                  
                  subBasisWeights = BasisReconciliation::composedSubBasisReconciliationWeights(cellSideToSideEntityReconciliationWeights, subBasisWeights);

//                  print("subBasisWeights (after composition): fineOrdinals", subBasisWeights.fineOrdinals);
//                  print("subBasisWeights (after composition): coarseOrdinals", subBasisWeights.coarseOrdinals);
                }
                
                CellPtr constrainingCell = _meshTopology->getCell(subsideConstraint.cellID);
                unsigned scOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(d,constrainingScIndex);
                unsigned scOrdinalInConstrainingSide = CamelliaCellTools::subcellReverseOrdinalMap(*constrainingCell->topology(),
                                                                                                   sideDim, subsideConstraint.sideOrdinal,
                                                                                                   d, scOrdinalInConstrainingCell);
                
                constraintMatrixSubcell = BasisReconciliation::subBasisReconciliationWeightsForSubcell(subBasisWeights, d, basis, scOrdinalInSide,
                                                                                                       constrainingBasis, scOrdinalInConstrainingSide, scBasisOrdinals);
                
                if (constraintMatrixSubcell.size() == 0) {
                  cout << "Error: empty constraint matrix encountered.\n";
                  TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Error: empty constraint matrix encountered.\n");
                }
              }
              
              vector<GlobalIndexType> globalDofOrdinals = dofIndexInfo[d][scOrdinalInCell][var->ID()];
              
              if (varHasSupportOnVolume) {
                if (scBasisOrdinals.size() > 0) {
                  if (!omitSideEntry && !omitVarEntry) {
                    volumeBasisMap.push_back(SubBasisDofMapper::subBasisDofMapper(scBasisOrdinals, globalDofOrdinals, constraintMatrixSubcell));
                  }
                  //                    cout << "getDofMapper: for var " << var->ID() << " on side " << sideOrdinal << ", adding volume sub-basis dofMapper (for subcell) for " << basisDofOrdinals.size() << "  local dofOrdinals";
                  //                    cout << ", mapping to " << globalDofOrdinals.size() << " global dof ordinals.\n";
                  //                    cout << "( ";
                  //                    for (set<unsigned>::iterator ordIt=scBasisOrdinals.begin(); ordIt != scBasisOrdinals.end(); ordIt++) {
                  //                      cout << *ordIt << " ";
                  //                    }
                  //                    cout << ") ---> ( ";
                  //                    for (vector<unsigned>::iterator ordIt=globalDofOrdinals.begin(); ordIt != globalDofOrdinals.end(); ordIt++) {
                  //                      cout << *ordIt << " ";
                  //                    }
                  //                    cout << ")\n";
                }
              } else {
                if (scBasisOrdinals.size() > 0) {
                  if (!omitSideEntry && !omitVarEntry) {
                    sideBasisMap.push_back(SubBasisDofMapper::subBasisDofMapper(scBasisOrdinals, globalDofOrdinals, constraintMatrixSubcell));
                  }
                  //                    cout << "getDofMapper: for var " << var->ID() << " on side " << sideOrdinal << ", adding side sub-basis dofMapper (for subcell) for " << basisDofOrdinals.size() << "  local dofOrdinals";
                  //                    cout << ", mapping to " << globalDofOrdinals.size() << " global dof ordinals.\n";
                  //                    cout << "( ";
                  //                    for (set<unsigned>::iterator ordIt=scBasisOrdinals.begin(); ordIt != scBasisOrdinals.end(); ordIt++) {
                  //                      cout << *ordIt << " ";
                  //                    }
                  //                    cout << ") ---> ( ";
                  //                    for (vector<unsigned>::iterator ordIt=globalDofOrdinals.begin(); ordIt != globalDofOrdinals.end(); ordIt++) {
                  //                      cout << *ordIt << " ";
                  //                    }
                  //                    cout << ")\n";
                }
              }
              
                //processedSubcells[d].insert(scOrdinalInCell);
              
              //}
            }
          }
        }
        if (!varHasSupportOnVolume) {
          sideMaps[sideOrdinal][var->ID()] = sideBasisMap;
        }
      }
    }
    if (varHasSupportOnVolume) {
      volumeMap[var->ID()] = volumeBasisMap;
    }
  }
  
  return Teuchos::rcp( new LocalDofMapper(trialOrdering,volumeMap,sideMaps,varIDToMap,sideOrdinalToMap) );
}

PartitionIndexType GDAMinimumRule::partitionForGlobalDofIndex( GlobalIndexType globalDofIndex ) {
  PartitionIndexType numRanks = _partitionDofCounts.size();
  GlobalIndexType totalDofCount = 0;
  for (PartitionIndexType i=0; i<numRanks; i++) {
    totalDofCount += _partitionDofCounts(i);
    if (totalDofCount > globalDofIndex) {
      return i;
    }
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Invalid globalDofIndex");
}

void GDAMinimumRule::printConstraintInfo(GlobalIndexType cellID) {
  CellConstraints cellConstraints = getCellConstraints(cellID);
  cout << "***** Constraints for cell " << cellID << " ****** \n";
  unsigned sideCount = cellConstraints.sideConstraints.size();
  for (int sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
    cout << "Side " << sideOrdinal << ":\n";
    ConstrainingCellInfo sideConstraint = cellConstraints.sideConstraints[sideOrdinal];
    cout << "    constraining cell: " << sideConstraint.cellID << ", side " << sideConstraint.sideOrdinal << endl;
    int subsideCount = cellConstraints.subsideConstraints[sideOrdinal].size();
    for (int subsideOrdinal = 0; subsideOrdinal < subsideCount; subsideOrdinal++) {
      ConstrainingSubsideInfo subsideConstraint = cellConstraints.subsideConstraints[sideOrdinal][subsideOrdinal];
      cout << "        subside " << subsideOrdinal << ": cellID " << subsideConstraint.cellID << ", side " << subsideConstraint.sideOrdinal;
      cout << ", subsideOrdinalInSide " << subsideConstraint.subsideOrdinalInSide << endl;
    }
  }
  cout << "Ownership info:\n";
  CellPtr cell = _meshTopology->getCell(cellID);
  int spaceDim = cell->topology()->getDimension();
  for (int d=0; d<spaceDim; d++) {
    cout << "  dimension " << d << " subcells: " << endl;
    int subcellCount = cell->topology()->getSubcellCount(d);
    for (int scord=0; scord<subcellCount; scord++) {
      cout << "    ordinal " << scord  << " owned by cell " << cellConstraints.owningCellIDForSubcell[d][scord].first << endl;
    }
  }
}

void GDAMinimumRule::rebuildLookups() {
  _constraintsCache.clear(); // to free up memory, could clear this again after the lookups are rebuilt.  Having the cache is most important during the construction below.
  
  determineActiveElements(); // call to super: constructs cell partitionings
  
  int rank = Teuchos::GlobalMPISession::getRank();
  vector<GlobalIndexType> myCellIDs = _partitions[rank];
  
  map<int, VarPtr> trialVars = _varFactory.trialVars();
  
  _partitionDofCount = 0; // how many dofs we own locally
  _cellDofOffsets.clear(); // within the partition, offsets for the owned dofs in cell
  
  int spaceDim = _meshTopology->getSpaceDim();
  int sideDim = spaceDim - 1;
  
  // pieces of this remain fairly ugly--the brute force searches are limited to entities on a cell (i.e. < O(12) items to search in a hexahedron),
  // and I've done a reasonable job only doing them when we need the result, but they still are brute force searches.  By tweaking
  // the design of MeshTopology and Cell to take better advantage of regularities (or just to store better lookups), we should be able to do better.
  // But in the interest of avoiding wasting development time on premature optimization, I'm leaving it as is for now...
  for (vector<GlobalIndexType>::iterator cellIDIt = myCellIDs.begin(); cellIDIt != myCellIDs.end(); cellIDIt++) {
    GlobalIndexType cellID = *cellIDIt;
    _cellDofOffsets[cellID] = _partitionDofCount;
    CellPtr cell = _meshTopology->getCell(cellID);
    CellTopoPtr topo = cell->topology();
    int sideCount = topo->getSideCount();
    CellConstraints constraints = getCellConstraints(cellID);
    DofOrderingPtr trialOrdering = _elementTypeForCell[cellID]->trialOrderPtr;
    for (map<int, VarPtr>::iterator varIt = trialVars.begin(); varIt != trialVars.end(); varIt++) {
      VarPtr var = varIt->second;
      bool isL2Var = (var->space() == L2) || (var->space() == VECTOR_L2);
      bool varHasSupportOnVolume = (var->varType() == FIELD) || (var->varType() == TEST);
      if ( varHasSupportOnVolume ) { // i.e. a variable with support on the volume
        BasisPtr basis = trialOrdering->getBasis(var->ID());
        if (isL2Var) { // unconstrained / local
          _partitionDofCount += basis->getCardinality();
        } else {
          _partitionDofCount += BasisReconciliation::interiorDofOrdinalsForBasis(basis).size(); // basis->dofOrdinalsForInterior().size();
        }
      }

      if ( ! (varHasSupportOnVolume && isL2Var) ) { // if space is L^2 on the volume, we'll have claimed all the dofs above, and we can skip further processing
        vector< set<unsigned> > processedSubcells(sideDim);
        for (int sideOrdinal=0; sideOrdinal<sideCount; sideOrdinal++) {
          shards::CellTopology sideTopo = topo->getCellTopologyData(sideDim, sideOrdinal);
          GlobalIndexType owningCellIDForSide = constraints.owningCellIDForSubcell[sideDim][sideOrdinal].first;
          if (owningCellIDForSide == cellID) {
            ConstrainingCellInfo sideConstraint = constraints.sideConstraints[sideOrdinal];
            DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[sideConstraint.cellID]->trialOrderPtr;
            if (varHasSupportOnVolume) {
              BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID());
              set<int> sideBasisOrdinals = basis->dofOrdinalsForSubcell(sideDim, sideConstraint.sideOrdinal, sideDim);
              _partitionDofCount += sideBasisOrdinals.size();
            } else {
              BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID(),sideConstraint.sideOrdinal);
              if (isL2Var) { // unconstrained / local
                _partitionDofCount += basis->getCardinality();
              } else {
                _partitionDofCount += BasisReconciliation::interiorDofOrdinalsForBasis(basis).size(); //basis->dofOrdinalsForInterior().size();
              }
            }
          }
          
          vector< map< unsigned, unsigned > > subsideMap = buildSubsideMap(sideTopo);
          
          for (int d=0; d<sideDim; d++) {
            unsigned scCount = sideTopo.getSubcellCount(d);
            for (unsigned scOrdinalInSide = 0; scOrdinalInSide < scCount; scOrdinalInSide++) {
              unsigned scOrdinalInCell = CamelliaCellTools::subcellOrdinalMap(*topo, sideDim, sideOrdinal, d, scOrdinalInSide);
              if (processedSubcells[d].find(scOrdinalInCell) == processedSubcells[d].end()) { // haven't processed this one yet
                GlobalIndexType owningCellID = constraints.owningCellIDForSubcell[d][scOrdinalInCell].first;
                
                // determine the subcell index in _meshTopology (will be used below to determine the subcell ordinal of the constraining subcell in the constraining cell)
                IndexType sideEntityIndex = cell->entityIndex(sideDim, sideOrdinal);
                IndexType scIndex = _meshTopology->getSubEntityIndex(sideDim, sideEntityIndex, d, scOrdinalInSide);
                IndexType constrainingScIndex = _meshTopology->getConstrainingEntityIndex(d, scIndex);
                
                if (owningCellID == cellID) {
                  if (varHasSupportOnVolume) {
                    ConstrainingCellInfo sideConstraint = constraints.sideConstraints[sideOrdinal];
                    DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[sideConstraint.cellID]->trialOrderPtr;
                    BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID());
                    CellPtr constrainingCell = _meshTopology->getCell(sideConstraint.cellID);
                    
                    unsigned scOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(d,constrainingScIndex);
                    set<int> scBasisOrdinals = basis->dofOrdinalsForSubcell(d, scOrdinalInConstrainingCell, d);
                    _partitionDofCount += scBasisOrdinals.size();
                  } else {
                    if (!isL2Var) {
                      // here, we are dealing with a subside or one of its constituents.  We want to use the constraint info for that subside.
                      unsigned subsideOrdinal = subsideMap[d][scOrdinalInSide];
                      ConstrainingSubsideInfo subsideConstraint = constraints.subsideConstraints[sideOrdinal][subsideOrdinal];
                      DofOrderingPtr constrainingTrialOrdering = _elementTypeForCell[subsideConstraint.cellID]->trialOrderPtr;
                      BasisPtr basis = constrainingTrialOrdering->getBasis(var->ID(),subsideConstraint.sideOrdinal);
                      
                      CellPtr constrainingCell = _meshTopology->getCell(subsideConstraint.cellID);
                      unsigned scOrdinalInConstrainingCell = constrainingCell->findSubcellOrdinal(d,constrainingScIndex);
                      unsigned scOrdinalInConstrainingSide = CamelliaCellTools::subcellReverseOrdinalMap(*constrainingCell->topology(),
                                                                                                         sideDim, subsideConstraint.sideOrdinal,
                                                                                                         d, scOrdinalInConstrainingCell);

                      set<int> scBasisOrdinals = basis->dofOrdinalsForSubcell(d, scOrdinalInConstrainingSide, d);
                      _partitionDofCount += scBasisOrdinals.size();
                    }
                  }
                }
                
                processedSubcells[d].insert(scOrdinalInCell);
              }
            }
          }
        }
      }
    }
  }
  int numRanks = Teuchos::GlobalMPISession::getNProc();
  _partitionDofCounts.resize(numRanks);
  _partitionDofCounts.initialize(0.0);
  _partitionDofCounts[rank] = _partitionDofCount;
  MPIWrapper::entryWiseSum(_partitionDofCounts);
//  if (rank==0) cout << "partitionDofCounts:\n" << _partitionDofCounts;
  _partitionDofOffset = 0; // add this to a local partition dof index to get the global dof index
  for (int i=0; i<rank; i++) {
    _partitionDofOffset += _partitionDofCounts[i];
  }
  _globalDofCount = _partitionDofOffset;
  for (int i=rank; i<numRanks; i++) {
    _globalDofCount += _partitionDofCounts[i];
  }
//  if (rank==0) cout << "globalDofCount: " << _globalDofCount << endl;
  // collect and communicate global cell dof offsets:
  int activeCellCount = _meshTopology->getActiveCellIndices().size();
  FieldContainer<int> globalCellIDDofOffsets(activeCellCount);
  int partitionCellOffset = 0;
  for (int i=0; i<rank; i++) {
    partitionCellOffset += _partitions[i].size();
  }
  // fill in our _cellDofOffsets:
  for (int i=0; i<myCellIDs.size(); i++) {
    globalCellIDDofOffsets[partitionCellOffset+i] = _cellDofOffsets[myCellIDs[i]] + _partitionDofOffset;
  }
  // global copy:
  MPIWrapper::entryWiseSum(globalCellIDDofOffsets);
  // fill in the lookup table:
  _globalCellDofOffsets.clear();
  int globalCellIndex = 0;
  for (int i=0; i<numRanks; i++) {
    vector<GlobalIndexType> rankCellIDs = _partitions[i];
    for (vector<GlobalIndexType>::iterator cellIDIt = rankCellIDs.begin(); cellIDIt != rankCellIDs.end(); cellIDIt++) {
      GlobalIndexType cellID = *cellIDIt;
      _globalCellDofOffsets[cellID] = globalCellIDDofOffsets[globalCellIndex];
      globalCellIndex++;
    }
  }
  
  _cellIDsForElementType = vector< map< ElementType*, vector<GlobalIndexType> > >(numRanks);
  for (int i=0; i<numRanks; i++) {
    vector<GlobalIndexType> cellIDs = _partitions[i];
    for (vector<GlobalIndexType>::iterator cellIDIt = cellIDs.begin(); cellIDIt != cellIDs.end(); cellIDIt++) {
      GlobalIndexType cellID = *cellIDIt;
      ElementTypePtr elemType = _elementTypeForCell[cellID];
      _cellIDsForElementType[i][elemType.get()].push_back(cellID);
    }
  }
}