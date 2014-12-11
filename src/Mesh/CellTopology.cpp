//
//  CellTopology.cpp
//  Camellia
//
//  Created by Nate Roberts on 9/15/14.
//
//

#include "CellTopology.h"
#include "CamelliaDebugUtility.h"

using namespace Camellia;

// define our static map:
map< pair<unsigned, unsigned>, CellTopoPtr > CellTopology::_tensorizedTrilinosTopologies;

CellTopology::CellTopology(const shards::CellTopology &baseTopo, unsigned tensorialDegree) {
  _shardsBaseTopology = baseTopo;
  _tensorialDegree = tensorialDegree;
  
  int baseDim = baseTopo.getDimension();
  vector<unsigned> subcellCounts = vector<unsigned>(baseDim + _tensorialDegree + 1);
  _subcells = vector< vector< CellTopoPtr > >(baseDim + _tensorialDegree + 1);
  
  if (!isHypercube() && (_tensorialDegree > 1)) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensorial degrees higher than 1 aren't supported for non-hypercube topologies");
    // (the reason is that permutations get a bit complex for this case, and we haven't yet implemented said complexity)
    // (it would be legitimate to defer this exception until permutations are requested, but I think on present implementations, they virtually
    //  always will be, so might as well fail sooner).
  }
  
  if (_tensorialDegree==0) {
    for (int d=0; d<=baseDim; d++) {
      subcellCounts[d] = baseTopo.getSubcellCount(d);
    }
  } else {
    CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
    subcellCounts[0] = 2 * tensorComponentTopo->getSubcellCount(0);
    for (int d=1; d < baseDim+_tensorialDegree; d++) {
      subcellCounts[d] = 2 * tensorComponentTopo->getSubcellCount(d) + tensorComponentTopo->getSubcellCount(d-1);
    }
    subcellCounts[baseDim + _tensorialDegree] = 1; // the volume topology
  }
  for (int d=0; d<baseDim+_tensorialDegree; d++) {
    _subcells[d] = vector< CellTopoPtr >(subcellCounts[d]);
    for (int scord=0; scord<_subcells[d].size(); scord++) {
      _subcells[d][scord] = getSubcell(d, scord);
    }
  }
  _subcells[baseDim+_tensorialDegree] = vector<CellTopoPtr>(1);
  _subcells[baseDim+_tensorialDegree][0] = Teuchos::rcp(this, false); // false: does not own memory (self-reference)
  
  computeAxisPermutations();
}

vector< vector<unsigned> > getPermutations(unsigned n) {
  if (n > 1) {
    vector< vector<unsigned> > previous = getPermutations(n-1); // permutations of (0, …, n-2)
    
    vector< vector<unsigned> > newList;
    // where should the n-1 entry go?  there are n choices.  Generate permutations for each.
    // so that the 0 entry corresponds to identity, start at the back of the list.
    for (int i=0; i<n; i++) {
      for (vector< vector<unsigned> >::iterator permIt= previous.begin(); permIt != previous.end(); permIt++) {
        vector<unsigned> permCopy = *permIt;
        permCopy.insert(permCopy.begin()+n-1-i, (unsigned) n-1);
        newList.push_back(permCopy);
      }
    }
    return newList;
  } else { // (n==1)
    vector< vector<unsigned> > singleton(1);
    singleton[0] = vector<unsigned>(1,0);
    return singleton;
  }
}

void CellTopology::computeAxisPermutations() {
  _axisPermutations = getPermutations(getDimension());
  for (int i=0; i<_axisPermutations.size(); i++) {
    _axisPermutationToOrdinal[_axisPermutations[i]] = i;
//    ostringstream ordinalString;
//    ordinalString << "axis permutation " << i;
//    Camellia::print(ordinalString.str(), _axisPermutations[i]);
  }
}

