//
//  MeshPartitionPolicy.h
//  Camellia
//
//  Created by Nathan Roberts on 11/18/11.
//  Copyright 2011 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_MeshPartitionPolicy_h
#define Camellia_MeshPartitionPolicy_h

// Intrepid includes
#include "Intrepid_FieldContainer.hpp"

class MeshPartitionPolicy;
typedef Teuchos::RCP<MeshPartitionPolicy> MeshPartitionPolicyPtr;

using namespace Intrepid;

#include "Mesh.h"

class MeshPartitionPolicy {
public:
  virtual ~MeshPartitionPolicy() {}
  virtual void partitionMesh(Mesh *mesh, PartitionIndexType numPartitions);
  
  static MeshPartitionPolicyPtr standardPartitionPolicy(); // aims to balance across all MPI ranks; present implementation uses Zoltan
  static MeshPartitionPolicyPtr oneRankPartitionPolicy(int rank=0); // all cells belong to the rank specified
};

#endif
