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
  virtual void partitionMesh(Mesh *mesh, int numPartitions, FieldContainer<int> &partitionedActiveCells);
};

#endif