unsigned CellTopology::convertHypercubeOrdinalToShardsNodeOrdinal(unsigned spaceDim, unsigned hypercubeOrdinal) {
  switch (spaceDim) {
    case 0:
    case 1:
      return hypercubeOrdinal;
      break;
    case 2:
      switch (hypercubeOrdinal) {
        case 0:
          return 0;
          break;
        case 1:
          return 1;
        case 2:
          return 3;
        case 3:
          return 2;
        default:
          break;
      }
    case 3:
      switch (hypercubeOrdinal) {
        case 0:
          return 0;
          break;
        case 1:
          return 1;
        case 2:
          return 3;
        case 3:
          return 2;
        case 4:
          return 4;
        case 5:
          return 5;
        case 6:
          return 7;
        case 7:
          return 6;
        default:
          break;
      }
    default:
      break;
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unhandled hypercubeOrdinal");
}

unsigned CellTopology::convertShardsNodeOrdinalToHypercubeOrdinal(unsigned spaceDim, unsigned shards_node_ord) {
  switch (spaceDim) {
    case 0:
    case 1:
      return shards_node_ord;
      break;
    case 2:
      switch (shards_node_ord) {
        case 0:
          return 0;
          break;
        case 1:
          return 1;
        case 2:
          return 3;
        case 3:
          return 2;
        default:
          break;
      }
    case 3:
      switch (shards_node_ord) {
        case 0:
          return 0;
          break;
        case 1:
          return 1;
        case 2:
          return 3;
        case 3:
          return 2;
        case 4:
          return 4;
        case 5:
          return 5;
        case 6:
          return 7;
        case 7:
          return 6;
        default:
          break;
      }
    default:
      break;
  }
  TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unhandled shards_node_ord");
}

unsigned CellTopology::getTensorialDegree() const {
  return _tensorialDegree;
}

const shards::CellTopology & CellTopology::getShardsTopology() const {
  return _shardsBaseTopology;
}

/** \brief  Dimension of this cell topology */
unsigned CellTopology::getDimension() const {
  return _shardsBaseTopology.getDimension() + _tensorialDegree;
}

/** \brief  Node count of this cell topology */
unsigned CellTopology::getNodeCount() const {
  unsigned two_pow = 1 << _tensorialDegree;
  return getNodeCount(_shardsBaseTopology) * two_pow;
}

unsigned CellTopology::getNodeCount(const shards::CellTopology &shardsTopo) {
  if (shardsTopo.getDimension()==0) return 1; // Node topology; by my lights shards returns the wrong thing (0) here
  return shardsTopo.getNodeCount();
}

/** \brief  Vertex count of this cell topology */
unsigned CellTopology::getVertexCount() const {
  unsigned two_pow = 1 << _tensorialDegree;
  return _shardsBaseTopology.getVertexCount() * two_pow;
}

/** \brief  Edge boundary subcell count of this cell topology */
unsigned CellTopology::getEdgeCount() const {
  return getSubcellCount(1);
}

/** \brief  Face boundary subcell count of this cell topology */
unsigned CellTopology::getFaceCount() const {
  return getSubcellCount(2);
}

/** \brief  Side boundary subcell count of this cell topology */
unsigned CellTopology::getSideCount() const {
  int spaceDim = getDimension();
  if (spaceDim == 0) {
    return 0;
  } else {
    int sideDim = spaceDim - 1;
    return getSubcellCount(sideDim);
  }
}

/** \brief  Node count of a subcell of the given dimension and ordinal.
 *  \param  subcell_dim    [in]  - spatial dimension of the subcell
 *  \param  subcell_ord    [in]  - subcell ordinal
 */
unsigned CellTopology::getNodeCount( const unsigned subcell_dim ,
                                    const unsigned subcell_ord ) const {
  return _subcells[subcell_dim][subcell_ord]->getNodeCount();
}

/** \brief  Vertex count of a subcell of the given dimension and ordinal.
 *  \param  subcell_dim    [in]  - spatial dimension of the subcell
 *  \param  subcell_ord    [in]  - subcell ordinal
 */
unsigned CellTopology::getVertexCount( const unsigned subcell_dim ,
                                      const unsigned subcell_ord ) const {
  return _subcells[subcell_dim][subcell_ord]->getVertexCount();
}


/** \brief  Edge count of a subcell of the given dimension and ordinal.
 *  \param  subcell_dim    [in]  - spatial dimension of the subcell
 *  \param  subcell_ord    [in]  - subcell ordinal
 */
unsigned CellTopology::getEdgeCount( const unsigned subcell_dim ,
                                    const unsigned subcell_ord ) const {
  return _subcells[subcell_dim][subcell_ord]->getEdgeCount();
}

/** \brief  Side count of a subcell of the given dimension and ordinal.
 *  \param  subcell_dim    [in]  - spatial dimension of the subcell
 *  \param  subcell_ord    [in]  - subcell ordinal
 */
unsigned CellTopology::getSideCount( const unsigned subcell_dim ,
                                    const unsigned subcell_ord ) const {
  return _subcells[subcell_dim][subcell_ord]->getSideCount();
}


/** \brief  Subcell count of subcells of the given dimension.
 *  \param  subcell_dim    [in]  - spatial dimension of the subcell
 */
unsigned CellTopology::getSubcellCount( const unsigned subcell_dim ) const {
  if (subcell_dim >= _subcells.size()) return 0;
  else return _subcells[subcell_dim].size();
}

vector<unsigned> CellTopology::getHypercubeNodeAddress(unsigned spaceDim, unsigned node_ord) {
  vector<unsigned> address(spaceDim);
  for (int i=0; i<spaceDim; i++) {
    address[spaceDim-1-i] = node_ord % 2;
    node_ord /= 2;
  }
  return address;
}

unsigned CellTopology::getHypercubeNode(const vector<unsigned> &address) {
  int d = address.size();
  int node_ord = 0;
  for (int i=0; i<d; i++) {
    node_ord *= 2;
    node_ord += address[i];
  }
  return node_ord;
}

pair<unsigned, vector<unsigned> > CellTopology::getHypercubePermutation(unsigned spaceDim, unsigned permutation_ordinal) {
  int d_fact = 1;
  for (int i=0; i<=spaceDim; i++) {
    d_fact *= i;
  }
  unsigned two_pow = 1 << spaceDim;
  unsigned axisChoicePermutation = permutation_ordinal >> spaceDim;
  unsigned flipChoiceOrdinal = permutation_ordinal % two_pow;
  vector<unsigned> flipChoices = getHypercubeNodeAddress(spaceDim, flipChoiceOrdinal);
  return std::make_pair(axisChoicePermutation, flipChoices);
}

vector<unsigned> CellTopology::getAxisChoices(unsigned axisChoiceOrdinal) const
{
  return _axisPermutations[axisChoiceOrdinal];
}

unsigned CellTopology::getAxisChoiceOrdinal(const vector<unsigned> &axisChoices) const {
  return _axisPermutationToOrdinal.find(axisChoices)->second;
}

/** \brief  Mapping from a subcell's node ordinal to a
 *          node ordinal of this parent cell topology.
 *  \param  subcell_dim      [in]  - spatial dimension of the subcell
 *  \param  subcell_ord      [in]  - subcell ordinal
 *  \param  subcell_node_ord [in]  - node ordinal relative to subcell
 */
unsigned CellTopology::getNodeMap( const unsigned scdim ,
                                  const unsigned  scord ,
                                  const unsigned  sc_node_ord ) const {
  if (scdim==getDimension()) return sc_node_ord; // map from topology to itself
  if (_tensorialDegree==0) {
    return _shardsBaseTopology.getNodeMap(scdim, scord, sc_node_ord);
  } else {
    CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
    unsigned componentSubcellCount = tensorComponentTopo->getSubcellCount(scdim);
    if (scord < componentSubcellCount * 2) { // subcell belongs to one of the two component topologies
      unsigned scord_comp = scord % componentSubcellCount;  // subcell ordinal in the component topology
      unsigned compOrdinal = scord / componentSubcellCount; // which component topology? 0 or 1.
      unsigned mappedNodeInsideComponentTopology = tensorComponentTopo->getSubcell(scdim, scord_comp)->getNodeMap(scdim, scord_comp, sc_node_ord);
      return mappedNodeInsideComponentTopology + compOrdinal * tensorComponentTopo->getNodeCount();
    } else {
      // otherwise, the subcell is a tensor product of a component's (scdim-1)-dimensional subcell with the line topology.
      unsigned scord_comp = scord - componentSubcellCount * 2;
      unsigned scdim_comp = scdim - 1;
      CellTopoPtr subcellTensorComponent = tensorComponentTopo->getSubcell(scdim_comp, scord_comp);
      // which of the two copies of the subcell tensor component owns the node sc_node_ord?
      unsigned scCompOrdinal = sc_node_ord / subcellTensorComponent->getNodeCount(); // 0 or 1
      // what's the node ordinal inside the subcell component?
      unsigned scCompNodeOrdinal = sc_node_ord % subcellTensorComponent->getNodeCount();
      unsigned mappedNodeInsideComponentTopology = tensorComponentTopo->getNodeMap(scdim_comp, scord_comp, scCompNodeOrdinal);
      return mappedNodeInsideComponentTopology + scCompOrdinal * tensorComponentTopo->getNodeCount();
    }
  }
}

/** \brief  Number of node permutations defined for this cell */
unsigned CellTopology::getNodePermutationCount() const {
  if (isHypercube()) {
    // formula for hypercube symmetries: 2^d * d!
    int count = 1;
    for (int d=1; d<=getDimension(); d++) {
      count *= 2 * d;
    }
    return count;
  } else if (_tensorialDegree==0) {
    if (_shardsBaseTopology.getDimension()==3) {
      cout << "ERROR: getNodePermutationCount() not yet implemented for 3D shards topologies (except hexahedra).\n";
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "getNodePermutationCount() not yet implemented for 3D shards topologies (except hexahedra)");
    }
    return _shardsBaseTopology.getNodePermutationCount();
  } else {
    CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
    // a permutation of the tensor component induces one on the tensor product topology,
    // and we have one additional permutation for each of these in the form of a reflection in the tensorial direction
    // NOTE: for hypercube topologies, there are additional permutations (see above)
    return tensorComponentTopo->getNodePermutationCount() * 2;
  }
}

