//
//  VarFactory.h
//  Camellia
//
//  Created by Nathan Roberts on 3/30/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#ifndef Camellia_VarFactory_h
#define Camellia_VarFactory_h

#include "Var.h"
#include "BilinearForm.h"

class VarFactory {
  map< string, VarPtr > _testVars;
  map< string, VarPtr > _trialVars;
  map< int, VarPtr > _testVarsByID;
  map< int, VarPtr > _trialVarsByID;
  int _nextTrialID;
  int _nextTestID;
  
  int getTestID(int IDarg) {
    if (IDarg == -1) {
      IDarg = _nextTestID++;
    } else {
      _nextTestID = max(IDarg + 1, _nextTestID); // ensures that automatically assigned IDs don't conflict with manually assigned ones…
    }
    return IDarg;
  }
  
  int getTrialID(int IDarg) {
    if (IDarg == -1) {
      IDarg = _nextTrialID++;
    } else {
      _nextTrialID = max(IDarg + 1, _nextTrialID); // ensures that automatically assigned IDs don't conflict with manually assigned ones…
    }
    return IDarg;
  }
protected:
  VarFactory(const map< string, VarPtr > &trialVars, const map< string, VarPtr > &testVars,
             const map< int, VarPtr > &trialVarsByID, const map< int, VarPtr > &testVarsByID,
             int nextTrialID, int nextTestID) {
    _trialVars = trialVars;
    _testVars = testVars;
    _trialVarsByID = trialVarsByID;
    _testVarsByID = testVarsByID;
    _nextTrialID = nextTrialID;
    _nextTestID = nextTestID;
  }
public:
  enum BubnovChoice { BUBNOV_TRIAL, BUBNOV_TEST };
  
  VarFactory() {
    _nextTestID = 0;
    _nextTrialID = 0;
  }
  VarFactory getBubnovFactory(BubnovChoice choice) {
    VarFactory factory;
    if (choice == BUBNOV_TRIAL) {
      factory = VarFactory(_trialVars, _trialVars, _trialVarsByID, _trialVarsByID, _nextTrialID, _nextTrialID);
    } else {
      factory = VarFactory(_testVars, _testVars, _testVarsByID, _testVarsByID, _nextTestID, _nextTestID);
    }
    return factory;
  }
  // accessors:
  VarPtr test(int testID) {
    return _testVarsByID[testID];
  }
  VarPtr trial(int trialID) {
    return _trialVarsByID[trialID];
  }
  
  vector<int> testIDs() {
    vector<int> testIDs;
    for ( map< int, VarPtr >::iterator testIt = _testVarsByID.begin();
         testIt != _testVarsByID.end(); testIt++) {
      testIDs.push_back( testIt->first );
    }
    return testIDs;
  }
  
  vector<int> trialIDs() {
    vector<int> trialIDs;
    for ( map< int, VarPtr >::iterator trialIt = _trialVarsByID.begin();
         trialIt != _trialVarsByID.end(); trialIt++) {
      trialIDs.push_back( trialIt->first );
    }
    return trialIDs;
  }
  
  // when there are other scratchpads (e.g. BilinearFormScratchPad), we'll want to share
  // the variables.  The basic function of the factory is to assign unique test/trial IDs.
  
  VarPtr testVar(string name, Space fs, int ID = -1) {
    if ( _testVars.find(name) != _testVars.end() ) {
      return _testVars[name];
    }
    
    ID = getTestID(ID);
    int rank = ((fs == HGRAD) || (fs == L2) || (fs == CONSTANT_SCALAR)) ? 0 : 1;
    
    _testVars[name] = Teuchos::rcp( new Var( ID, rank, name, 
                                             IntrepidExtendedTypes::OP_VALUE, fs, TEST) );
    _testVarsByID[ID] = _testVars[name];
    return _testVarsByID[ID];
  }
  VarPtr fieldVar(string name, Space fs = L2, int ID = -1) {
    if (_trialVars.find(name) != _trialVars.end()) {
      return _trialVars[name];
    }
    ID = getTrialID(ID);
    int rank = ((fs == HGRAD) || (fs == L2)) ? 0 : 1;
    _trialVars[name] = Teuchos::rcp( new Var( ID, rank, name,
                                              IntrepidExtendedTypes::OP_VALUE, fs, FIELD) );
    _trialVarsByID[ID] = _trialVars[name];
    return _trialVarsByID[ID];
  }
  VarPtr fluxVar(string name, Space fs = L2, int ID = -1) { // trace of HDIV  (implemented as L2 on boundary)
    if (_trialVars.find(name) != _trialVars.end()) {
      return _trialVars[name];
    }
    int rank = 0;
    ID = getTrialID(ID);
    _trialVars[name] = Teuchos::rcp( new Var( ID, rank, name, 
                                              IntrepidExtendedTypes::OP_VALUE, fs, FLUX) );
    _trialVarsByID[ID] = _trialVars[name];
    return _trialVarsByID[ID];
  }
  VarPtr traceVar(string name, Space fs = HGRAD, int ID = -1) { // trace of HGRAD (implemented as HGRAD on boundary)
    if (_trialVars.find(name) != _trialVars.end() ) {
      return _trialVars[name];
    }
    int rank = 0;
    ID = getTrialID(ID);
    _trialVars[name] = Teuchos::rcp( new Var( ID, rank, name, 
                                              IntrepidExtendedTypes::OP_VALUE, fs, TRACE) );
    _trialVarsByID[ID] = _trialVars[name];
    return _trialVarsByID[ID];
  }
  
  const map< int, VarPtr > & testVars() {
    return _testVarsByID;
  }
  
  const map< int, VarPtr > & trialVars() {
    return _trialVarsByID;
  }
};

#endif
