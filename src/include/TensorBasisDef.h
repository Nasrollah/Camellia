//
//  TensorBasisDef.h
//  Camellia
//
//  Created by Nathan Roberts on 11/12/14.
//
//

#include "CamelliaCellTools.h"

#define TENSOR_FIELD_ORDINAL(spaceFieldOrdinal,timeFieldOrdinal) timeFieldOrdinal * _spatialBasis->getCardinality() + spaceFieldOrdinal
#define TENSOR_DOF_OFFSET_ORDINAL(spaceDofOffsetOrdinal,timeDofOffsetOrdinal,spaceDofsForSubcell) timeDofOffsetOrdinal * spaceDofsForSubcell + spaceDofOffsetOrdinal

namespace Camellia {
  template<class Scalar, class ArrayScalar>
  TensorBasis<Scalar,ArrayScalar>::TensorBasis(Teuchos::RCP< Camellia::Basis<Scalar,ArrayScalar> > spatialBasis, Teuchos::RCP< Camellia::Basis<Scalar,ArrayScalar> > temporalBasis) {
    _spatialBasis = spatialBasis;
    _temporalBasis = temporalBasis;
    
    if (temporalBasis->domainTopology()->getDimension() != 1) {
      cout << "temporalBasis must have a line topology as its domain.\n";
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "temporalBasis must have a line topology as its domain.");
    }
    