/** \brief  Permutation of a cell's node ordinals.
 *  \param  permutation_ordinal [in]
 *  \param  node_ordinal        [in]
 */
unsigned CellTopology::getNodePermutation( const unsigned permutation_ord ,
                                           const unsigned node_ord ) const {
  if (!isHypercube()) {
    // then the way we order the 2n permutation labels is:
    //  - the first n are the unreflected symmetries of the component topologies
    //  - the second n are the corresponding symmetries, but reflected across the tensorial direction
    if (_tensorialDegree==0) {
      if (_shardsBaseTopology.getDimension()==3) {
        cout << "ERROR: getNodePermutation() not yet implemented for 3D shards topologies (except hexahedra).\n";
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "getNodePermutation() not yet implemented for 3D shards topologies (except hexahedra)");
      }
      return _shardsBaseTopology.getNodePermutation(permutation_ord, node_ord);
    } else {
      int nodePermutationCount = _shardsBaseTopology.getNodePermutationCount();
      bool reflected = (permutation_ord >= nodePermutationCount);
      unsigned component_permutation_ord = permutation_ord % nodePermutationCount;
      CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
      int componentNodeCount = tensorComponentTopo->getNodeCount();
      unsigned component_node_ord = node_ord % componentNodeCount;
      int nodeComponentOrdinal = node_ord / componentNodeCount; // 0 or 1
      unsigned component_permuted_node_ord = tensorComponentTopo->getNodePermutation(component_permutation_ord, component_node_ord);
      if (reflected) nodeComponentOrdinal = (nodeComponentOrdinal + 1) % 2; // swap components if reflected
      return component_permuted_node_ord + nodeComponentOrdinal * componentNodeCount;
    }
  } else { // hypercube
    // basically, what you get to choose here is first the axis labeling (d! choices)
    // and then whether to flip each of the axes (2^d choices)
    
    // we define the ordering of the unpermuted hypercube to be lexicographic base 2:
    // in 3D, 000 is the x0=0, x1=0, x2=0 vertex, 011 is x0=1, x1=1, x2=0 vertex, etc.
    // (that is, you read right to left).  This preserves the usual x,y,z order while also
    // maintaining our convention regarding tensorial component node numbering.
    
    // TODO: get the node_ord corresponding to the base shards topology
    //       then, convert to our hypercube numbering, then add back on the discarded bits of the node_ord
    //       corresponding to the tensorial product dimensions...
    
    // we may have a shards hypercube topology that's tensor-producted several times with line topology
    
    // node numbering in each tensor product is such that the tensorial component topologies are
    // contiguously numbered.
    // therefore, the base_node_ord is just the modulus of node_ord:
    unsigned shards_base_node_ord = node_ord % getNodeCount(_shardsBaseTopology);
    unsigned tensorialOrdinal = node_ord / getNodeCount(_shardsBaseTopology); // this should be understood as a base-2 address
    
    unsigned node_ord_shards_hypercube = convertShardsNodeOrdinalToHypercubeOrdinal(_shardsBaseTopology.getDimension(), shards_base_node_ord);
    vector<unsigned> shards_address = getHypercubeNodeAddress(_shardsBaseTopology.getDimension(), node_ord_shards_hypercube);
    vector<unsigned> tensorial_address = getHypercubeNodeAddress(_tensorialDegree, tensorialOrdinal);
    
    vector<unsigned> address;
    address.insert(address.begin(), shards_address.begin(), shards_address.end());
    address.insert(address.begin(), tensorial_address.begin(), tensorial_address.end());

    // get info about the permution:
    pair< unsigned, vector<unsigned> > permutationInfo = this->getHypercubePermutation(getDimension(), permutation_ord);
    unsigned axisChoiceOrdinal = permutationInfo.first;
    vector<unsigned> axisChoices = getAxisChoices(axisChoiceOrdinal);
    vector<unsigned> flipChoices = permutationInfo.second;
    
    // permute the address according to the axis choices:
    vector<unsigned> permutedAddress(address.size());
    for (int i=0; i<address.size(); i++) {
      permutedAddress[i] = address[axisChoices[i]];
    }
    
    vector<unsigned> permutedAddress_tensorial_part(_tensorialDegree);
    vector<unsigned> permutedAddress_shards_part(_shardsBaseTopology.getDimension());
    
    for (int i=0; i<address.size(); i++) {
      permutedAddress[i] = (permutedAddress[i] + flipChoices[i]) % 2;
    }
    
    for (int i=0; i<address.size(); i++) {
      if (i<_tensorialDegree) {
        permutedAddress_tensorial_part[i] = permutedAddress[i];
      } else {
        permutedAddress_shards_part[i-_tensorialDegree] = permutedAddress[i];
      }
    }

    unsigned permutedTensorialOrdinal = getHypercubeNode(permutedAddress_tensorial_part);
    unsigned permuted_node_ord_shards_hypercube = getHypercubeNode(permutedAddress_shards_part);
    
    unsigned permutedShardsOrdinal = convertHypercubeOrdinalToShardsNodeOrdinal(_shardsBaseTopology.getDimension(), permuted_node_ord_shards_hypercube);
    
    return (permutedTensorialOrdinal * getNodeCount(_shardsBaseTopology)) + permutedShardsOrdinal;
  }
}

