#ifndef DPG_RHS
#define DPG_RHS

// @HEADER
//
// Copyright © 2011 Sandia Corporation. All Rights Reserved.
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
 *  RHS.h
 *
 *  Created by Nathan Roberts on 6/27/11.
 *
 */

#include "Intrepid_FieldContainer.hpp"

#include "BilinearForm.h" // defines EOperatorExtended

#include "BasisCache.h"

#include <vector>

using namespace std;

using namespace Intrepid;

class RHS {
public:
  virtual bool nonZeroRHS(int testVarID) = 0;
  virtual vector<EOperatorExtended> operatorsForTestID(int testID) {
    vector<EOperatorExtended> ops;
    ops.push_back(IntrepidExtendedTypes::OPERATOR_VALUE);
    return ops;
  }
  virtual void rhs(int testVarID, int operatorIndex, Teuchos::RCP<BasisCache> basisCache, FieldContainer<double> &values) {
    rhs(testVarID, operatorIndex, basisCache->getPhysicalCubaturePoints(), values);
  }
  virtual void rhs(int testVarID, int operatorIndex, const FieldContainer<double> &physicalPoints, FieldContainer<double> &values) {
    TEST_FOR_EXCEPTION(operatorIndex != 0, std::invalid_argument, "base rhs() method called for operatorIndex != 0");
    rhs(testVarID,physicalPoints,values);
  }
  virtual void rhs(int testVarID, const FieldContainer<double> &physicalPoints, FieldContainer<double> &values) {
    TEST_FOR_EXCEPTION(true, std::invalid_argument, "no rhs() implemented within RHS");
  }
  // physPoints (numCells,numPoints,spaceDim)
  // values: either (numCells,numPoints) or (numCells,numPoints,spaceDim)
};

#endif