    if (temporalBasis->rangeRank() > 0) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "only scalar temporal bases are supported by TensorBasis at present.");
    }
    
    if (_spatialBasis->domainTopology()->getTensorialDegree() > 0) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "only spatial bases defined on toplogies with 0 tensorial degree are supported by TensorBasis at present.");
    }
    
    if (_spatialBasis->domainTopology()->getDimension() == 0) {
      this->_functionSpace = _temporalBasis->functionSpace();
    } else if (_spatialBasis->functionSpace() == _temporalBasis->functionSpace()) {
      // I doubt this would be right if the function spaces were HDIV or HCURL, but
      // for HVOL AND HGRAD, it does hold, and since our temporal bases are always defined
      // on line topologies, HVOL and HGRAD are the only ones for which we'll use this...
      this->_functionSpace = _spatialBasis->functionSpace();
    } else {
      this->_functionSpace = FUNCTION_SPACE_UNKNOWN;
    }
    
    int tensorialDegree = 1;
    this->_domainTopology = CellTopology::cellTopology(_spatialBasis->domainTopology()->getShardsTopology(), tensorialDegree);
  }

  template<class Scalar, class ArrayScalar>
  int TensorBasis<Scalar,ArrayScalar>::getCardinality() const {
    return _spatialBasis->getCardinality() * _temporalBasis->getCardinality();
  }

  template<class Scalar, class ArrayScalar>
  const Teuchos::RCP< Camellia::Basis<Scalar, ArrayScalar> > TensorBasis<Scalar, ArrayScalar>::getComponentBasis(int tensorialBasisRank) const {
    if (tensorialBasisRank==0) return _spatialBasis;
    if (tensorialBasisRank==1) return _temporalBasis;
    
    TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "tensorialBasisRank exceeds the tensorial degree of basis.");
  }

  template<class Scalar, class ArrayScalar>
  int TensorBasis<Scalar,ArrayScalar>::getDegree() const {
    return _spatialBasis->getDegree();
  }
  
  template<class Scalar, class ArrayScalar>
  const Teuchos::RCP< Camellia::Basis<Scalar, ArrayScalar> > TensorBasis<Scalar, ArrayScalar>::getSpatialBasis() const {
    return this->getComponentBasis(0);
  }
  
  template<class Scalar, class ArrayScalar>
  const Teuchos::RCP< Camellia::Basis<Scalar, ArrayScalar> > TensorBasis<Scalar, ArrayScalar>::getTemporalBasis() const {
    return this->getComponentBasis(1);
  }

  template<class Scalar, class ArrayScalar>
  void TensorBasis<Scalar,ArrayScalar>::getTensorValues(ArrayScalar& outputValues, std::vector< const ArrayScalar> & componentOutputValuesVector, std::vector<Intrepid::EOperator> operatorTypes) const {
    // outputValues can have dimensions (C,F,P,...) or (F,P,...)
    
    if (operatorTypes.size() != 2) {
      TEUCHOS_TEST_FOR_EXCEPTION(true,std::invalid_argument, "only two-component tensor bases supported right now");
    }
    
    Intrepid::EOperator spatialOperator = operatorTypes[0];
    int rankAdjustment;
    switch (spatialOperator) {
      case(OPERATOR_VALUE):
        rankAdjustment = 0;
        break;
      case(OPERATOR_GRAD):
        rankAdjustment = 1;
        break;
      case(OPERATOR_CURL):
        if (this->rangeRank() == 1)
        break;
      case(OPERATOR_DIV):
        rankAdjustment = -1;
        break;
      default:
        TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unhandled operator type.");
    }
    
    int valuesRank = this->rangeRank() + rankAdjustment; // should have 2 additional dimensions, plus optionally the cell dimension
    int fieldIndex;
    if (outputValues.rank() == valuesRank + 3) {
      fieldIndex = 1; // (C,F,...)
    } else if (outputValues.rank() == valuesRank + 2) {
      fieldIndex = 0; // (F,...)
    } else {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "outputValues shape must be either (C,F,P,...) or (F,P,...)");
    }
    
    const ArrayScalar* spatialValues = &componentOutputValuesVector[0];
    const ArrayScalar* temporalValues = &componentOutputValuesVector[1];
    
    //cout << "componentOutputValues: \n" << componentOutputValues;
    TEUCHOS_TEST_FOR_EXCEPTION( outputValues.dimension(fieldIndex) != this->getCardinality(),
                               std::invalid_argument, "outputValues.dimension(fieldIndex) != this->getCardinality()");
    TEUCHOS_TEST_FOR_EXCEPTION( spatialValues->dimension(fieldIndex) != _spatialBasis->getCardinality(),
                               std::invalid_argument, "spatialValues->dimension(fieldIndex) != _spatialBasis->getCardinality()");
    TEUCHOS_TEST_FOR_EXCEPTION( temporalValues->dimension(fieldIndex) != _temporalBasis->getCardinality(),
                               std::invalid_argument, "temporalValues->dimension(fieldIndex) != _temporalBasis->getCardinality()");
    int pointIndex = fieldIndex+1;
    TEUCHOS_TEST_FOR_EXCEPTION( outputValues.dimension(pointIndex) != spatialValues->dimension(pointIndex) * temporalValues->dimension(pointIndex),
                               std::invalid_argument, "outputValues.dimension(pointIndex) != spatialValues->dimension(pointIndex) * temporalValues->dimension(pointIndex)");
    Teuchos::Array<int> dimensions;
    outputValues.dimensions(dimensions);
    outputValues.initialize(0.0);
    int numPoints = dimensions[fieldIndex+1];
    int numComponents = dimensions[fieldIndex+2];
    // TODO: finish writing this...
    /*
    if (_numComponents != numComponents) {
      TEUCHOS_TEST_FOR_EXCEPTION ( _numComponents != numComponents, std::invalid_argument,
                                  "fieldIndex+2 dimension of outputValues must match the number of vector components.");
    }
    int componentCardinality = _componentBasis->getCardinality();
    
    for (int i=fieldIndex+3; i<dimensions.size(); i++) {
      dimensions[i] = 0;
    }
    
    int numCells = (fieldIndex==1) ? dimensions[0] : 1;
    
    int compValuesPerPoint = componentOutputValues.size() / (numCells * componentCardinality * numPoints);
    
    for (int cellIndex=0; cellIndex<numCells; cellIndex++) {
      if (fieldIndex==1) {
        dimensions[0] = cellIndex;
      }
      for (int field=0; field<componentCardinality; field++) {
        for (int ptIndex=0; ptIndex<numPoints; ptIndex++) {
          dimensions[fieldIndex+1] = ptIndex;
          dimensions[fieldIndex+2] = 0; // component dimension
          Teuchos::Array<int> compIndexArray = dimensions;
          compIndexArray.pop_back(); // removes the last 0 (conceptually it's fieldIndex + 2 that corresponds to the comp dimension, but it's all 0s from fieldIndex+2 on...)
          compIndexArray[fieldIndex] = field;
          int compEnumerationOffset = componentOutputValues.getEnumeration(compIndexArray);
          for (int comp=0; comp<numComponents; comp++) {
            dimensions[fieldIndex] = field + componentCardinality * comp;
            dimensions[fieldIndex+2] = comp;
            int outputEnumerationOffset = outputValues.getEnumeration(dimensions);
            const double *compValue = &componentOutputValues[compEnumerationOffset];
            double *outValue = &outputValues[outputEnumerationOffset];
            for (int i=0; i<compValuesPerPoint; i++) {
              *outValue++ = *compValue++;
            }
          }
        }
      }
    }*/
    //    cout << "getVectorizedValues: componentOutputValues:\n" << componentOutputValues;
    //    cout << "getVectorizedValues: outputValues:\n" << outputValues;
  }

  // range info for basis values:
  template<class Scalar, class ArrayScalar>
  int TensorBasis<Scalar,ArrayScalar>::rangeDimension() const {
    // two possibilities:
    // 1. We have a temporal basis which we won't take the derivative of--because it's in HVOL.  Then:
    if (_temporalBasis->functionSpace() == Camellia::FUNCTION_SPACE_HVOL) {
      return _spatialBasis->rangeDimension();
    } else {
      // 2. We can take derivatives of the temporal basis.  Then we sum the two range dimensions:
      return _spatialBasis->rangeDimension() + _temporalBasis->rangeDimension();
    }
  }
  
  template<class Scalar, class ArrayScalar>
  int TensorBasis<Scalar,ArrayScalar>::rangeRank() const {
    return _spatialBasis->rangeRank();
  }
  
  template<class Scalar, class ArrayScalar>
  void TensorBasis<Scalar,ArrayScalar>::getValues(ArrayScalar &values, const ArrayScalar &refPoints,
                                                  Intrepid::EOperator operatorType) const {
    Intrepid::EOperator temporalOperator = OPERATOR_VALUE; // default
    if (operatorType==OPERATOR_GRAD) {
      if (values.rank() == 3) { // F, P, D
        if (_spatialBasis->rangeDimension() + _temporalBasis->rangeDimension() == values.dimension(2)) {
          // then we can/should use OPERATOR_GRAD for the temporal operator as well
          temporalOperator = OPERATOR_GRAD;
        }
      }
    }
    getValues(values, refPoints, operatorType, temporalOperator);
  }
  
  template<class Scalar, class ArrayScalar>
  void TensorBasis<Scalar,ArrayScalar>::getValues(ArrayScalar &values, const ArrayScalar &refPoints,
                                                  Intrepid::EOperator spatialOperatorType, Intrepid::EOperator temporalOperatorType) const {
    bool gradInBoth = (spatialOperatorType==OPERATOR_GRAD) && (temporalOperatorType==OPERATOR_GRAD);
    this->CHECK_VALUES_ARGUMENTS(values,refPoints,spatialOperatorType);
    
    int numPoints = refPoints.dimension(0);
    
    Teuchos::Array<int> spaceTimePointsDimensions;
    refPoints.dimensions(spaceTimePointsDimensions);
    
    spaceTimePointsDimensions[1] -= 1; // spatial dimension is 1 less than space-time
    if (spaceTimePointsDimensions[1]==0) spaceTimePointsDimensions[1] = 1; // degenerate case: we still want points defined in the 0-dimensional case...
    ArrayScalar refPointsSpatial(spaceTimePointsDimensions);
    
    spaceTimePointsDimensions[1] = 1; // time is 1-dimensional
    ArrayScalar refPointsTemporal(spaceTimePointsDimensions);
    
    int spaceDim = _spatialBasis->domainTopology()->getDimension();
    // copy the points out:
    for (int pointOrdinal=0; pointOrdinal<numPoints; pointOrdinal++) {
      for (int d=0; d<spaceDim; d++) {
        refPointsSpatial(pointOrdinal,d) = refPoints(pointOrdinal,d);
      }
      refPointsTemporal(pointOrdinal,0) = refPoints(pointOrdinal,spaceDim);
    }
    
    Teuchos::Array<int> valuesDim; // will use to size our space and time values arrays
    values.dimensions(valuesDim); // F, P[,D,...]
    
    int valuesPerPointSpace = 1;
    for (int d=2; d<valuesDim.size(); d++) { // for vector and tensor-valued bases, take the spatial range dimension in each tensorial rank
      valuesDim[d] = max(_spatialBasis->rangeDimension(), 1); // ensure that for 0-dimensional topologies, we still define a value
      valuesPerPointSpace *= valuesDim[d];
    }
    valuesDim[0] = _spatialBasis->getCardinality(); // field dimension
    ArrayScalar spatialValues(valuesDim);
    _spatialBasis->getValues(spatialValues, refPointsSpatial, spatialOperatorType);
    
    ArrayScalar temporalValues(_temporalBasis->getCardinality(), numPoints);
    if (temporalOperatorType==OPERATOR_GRAD) {
      temporalValues.resize(_temporalBasis->getCardinality(), numPoints, _temporalBasis->rangeDimension());
    }
    _temporalBasis->getValues(temporalValues, refPointsTemporal, temporalOperatorType);
    
    ArrayScalar spatialValues_opValue;
    ArrayScalar temporalValues_opValue;
    if (gradInBoth) {
      spatialValues_opValue.resize(_spatialBasis->getCardinality(), numPoints);
      temporalValues_opValue.resize(_temporalBasis->getCardinality(), numPoints);
      _spatialBasis->getValues(spatialValues_opValue, refPointsSpatial, OPERATOR_VALUE);
      _temporalBasis->getValues(temporalValues_opValue, refPointsTemporal, OPERATOR_VALUE);
    }
    
//    cout << "refPointsTemporal:\n" << refPointsTemporal;
    
//    cout << "spatialValues:\n" << spatialValues;
//    cout << "temporalValues:\n" << temporalValues;
    
    Teuchos::Array<int> spaceTimeValueCoordinate(valuesDim.size(), 0);
    Teuchos::Array<int> spatialValueCoordinate(valuesDim.size(), 0);
    
    // combine values:
    for (int spaceFieldOrdinal=0; spaceFieldOrdinal<_spatialBasis->getCardinality(); spaceFieldOrdinal++) {
      spatialValueCoordinate[0] = spaceFieldOrdinal;
      for (int timeFieldOrdinal=0; timeFieldOrdinal<_temporalBasis->getCardinality(); timeFieldOrdinal++) {
        int spaceTimeFieldOrdinal = TENSOR_FIELD_ORDINAL(spaceFieldOrdinal, timeFieldOrdinal);
        spaceTimeValueCoordinate[0] = spaceTimeFieldOrdinal;
        for (int pointOrdinal=0; pointOrdinal<numPoints; pointOrdinal++) {
          spaceTimeValueCoordinate[1] = pointOrdinal;
          spatialValueCoordinate[1] = pointOrdinal;
          double temporalValue;
          if (temporalOperatorType!=OPERATOR_GRAD)
            temporalValue = temporalValues(timeFieldOrdinal,pointOrdinal);
          else
            temporalValue = temporalValues(timeFieldOrdinal,pointOrdinal, 0);
          int spatialValueEnumeration = spatialValues.getEnumeration(spatialValueCoordinate);
          
          if (! gradInBoth) {
            int spaceTimeValueEnumeration = values.getEnumeration(spaceTimeValueCoordinate);
            for (int offset=0; offset<valuesPerPointSpace; offset++) {
              double spatialValue = spatialValues[spatialValueEnumeration+offset];
              values[spaceTimeValueEnumeration+offset] = spatialValue * temporalValue;
            }
          } else {
            double spatialValue_opValue = spatialValues_opValue(spaceFieldOrdinal,pointOrdinal);
            double temporalValue_opValue = temporalValues_opValue(timeFieldOrdinal,pointOrdinal);
            
            // product rule: first components are spatial gradient times temporal value; next components are spatial value times temporal gradient
            // first, handle spatial gradients
            spaceTimeValueCoordinate[2] = 0;
            int spaceTimeValueEnumeration = values.getEnumeration(spaceTimeValueCoordinate);
            for (int offset=0; offset<valuesPerPointSpace; offset++) {
              double spatialGradValue = spatialValues[spatialValueEnumeration+offset];
              double spaceTimeValue = spatialGradValue * temporalValue_opValue;

              values[spaceTimeValueEnumeration+offset] = spaceTimeValue;
            }

            // next, temporal gradients
            spaceTimeValueCoordinate[2] = _spatialBasis->rangeDimension();
            spaceTimeValueEnumeration = values.getEnumeration(spaceTimeValueCoordinate);
            
            double temporalGradValue = temporalValues(timeFieldOrdinal,pointOrdinal,0);
            double spaceTimeValue = spatialValue_opValue * temporalGradValue;

            values[spaceTimeValueEnumeration] = spaceTimeValue;
          }
        }
      }
    }
  }
  
  template<class Scalar, class ArrayScalar>
  int TensorBasis<Scalar, ArrayScalar>::getDofOrdinalFromComponentDofOrdinals(std::vector<int> componentDofOrdinals) const {
    if (componentDofOrdinals.size() != 2) {
      TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "componentDofOrdinals.size() must equal 2, in present implementation");
    }
    int spaceFieldOrdinal = componentDofOrdinals[0];
    int timeFieldOrdinal = componentDofOrdinals[1];
    return TENSOR_FIELD_ORDINAL(spaceFieldOrdinal, timeFieldOrdinal);
  }

  template<class Scalar, class ArrayScalar>
  void TensorBasis<Scalar,ArrayScalar>::initializeTags() const {
    const std::vector<std::vector<std::vector<int> > > spatialTagToOrdinal = _spatialBasis->getDofOrdinalData();
    const std::vector<std::vector<std::vector<int> > > temporalTagToOrdinal = _temporalBasis->getDofOrdinalData();
    
    const std::vector<std::vector<int> > spatialOrdinalToTag = _spatialBasis->getAllDofTags();
    const std::vector<std::vector<int> > temporalOrdinalToTag = _temporalBasis->getAllDofTags();
    
    int tagSize = 4;
    int posScDim = 0;        // position in the tag, counting from 0, of the subcell dim
    int posScOrd = 1;        // position in the tag, counting from 0, of the subcell ordinal
    int posDfOrd = 2;        // position in the tag, counting from 0, of DoF ordinal relative to the subcell
    int posDfCnt = 2;        // position in the tag, counting from 0, of DoF ordinal count for subcell
    
    std::vector<int> tags( tagSize * this->getCardinality() ); // flat array
    
    CellTopoPtr spaceTimeTopo = this->domainTopology();
    
    int sideDim = spaceTimeTopo->getDimension() - 1;
    
    for (int spaceFieldOrdinal=0; spaceFieldOrdinal<_spatialBasis->getCardinality(); spaceFieldOrdinal++) {
      std::vector<int> spaceTagData = spatialOrdinalToTag[spaceFieldOrdinal];
      unsigned spaceSubcellDim = spaceTagData[posScDim];
      unsigned spaceSubcellOrd = spaceTagData[posScOrd];
      for (int timeFieldOrdinal=0; timeFieldOrdinal<_temporalBasis->getCardinality(); timeFieldOrdinal++) {
        std::vector<int> timeTagData = temporalOrdinalToTag[timeFieldOrdinal];
        unsigned timeSubcellDim = timeTagData[posScDim];
        unsigned timeSubcellOrd = timeTagData[posScOrd];
        
        unsigned spaceTimeSubcellDim = spaceSubcellDim + timeSubcellDim;
        unsigned spaceTimeSubcellOrd;
        if (timeSubcellDim == 0) {
          // vertex node in time; the subcell is not extruded in time but belongs to one of the two "copies"
          // of the spatial topology
          unsigned spaceTimeSideOrdinal = this->domainTopology()->getTemporalComponentSideOrdinal(timeSubcellOrd);
          spaceTimeSubcellOrd = CamelliaCellTools::subcellOrdinalMap(spaceTimeTopo, sideDim, spaceTimeSideOrdinal,
                                                                     spaceSubcellDim, spaceSubcellOrd);
        } else {
          // line subcell in time; the subcell *is* extruded in time
          spaceTimeSubcellOrd = spaceTimeTopo->getExtrudedSubcellOrdinal(spaceSubcellDim, spaceSubcellOrd);
          if (spaceTimeSubcellOrd == (unsigned)-1) {
            cout << "ERROR: -1 subcell ordinal.\n";
            spaceTimeSubcellOrd = spaceTimeTopo->getExtrudedSubcellOrdinal(spaceSubcellDim, spaceSubcellOrd);
          }
        }
        
        int i = TENSOR_FIELD_ORDINAL(spaceFieldOrdinal, timeFieldOrdinal);
        int spaceDofOffsetOrdinal = spaceTagData[posDfOrd];
        int timeDofOffsetOrdinal = timeTagData[posDfOrd];
        int spaceDofsForSubcell = spaceTagData[posDfCnt];
        int spaceTimeDofOffsetOrdinal = TENSOR_DOF_OFFSET_ORDINAL(spaceDofOffsetOrdinal, timeDofOffsetOrdinal, spaceDofsForSubcell);
        tags[tagSize*i + posScDim] = spaceTimeSubcellDim; // subcellDim
        tags[tagSize*i + posScOrd] = spaceTimeSubcellOrd; // subcell ordinal
        tags[tagSize*i + posDfOrd] = spaceTimeDofOffsetOrdinal;  // ordinal of the specified DoF relative to the subcell
        tags[tagSize*i + posDfCnt] = spaceTagData[posDfCnt] * timeTagData[posDfCnt];     // total number of DoFs associated with the subcell
      }
    }
    
    // call basis-independent method (declared in IntrepidUtil.hpp) to set up the data structures
    Intrepid::setOrdinalTagData(this -> _tagToOrdinal,
                                this -> _ordinalToTag,
                                &(tags[0]),
                                this -> getCardinality(),
                                tagSize,
                                posScDim,
                                posScOrd,
                                posDfOrd);
  }

} // namespace Camellia