/** \brief  Inverse permutation of a cell's node ordinals.
 *  \param  permutation_ordinal [in]
 *  \param  node_ordinal        [in]
 */
unsigned CellTopology::getNodePermutationInverse( const unsigned permutation_ord ,
                                                 const unsigned node_ord ) const {
  if (!isHypercube()) {
    // then the way we order the 2n permutation labels is:
    //  - the first n are the unreflected symmetries of the component topologies
    //  - the second n are the corresponding symmetries, but reflected across the tensorial direction
    if (_tensorialDegree==0) {
      if (_shardsBaseTopology.getDimension()==3) {
        cout << "ERROR: getNodePermutation() not yet implemented for 3D shards topologies (except hexahedra).\n";
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "getNodePermutation() not yet implemented for 3D shards topologies (except hexahedra)");
      }
      return _shardsBaseTopology.getNodePermutationInverse(permutation_ord, node_ord);
    } else {
      int nodePermutationCount = _shardsBaseTopology.getNodePermutationCount();
      bool reflected = (permutation_ord >= nodePermutationCount);
      unsigned component_permutation_ord = permutation_ord % nodePermutationCount;
      CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
      int componentNodeCount = tensorComponentTopo->getNodeCount();
      unsigned component_node_ord = node_ord % componentNodeCount;
      int nodeComponentOrdinal = node_ord / componentNodeCount; // 0 or 1
      unsigned component_permuted_node_ord = tensorComponentTopo->getNodePermutationInverse(component_permutation_ord, component_node_ord);
      if (reflected) nodeComponentOrdinal = (nodeComponentOrdinal + 1) % 2; // swap components if reflected
      return component_permuted_node_ord + nodeComponentOrdinal * componentNodeCount;
    }
  } else { // hypercube
    // basically, what you get to choose here is first the axis labeling (d! choices)
    // and then whether to flip each of the axes (2^d choices)
    
    // we define the ordering of the unpermuted hypercube to be lexicographic base 2:
    // in 3D, 000 is the x0=0, x1=0, x2=0 vertex, 011 is x0=1, x1=1, x2=0 vertex, etc.
    // (that is, you read right to left).  This preserves the usual x,y,z order while also
    // maintaining our convention regarding tensorial component node numbering.
    
    // TODO: get the node_ord corresponding to the base shards topology
    //       then, convert to our hypercube numbering, then add back on the discarded bits of the node_ord
    //       corresponding to the tensorial product dimensions...
    
    // we may have a shards hypercube topology that's tensor-producted several times with line topology
    
    // node numbering in each tensor product is such that the tensorial component topologies are
    // contiguously numbered.
    // therefore, the base_node_ord is just the modulus of node_ord:
    unsigned shards_base_node_ord = node_ord % getNodeCount(_shardsBaseTopology);
    unsigned tensorialOrdinal = node_ord / getNodeCount(_shardsBaseTopology); // this should be understood as a base-2 address
    
    unsigned node_ord_shards_hypercube = convertShardsNodeOrdinalToHypercubeOrdinal(_shardsBaseTopology.getDimension(), shards_base_node_ord);
    vector<unsigned> shards_address = getHypercubeNodeAddress(_shardsBaseTopology.getDimension(), node_ord_shards_hypercube);
    vector<unsigned> tensorial_address = getHypercubeNodeAddress(_tensorialDegree, tensorialOrdinal);
    
    vector<unsigned> address;
    address.insert(address.begin(), shards_address.begin(), shards_address.end());
    address.insert(address.begin(), tensorial_address.begin(), tensorial_address.end());
    
    // get info about the permution:
    pair< unsigned, vector<unsigned> > permutationInfo = this->getHypercubePermutation(getDimension(), permutation_ord);
    unsigned axisChoiceOrdinal = permutationInfo.first;
    vector<unsigned> axisChoices = getAxisChoices(axisChoiceOrdinal);
    vector<unsigned> flipChoices = permutationInfo.second;

    // flip, then permute axes (reverse of the forward permutation)
    for (int i=0; i<address.size(); i++) {
      address[i] = (address[i] + flipChoices[i]) % 2; // flips are their own inverses, so this code is identical to the forward permutation
    }
    
    vector<unsigned> permutedAddress_tensorial_part(_tensorialDegree);
    vector<unsigned> permutedAddress_shards_part(_shardsBaseTopology.getDimension());

    // permute the address according to the axis choices:
    vector<unsigned> permutedAddress(address.size());
    for (int i=0; i<address.size(); i++) {
      permutedAddress[axisChoices[i]] = address[i];
    }
    
    // copy out the components:
    for (int i=0; i<address.size(); i++) {
      if (i<_tensorialDegree) {
        permutedAddress_tensorial_part[i] = permutedAddress[i];
      } else {
        permutedAddress_shards_part[i-_tensorialDegree] = permutedAddress[i];
      }
    }
    
    unsigned permutedTensorialOrdinal = getHypercubeNode(permutedAddress_tensorial_part);
    unsigned permuted_node_ord_shards_hypercube = getHypercubeNode(permutedAddress_shards_part);
    
    unsigned permutedShardsOrdinal = convertHypercubeOrdinalToShardsNodeOrdinal(_shardsBaseTopology.getDimension(), permuted_node_ord_shards_hypercube);
    
    return (permutedTensorialOrdinal * getNodeCount(_shardsBaseTopology)) + permutedShardsOrdinal;
  }
}

