#include "Solution.h"

#include "CamelliaCellTools.h"

#ifdef USE_VTK
#include "vtkPointData.h"
#include "vtkFloatArray.h"
#include "vtkIntArray.h"
#include "vtkUnstructuredGrid.h"
#include "vtkXMLUnstructuredGridWriter.h"
#include "vtkCellType.h"
#include "vtkIdList.h"
#include "vtkVersion.h"

// Write solution to unstructured VTK format
void Solution::writeToVTK(const string& filePath, unsigned int num1DPts)
{
  writeFieldsToVTK(filePath, num1DPts);
  writeTracesToVTK(filePath);
}

// Write field variables to unstructured VTK format
void Solution::writeFieldsToVTK(const string& filePath, unsigned int num1DPts)
{
  bool defaultPts = (num1DPts == 0);

  vtkUnstructuredGrid* ug = vtkUnstructuredGrid::New();
  vector<vtkFloatArray*> fieldData;
  vtkPoints* points = vtkPoints::New();
  vtkIntArray* polyOrderData = vtkIntArray::New();

  // Get trialIDs
  vector<int> trialIDs = _mesh->bilinearForm()->trialIDs();
  vector<int> fieldTrialIDs = _mesh->bilinearForm()->trialVolumeIDs();
  int numFieldVars = fieldTrialIDs.size();

  int spaceDim = 2; // TODO: generalize to 3D...
  int totalCells = _mesh->activeElements().size();
  for (int i=0; i < numFieldVars; i++)
    fieldData.push_back(vtkFloatArray::New());
  // int numFieldVars = 0;
  // for (unsigned int i=0; i < trialIDs.size(); i++)
  // {
  //   if (!(_mesh->bilinearForm()->isFluxOrTrace(trialIDs[i])))
  //   {
  //     numFieldVars++;
  //     fieldTrialIDs.push_back(trialIDs[i]);
  //     fieldData.push_back(vtkFloatArray::New());
  //   }
  // }

  for (int varIdx = 0; varIdx < numFieldVars; varIdx++)
  {
    bool vectorized = Camellia::functionSpaceIsVectorized(_mesh->bilinearForm()->functionSpaceForTrial(fieldTrialIDs[varIdx]));
    if (vectorized)
      fieldData[varIdx]->SetNumberOfComponents(spaceDim);
    else
      fieldData[varIdx]->SetNumberOfComponents(1);
    fieldData[varIdx]->SetName(_mesh->bilinearForm()->trialName(fieldTrialIDs[varIdx]).c_str());
  }
  polyOrderData->SetNumberOfComponents(1);
  polyOrderData->SetName("Polynomial Order");

  vector< ElementTypePtr > elementTypes = _mesh->elementTypes();
  vector< ElementTypePtr >::iterator elemTypeIt;

  unsigned int total_vertices = 0;

  // Loop through Quads, Triangles, etc
  for (elemTypeIt = elementTypes.begin(); elemTypeIt != elementTypes.end(); elemTypeIt++)
  {
    ElementTypePtr elemTypePtr = *(elemTypeIt);
    cellTopo = elemTypePtr->cellTopoPtr;

    FieldContainer<double> vertexPoints;
    _mesh->verticesForElementType(vertexPoints,elemTypePtr); //stores vertex points for this element
    FieldContainer<double> physicalCellNodes = _mesh->physicalCellNodesGlobal(elemTypePtr);

    int numCells = physicalCellNodes.dimension(0);
    bool createSideCacheToo = false;
    BasisCachePtr basisCache = Teuchos::rcp(new BasisCache(elemTypePtr,_mesh, createSideCacheToo));

    vector<GlobalIndexType> cellIDs;
    for (GlobalIndexType cellIndex=0; cellIndex<numCells; cellIndex++)
    {
      GlobalIndexType cellID = _mesh->cellID(elemTypePtr, cellIndex, -1); // -1: global cellID
      cellIDs.push_back(cellID);
    }

    int pOrder = _mesh->cellPolyOrder(cellIDs[0]);

    int numVertices = vertexPoints.dimension(1);
    bool isQuad = (numVertices == 4);
    int numPoints = 0;
    if (defaultPts)
      num1DPts = pow(2.0, pOrder-1);
    if (isQuad)
      numPoints = num1DPts*num1DPts;
    else
      for (int i=1; i <= num1DPts; i++)
        numPoints += i;

    FieldContainer<double> refPoints(numPoints,spaceDim);
    if (isQuad)
    {
      for (int j = 0; j < num1DPts; j++)
        for (int i=0; i < num1DPts; i++)
        {
          int pointIndex = j*num1DPts + i;
          double x = -1.0 + 2.0*(double(i)/double(num1DPts-1));
          double y = -1.0 + 2.0*(double(j)/double(num1DPts-1));
          refPoints(pointIndex,0) = x;
          refPoints(pointIndex,1) = y;
        }
    }
    else
    {
      int pointIndex = 0;
      for (int j = 0; j < num1DPts; j++)
        for (int i=0; i < num1DPts-j; i++)
        {
          double x = (double(i)/double(num1DPts-1));
          double y = (double(j)/double(num1DPts-1));
          refPoints(pointIndex,0) = x;
          refPoints(pointIndex,1) = y;
          pointIndex++;
        }
    }

    basisCache->setRefCellPoints(refPoints);
    basisCache->setPhysicalCellNodes(physicalCellNodes, cellIDs, createSideCacheToo);
    const FieldContainer<double> *physicalPoints = &basisCache->getPhysicalCubaturePoints();

    vector< FieldContainer<double> > computedValues;
    computedValues.resize(numFieldVars);
    for (int i=0; i < numFieldVars; i++)
    {
      int numberComponents = fieldData[i]->GetNumberOfComponents();
      // cout << "field var " << i << " " << numFieldVars << endl;
      if (numberComponents == 1)
        computedValues[i].resize(numCells, numPoints);
      else
        computedValues[i].resize(numCells, numPoints, spaceDim);
      // cout << "finished resizing" << endl;
      solutionValues(computedValues[i], fieldTrialIDs[i], basisCache);
    }
    // cout << "After All" << endl;

    for (GlobalIndexType cellIndex=0; cellIndex<numCells; cellIndex++ )
    {
      int subcellStartIndex = total_vertices;
      if (isQuad)
      {
        for (int j=0; j < num1DPts-1; j++)
        {
          for (int i=0; i < num1DPts-1; i++)
          {
            int ind1 = subcellStartIndex;
            int ind2 = ind1 + 1;
            int ind3 = ind2 + num1DPts;
            int ind4 = ind1 + num1DPts;
            vtkIdType subCell[4] = {
              ind1, ind2, ind3, ind4};
            ug->InsertNextCell((int)VTK_QUAD, 4, subCell);
            polyOrderData->InsertNextValue(pOrder-1);

            subcellStartIndex++;
          }
          subcellStartIndex++;
        }
      }
      else
      {
        for (int j=0; j < num1DPts-1; j++)
        {
          for (int i=0; i < num1DPts-1-j; i++)
          {
            int ind1 = subcellStartIndex;
            int ind2 = ind1 + 1;
            int ind3 = ind1 + num1DPts-j;
            vtkIdType subCell[3] = {
              ind1, ind2, ind3};
            ug->InsertNextCell((int)VTK_TRIANGLE, 3, subCell);
            polyOrderData->InsertNextValue(pOrder-1);

            if (i < num1DPts-2-j)
            {
              int ind1 = subcellStartIndex+1;
              int ind2 = ind1 + num1DPts - j;
              int ind3 = ind1 + num1DPts -j - 1;
              vtkIdType subCell[3] = {
                ind1, ind2, ind3};
              ug->InsertNextCell((int)VTK_TRIANGLE, 3, subCell);
              polyOrderData->InsertNextValue(pOrder-1);
            }

            subcellStartIndex++;
          }
          subcellStartIndex++;
        }
      }

      for (int pointIndex = 0; pointIndex < numPoints; pointIndex++)
      {
        points->InsertNextPoint((*physicalPoints)(cellIndex, pointIndex, 0),
                                (*physicalPoints)(cellIndex, pointIndex, 1), 0.0);
        for (int varIdx=0; varIdx < numFieldVars; varIdx++)
        {
          // fieldData[varIdx]->InsertNextValue(computedValues[varIdx](cellIndex, pointIndex));
          switch(fieldData[varIdx]->GetNumberOfComponents())
          {
            case 1:
              fieldData[varIdx]->InsertNextTuple1(computedValues[varIdx](cellIndex, pointIndex));
              break;
            case 2:
              if (cellIndex == 0 && pointIndex == 0)
                // cout << computedValues[varIdx] << endl;
              // fieldData[varIdx]->InsertNextTuple1(computedValues[varIdx](cellIndex, pointIndex));
              fieldData[varIdx]->InsertNextTuple2(computedValues[varIdx](cellIndex, pointIndex, 0), computedValues[varIdx](cellIndex, pointIndex, 1));
              break;
            default:
              TEUCHOS_TEST_FOR_EXCEPTION(true, std::invalid_argument, "Unsupported number of components");
          }
        }
        total_vertices++;
      }
    }
  }

  ug->SetPoints(points);
  points->Delete();

  for (int varIdx=0; varIdx < numFieldVars; varIdx++)
  {
    ug->GetPointData()->AddArray(fieldData[varIdx]);
    fieldData[varIdx]->Delete();
  }
  // ug->GetCellData()->AddArray(polyOrderData);

  vtkXMLUnstructuredGridWriter* wr = vtkXMLUnstructuredGridWriter::New();
#if VTK_MAJOR_VERSION <= 5
  wr->SetInput(ug);
#else
  wr->SetInputData(ug);
#endif
  ug->Delete();
  wr->SetFileName((filePath+".vtu").c_str());
  wr->SetDataModeToBinary();
  wr->Update();
  wr->Write();
  wr->Delete();

  cout << "Wrote Field Variables to " << filePath << ".vtu" << endl;
}