/** \brief  Get the subcell of dimension scdim with ordinal scord.
 *  \param  scdim        [in]
 *  \param  scord        [in]
 *  For tensor-product topologies T x L (L being the line topology), there are two "copies" of T, T0 and T1,
 *  and the enumeration of subcells of dimension d goes as follows:
    - d-dimensional subcells from T0
    - d-dimensional subcells from T1
    - ((d-1)-dimensional subcells of T) x L.
 */
CellTopoPtr CellTopology::getSubcell( unsigned scdim, unsigned scord ) const {
  if (_tensorialDegree==0) {
    return cellTopology(_shardsBaseTopology.getCellTopologyData(scdim, scord));
  } else {
    CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
    unsigned componentSubcellCount = tensorComponentTopo->getSubcellCount(scdim);
    if (scord < componentSubcellCount * 2) {
      scord = scord % componentSubcellCount;
      return tensorComponentTopo->getSubcell(scdim, scord);
    }
    // otherwise, the subcell is a tensor product of one of the components (scdim-1)-dimensional subcells with the line topology.
    scord = scord - componentSubcellCount *2;
    scdim = scdim - 1;
    CellTopoPtr subcellTensorComponent = tensorComponentTopo->getSubcell(scdim, scord);
    return lineTensorTopology(subcellTensorComponent);
  }
}

void CellTopology::initializeNodes(const std::vector<Intrepid::FieldContainer<double> > &tensorComponentNodes, Intrepid::FieldContainer<double> &cellNodes) {
  if (cellNodes.dimension(0) != this->getNodeCount()) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "cellNodes.dimension(0) != this->getNodeCount()");
  }
  if (cellNodes.dimension(1) != this->getDimension()) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "cellNodes.dimension(1) != this->getDimension()");
  }
  if (tensorComponentNodes.size() != this->getTensorialDegree() + 1) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensorComponentNodes.size() != this->getTensorialDegree() + 1");
  }
  // nodes are ordered as they are in the shards topology, but then repeated for each tensor component choice
  // i.e. if there are N nodes in the shards topology, the first N nodes here will be those with the 0-index of each tensorComponent node selected
  //      Will the next N be the (1,0,0,...,0) or the (0,0,0,...,1)?  I think the consistent choice is the former...
  if (tensorComponentNodes[0].dimension(0) != 2) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensor components beyond the first must have 2 nodes specified!");
  }
  if (tensorComponentNodes[0].dimension(1) != 1) {
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensor components must be one-dimensional!");
  }
  for (int degreeOrdinal=1; degreeOrdinal<tensorComponentNodes.size(); degreeOrdinal++) {
    if (tensorComponentNodes[degreeOrdinal].dimension(0) != 2) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensor components beyond the first must have 2 nodes specified!");
    }
    if (tensorComponentNodes[degreeOrdinal].dimension(1) != 1) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensor components must be one-dimensional!");
    }
  }
  
  if (_tensorialDegree==0) {
    cellNodes = tensorComponentNodes[0];
  } else {
    // note that the construction of tensorComponentTopo here does assume that all the tensorial components
    // after _shardsBaseTopology are lines, but the code that follows does not.  (The argument checks above
    // do also require that the tensorial components are lines.)
    CellTopoPtr tensorComponentTopo = CellTopology::cellTopology(_shardsBaseTopology, _tensorialDegree - 1);
    Intrepid::FieldContainer<double> componentCellNodes(tensorComponentTopo->getNodeCount(),tensorComponentTopo->getDimension());
    
    std::vector<Intrepid::FieldContainer<double> > fewerNodes = tensorComponentNodes;
    Intrepid::FieldContainer<double> lastNodes = fewerNodes[fewerNodes.size()-1];
    fewerNodes.pop_back();
    
    tensorComponentTopo->initializeNodes(fewerNodes, componentCellNodes);
    
    // now the component nodes get stacked atop each other
    int nodeOrdinal=0;
    for (int lastNodesOrdinal=0; lastNodesOrdinal<lastNodes.dimension(0); lastNodesOrdinal++) {
      for (int componentNodeOrdinal=0; componentNodeOrdinal<componentCellNodes.dimension(0); componentNodeOrdinal++, nodeOrdinal++) {
        // TODO: finish this...
        for (int d=0; d<tensorComponentTopo->getDimension(); d++) {
          cellNodes(nodeOrdinal,d) = componentCellNodes(componentNodeOrdinal,d);
        }
        int dOffset = tensorComponentTopo->getDimension();
        for (int d=0; d<lastNodes.dimension(1); d++) {
          cellNodes(nodeOrdinal,d+dOffset) = lastNodes(lastNodesOrdinal,d);
        }
      }
    }
  }
}