void Solution::writeTracesToVTK(const string& filePath)
{
  vtkUnstructuredGrid* trace_ug = vtkUnstructuredGrid::New();
  vector<vtkFloatArray*> traceData;
  vtkPoints* trace_points = vtkPoints::New();

  // Get trialIDs
  vector<int> trialIDs = _mesh->bilinearForm()->trialIDs();
  vector<int> traceTrialIDs;

  // Allocate memory for VTK unstructured grid
  int totalCells = _mesh->activeElements().size();
  // trace_ug->Allocate(4*totalCells, 4*totalCells);

  // Count trace variables
  int numTraceVars = 0;
  for (unsigned int i=0; i < trialIDs.size(); i++)
  {
    if (_mesh->bilinearForm()->isFluxOrTrace(trialIDs[i]))
    {
      numTraceVars++;
      traceTrialIDs.push_back(trialIDs[i]);
      traceData.push_back(vtkFloatArray::New());
    }
  }
  for (int varIdx = 0; varIdx < numTraceVars; varIdx++)
  {
    traceData[varIdx]->SetNumberOfComponents(1);
    traceData[varIdx]->SetName(_mesh->bilinearForm()->trialName(traceTrialIDs[varIdx]).c_str());
  }
  unsigned int trace_vertex_count = 0;

  BasisCachePtr basisCache;
  vector< ElementTypePtr > elementTypes = _mesh->elementTypes();
  vector< ElementTypePtr >::iterator elemTypeIt;
  for (elemTypeIt = elementTypes.begin(); elemTypeIt != elementTypes.end(); elemTypeIt++)
  {
    ElementTypePtr elemTypePtr = *(elemTypeIt);
    FieldContainer<double> physicalCellNodes = _mesh()->physicalCellNodesGlobal(elemTypePtr);
    CellTopoPtr cellTopo = elemTypePtr->cellTopoPtr;
    // basisCache = Teuchos::rcp( new BasisCache(physicalCellNodes, cellTopo, 1) );
    basisCache = Teuchos::rcp( new BasisCache( elemTypePtr, _mesh ) );
    if (basisCache.get() == NULL)
      cout << "NULL Basis" << endl;
    int numSides = cellTopo->getSideCount();

    int numCells = physicalCellNodes.dimension(0);
    // determine cellIDs
    vector<GlobalIndexType> cellIDs;
    for (int cellIndex=0; cellIndex<numCells; cellIndex++)
    {
      GlobalIndexType cellID = _mesh->cellID(elemTypePtr, cellIndex, -1); // -1: "global" lookup (independent of MPI node)
      cellIDs.push_back(cellID);
    }
    basisCache->setPhysicalCellNodes(physicalCellNodes, cellIDs, true); // true: create side caches

    // int numPoints = 2;
    FieldContainer<double> refTracePoints(3);
    refTracePoints(0) = -1.0;
    refTracePoints(1) =  0.0;
    refTracePoints(2) =  1.0;
    // FieldContainer<double> refTracePoints(2, 1);
    // refTracePoints(0, 0) =  0.0;
    // refTracePoints(1, 0) =  1.0;
    // refTracePoints(2, 0) =  0.0;
    for (int sideIndex=0; sideIndex < numSides; sideIndex++)
    {
      BasisCachePtr sideBasisCache = basisCache->getSideBasisCache(sideIndex);
      // sideBasisCache->setRefCellPoints(refTracePoints);
      int numPoints = sideBasisCache->getPhysicalCubaturePoints().dimension(1);
      // cout << "numPoints = " << numPoints << endl;
      if (sideBasisCache.get() == NULL)
        cout << "NULL Side Basis" << endl;

      vector< FieldContainer<double> > computedValues;
      computedValues.resize(numTraceVars);
      for (int i=0; i < numTraceVars; i++)
      {
        computedValues[i].resize(numCells, numPoints);
        solutionValues(computedValues[i], traceTrialIDs[i], sideBasisCache);
      }
      const FieldContainer<double> *physicalPoints = &sideBasisCache->getPhysicalCubaturePoints();
      // FieldContainer<double> physCubPoints = sideBasisCache->getPhysicalCubaturePoints();
      // cout << " physPoints dim = " << physicalPoints->dimension(0) << " " << physicalPoints->dimension(1)<< endl;

      for (int cellIndex=0;cellIndex < numCells;cellIndex++)
      {
        vtkIdList* edge = vtkIdList::New();
        edge->Initialize();
        for (int i=0; i < numPoints; i++)
        {
          edge->InsertNextId(trace_vertex_count+i);
        }
        trace_ug->InsertNextCell((int)VTK_POLY_LINE, edge);
        edge->Delete();

        // cout << "Physical Points: " << endl;
        for (int pointIndex = 0; pointIndex < numPoints; pointIndex++)
        {
          trace_points->InsertNextPoint((*physicalPoints)(cellIndex, pointIndex, 0),
              (*physicalPoints)(cellIndex, pointIndex, 1), 0.0);
          // cout << (*physicalPoints)(cellIndex, pointIndex, 0)<<" "<<(*physicalPoints)(cellIndex, pointIndex, 1) << endl;
          // trace_points->InsertNextPoint(physCubPoints(cellIndex, pointIndex, 0),
          //     physCubPoints(cellIndex, pointIndex, 1), 0.0);
          for (int varIdx=0; varIdx < numTraceVars; varIdx++)
          {
            traceData[varIdx]->InsertNextValue(computedValues[varIdx](cellIndex, pointIndex));
          }
          trace_vertex_count++;
        }
      }
    }
  }
  trace_ug->SetPoints(trace_points);
  trace_points->Delete();
  for (int varIdx=0; varIdx < numTraceVars; varIdx++)
  {
    trace_ug->GetPointData()->AddArray(traceData[varIdx]);
    traceData[varIdx]->Delete();
  }
  vtkXMLUnstructuredGridWriter* trace_wr = vtkXMLUnstructuredGridWriter::New();
#if VTK_MAJOR_VERSION <= 5
  trace_wr->SetInput(trace_ug);
#else
  trace_wr->SetInputData(trace_ug);
#endif
  trace_ug->Delete();
  trace_wr->SetFileName(("trace_"+filePath+".vtu").c_str());
  trace_wr->SetDataModeToAscii();
  trace_wr->Update();
  trace_wr->Write();
  trace_wr->Delete();

  cout << "Wrote Trace Variables to " << "trace_"+filePath << ".vtu" << endl;
}
#else

// Write solution to new VTK format
// Sorry this code gets a little ugly, I had to make compromises by not using VTK
// Prefer the VTK version, it is cleaner and probably more efficient
void Solution::writeToVTK(const string& filePath, unsigned int num1DPts)
{
  // Get trialIDs
  vector<int> trialIDs = _mesh->bilinearForm()->trialIDs();
  vector<int> fieldTrialIDs;

  // NVR: for this version, if default of num1DPts=0 chosen, just use 4 points:
  if (num1DPts==0) num1DPts = 4;

  int spaceDim = 2; // TODO: generalize to 3D...
  int numFieldVars = 0;
  for (unsigned int i=0; i < trialIDs.size(); i++)
  {
    if (!(_mesh->bilinearForm()->isFluxOrTrace(trialIDs[i])))
    {
      numFieldVars++;
      fieldTrialIDs.push_back(trialIDs[i]);
    }
  }

  ofstream fout(filePath.c_str());
  fout << setprecision(15);

  // To store points as we loop through elements
  Teuchos::RCP< ostringstream > pout(new ostringstream);
  *pout << setprecision(15);
  // To store connectivity as we loop through elements
  Teuchos::RCP< ostringstream > conn(new ostringstream);
  // To store point data as we loop through elements
  vector< Teuchos::RCP< ostringstream> > pdata(0);
  pdata.resize(numFieldVars);
  for (unsigned int k=0; k < numFieldVars; k++)
  {
    pdata[k] = Teuchos::RCP< ostringstream > (new ostringstream);
    *pdata[k] << setprecision(15);
  }
  // To store offset data as we loop through elements
  ostringstream offsets;
  offsets << "          ";
  // To store type data as we loop through elements
  ostringstream types;
  types << "          ";

  fout << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"BigEndian\">" << endl;
  fout << "  <UnstructuredGrid>" << endl;

  vector< ElementTypePtr > elementTypes = _mesh->elementTypes();
  vector< ElementTypePtr >::iterator elemTypeIt;

  unsigned int total_vertices = 0;
  unsigned int total_cells = 0;
  int offsetCount = 0;

  // Loop through Quads, Triangles, etc
  for (elemTypeIt = elementTypes.begin(); elemTypeIt != elementTypes.end(); elemTypeIt++)
  {
    ElementTypePtr elemTypePtr = *(elemTypeIt);
    CellTopoPtr cellTopo = elemTypePtr->cellTopoPtr;

    FieldContainer<double> vertexPoints;
    _mesh->verticesForElementType(vertexPoints,elemTypePtr); //stores vertex points for this element
    FieldContainer<double> physicalCellNodes = _mesh()->physicalCellNodesGlobal(elemTypePtr);

    int numCells = physicalCellNodes.dimension(0);
    bool createSideCacheToo = true;
    BasisCachePtr basisCache = Teuchos::rcp(new BasisCache(elemTypePtr,_mesh, createSideCacheToo));

    vector<GlobalIndexType> cellIDs;
    for (int cellIndex=0; cellIndex<numCells; cellIndex++)
    {
      int cellID = _mesh->cellID(elemTypePtr, cellIndex, -1); // -1: global cellID
      cellIDs.push_back(cellID);
    }

    int numPoints = num1DPts * num1DPts;
    FieldContainer<double> refPoints(numPoints,spaceDim);
    for (int xPointIndex = 0; xPointIndex < num1DPts; xPointIndex++){
      for (int yPointIndex = 0; yPointIndex < num1DPts; yPointIndex++){
        int pointIndex = xPointIndex*num1DPts + yPointIndex;
        double x = -1.0 + 2.0*(double)xPointIndex/((double)num1DPts-1.0);
        double y = -1.0 + 2.0*(double)yPointIndex/((double)num1DPts-1.0);
        refPoints(pointIndex,0) = x;
        refPoints(pointIndex,1) = y;
      }
    }

    basisCache->setRefCellPoints(refPoints);
    basisCache->setPhysicalCellNodes(physicalCellNodes, cellIDs, createSideCacheToo);
    const FieldContainer<double> *physicalPoints = &basisCache->getPhysicalCubaturePoints();

    vector< FieldContainer<double> > computedValues;
    computedValues.resize(numFieldVars);
    for (int i=0; i < numFieldVars; i++)
    {
      computedValues[i].resize(numCells, numPoints);
      solutionValues(computedValues[i], fieldTrialIDs[i], basisCache);
    }

    for (int cellIndex=0; cellIndex<numCells; cellIndex++ )
    {
      int subcellStartIndex = total_vertices;
      for (int I=0; I < num1DPts-1; I++)
      {
        for (int J=0; J < num1DPts-1; J++)
        {
          int ind1 = subcellStartIndex;
          int ind2 = ind1 + num1DPts;
          int ind3 = ind2 + 1;
          int ind4 = ind1 + 1;
          *conn << "          "
            << ind1 <<" "<< ind2 <<" "<< ind3 <<" "<< ind4 << endl;
          subcellStartIndex++;
          total_cells++;
          offsetCount += 4;
          offsets << offsetCount << " ";
          types << 9 << " ";
        }
        subcellStartIndex++;
      }
      for (int pointIndex = 0; pointIndex < num1DPts*num1DPts; pointIndex++)
      {
        *pout << "          "
          << (*physicalPoints)(cellIndex, pointIndex, 0) << " "
          << (*physicalPoints)(cellIndex, pointIndex, 1) << " "
          << 0 << endl;
        total_vertices++;
      }
      for (int varIdx=0; varIdx < numFieldVars; varIdx++)
      {
        for (int pointIndex = 0; pointIndex < num1DPts*num1DPts; pointIndex++)
        {
          *pdata[varIdx] << "          "
            << computedValues[varIdx](cellIndex, pointIndex) << endl;
        }
      }
    }
  }
  fout << "    <Piece NumberOfPoints=\"" << total_vertices << "\" "
    << "NumberOfCells=\"" << total_cells << "\">" << endl;
  fout << "      <Points>" << endl;
  fout << "        <DataArray type=\"Float32\" NumberOfComponents=\"3\" "
    << "Format=\"ascii\">" << endl;
  fout << pout->str();
  fout << "        </DataArray>" << endl;
  fout << "      </Points>" << endl;
  fout << "      <Cells>" << endl;
  fout << "        <DataArray type=\"Int32\" Name=\"connectivity\" " << "Format=\"ascii\">" << endl;
  fout << conn->str();
  fout << "        </DataArray>" << endl;
  fout << "        <DataArray type=\"Int32\" Name=\"offsets\" " << "Format=\"ascii\">" << endl;
  fout << offsets.str() << endl;
  fout << "        </DataArray>" << endl;
  fout << "        <DataArray type=\"Int32\" Name=\"types\" " << "Format=\"ascii\">" << endl;
  fout << types.str() << endl;
  fout << "        </DataArray>" << endl;
  fout << "      </Cells>" << endl;
  fout << "      <PointData Scalars=\"scalars\">" << endl;
  for (int varIdx = 0; varIdx < numFieldVars; varIdx++)
  {
  fout << "        <DataArray type=\"Float32\" Name=\""
    << _mesh->bilinearForm()->trialName(fieldTrialIDs[varIdx]) << "\" "
    << "NumberOfComponents=\"1\" Format=\"ascii\">" << endl;
  fout << pdata[varIdx]->str();
  fout << "        </DataArray>" << endl;
  }
  fout << "      </PointData>" << endl;
  fout << "    </Piece>" << endl;
  fout << "  </UnstructuredGrid>" << endl;
  fout << "</VTKFile>" << endl;
  fout.close();
}

#endif