bool CellTopology::isHypercube() const {
  unsigned baseKey = _shardsBaseTopology.getBaseKey();
  return (baseKey==shards::Node::key) || (baseKey==shards::Line<2>::key) || (baseKey==shards::Quadrilateral<4>::key) || (baseKey==shards::Hexahedron<8>::key);
}

CellTopoPtr CellTopology::cellTopology(const shards::CellTopology &shardsCellTopo) {
  return cellTopology(shardsCellTopo, 0);
}

CellTopoPtr CellTopology::cellTopology(const shards::CellTopology &shardsCellTopo, unsigned tensorialDegree) {
  unsigned shardsKey = shardsCellTopo.getBaseKey();
  pair<unsigned,unsigned> key = std::make_pair(shardsKey, tensorialDegree);
  if (_tensorizedTrilinosTopologies.find(key) == _tensorizedTrilinosTopologies.end()) {
    _tensorizedTrilinosTopologies[key] = Teuchos::rcp( new CellTopology(shardsCellTopo, tensorialDegree));
  }
  return _tensorizedTrilinosTopologies[key];
}

CellTopoPtr CellTopology::lineTensorTopology(CellTopoPtr camelliaCellTopo) {
  return cellTopology(camelliaCellTopo->getShardsTopology(), camelliaCellTopo->getTensorialDegree() + 1);
}

CellTopoPtr CellTopology::point() {
  static CellTopoPtr node = cellTopology(shards::getCellTopologyData<shards::Node >());
  return node;
}

CellTopoPtr CellTopology::line() {
  static CellTopoPtr line = cellTopology(shards::getCellTopologyData<shards::Line<2> >());
  return line;
}

CellTopoPtr CellTopology::quad() {
  static CellTopoPtr quad = cellTopology(shards::getCellTopologyData<shards::Quadrilateral<4> >());
  return quad;
}

CellTopoPtr CellTopology::hexahedron() {
  static CellTopoPtr hex = cellTopology(shards::getCellTopologyData<shards::Hexahedron<8> >() );
  return hex;
}

CellTopoPtr CellTopology::triangle() {
  static CellTopoPtr triangle = cellTopology(shards::getCellTopologyData<shards::Triangle<3> >());
  return triangle;
}

CellTopoPtr CellTopology::tetrahedron() {
  static CellTopoPtr tet = cellTopology(shards::getCellTopologyData<shards::Tetrahedron<4> >());
  return tet;
}