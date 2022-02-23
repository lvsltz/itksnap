#include "MeshWrapperBase.h"
#include "vtkPolyData.h"
#include "vtkPointData.h"
#include "vtkCellData.h"
#include "vtkDataSetAttributes.h"
#include <itksys/SystemTools.hxx>
#include "MeshDisplayMappingPolicy.h"

// ========================================
//  AbstractMeshDataArrayProperty Implementation
// ========================================
AbstractMeshDataArrayProperty::
AbstractMeshDataArrayProperty()
{

}

AbstractMeshDataArrayProperty::
~AbstractMeshDataArrayProperty()
{
  delete[] m_Name;
}

void
AbstractMeshDataArrayProperty::
Initialize(vtkDataArray *array, MeshDataType type)
{
  // Copy the array name to
  m_Name = new char[strlen(array->GetName())];
  strcpy(m_Name, array->GetName());

  double *range = array->GetRange();
  m_min = range[0];
  m_max = range[1];

  m_Type = type;
}

void
AbstractMeshDataArrayProperty::
Update(vtkDataArray *array)
{
  if (!strcmp(m_Name, array->GetName()))
    return;

  double *range = array->GetRange();
  m_min = range[0];
  m_max = range[1];
}

AbstractMeshDataArrayProperty::MeshDataType
AbstractMeshDataArrayProperty::GetType() const
{
  return m_Type;
}

void
AbstractMeshDataArrayProperty::
Print(ostream &os) const
{
  os << "[AbstractMeshDataArrayProperty]" << std::endl;
  std::cout << "name: " << m_Name << std::endl;
  std::cout << "type: " << m_Type << std::endl;
  std::cout << "min: " << m_min << std::endl;
  std::cout << "max: " << m_max << std::endl;
}

// ========================================
//  MeshDataArrayProperty Implementation
// ========================================
MeshDataArrayProperty::
MeshDataArrayProperty()
{

}

MeshDataArrayProperty::
~MeshDataArrayProperty()
{

}

void
MeshDataArrayProperty::
SetDataPointer(vtkDataArray *array)
{
  m_DataPointer = array;
}


// ============================================
//  MeshLayerDataArrayProperty Implementation
// ============================================
MeshLayerDataArrayProperty::
MeshLayerDataArrayProperty()
{
  m_ColorMap = ColorMap::New();
  m_ColorMap->SetToSystemPreset(
        static_cast<ColorMap::SystemPreset>
        (ColorMap::SystemPreset::COLORMAP_JET));
  m_IntensityCurve = IntensityCurveVTK::New();
  m_IntensityCurve->Initialize();

  m_HistogramFilter = HistogramFilterType::New();
  m_HistogramFilter->SetNumberOfBins(DEFAULT_HISTOGRAM_BINS);
  m_MinMaxFilter = MinMaxFilterType::New();
}


void
MeshLayerDataArrayProperty::
Initialize(MeshDataArrayProperty *other)
{
  this->m_Name = new char[strlen(other->GetName())];
  strcpy(this->m_Name, other->GetName());
  this->m_min = other->GetMin();
  this->m_max = other->GetMax();
  this->m_Type = other->GetType();

  m_DataPointerList.push_back(other->GetDataPointer());
}

void
MeshLayerDataArrayProperty::
Merge(MeshDataArrayProperty *other)
{
  // the name must be same
  assert(!strcmp(this->m_Name, other->GetName()));

  if (other->GetMax() > this->m_max)
    this->m_max = other->GetMax();

  if (other->GetMin() < this->m_min)
    this->m_min = other->GetMin();

  auto it = std::find(m_DataPointerList.begin(), m_DataPointerList.end(),
            other->GetDataPointer());

  if (it == m_DataPointerList.end())
    m_DataPointerList.push_back(other->GetDataPointer());
}

ScalarImageHistogram*
MeshLayerDataArrayProperty::
GetHistogram(size_t nBins) const
{
  if (nBins > 0)
    m_HistogramFilter->SetNumberOfBins(nBins);

  std::vector<double> tmpdata;
  long n = 0;

  for (auto cit = m_DataPointerList.cbegin(); cit != m_DataPointerList.cend(); ++cit)
    {
    auto array = *cit;
    n += array->GetNumberOfTuples();
    }

  std::cout << "[MeshLayerDataArrayProp] tmpdata size=" << n << std::endl;

  DataArrayImageType::Pointer img = DataArrayImageType::New();

  DataArrayImageType::IndexType start;
  start[0] = 0;

  DataArrayImageType::SizeType size;
  size[0] = n;

  DataArrayImageType::RegionType region;
  region.SetSize(size);
  region.SetIndex(start);

  img->SetRegions(region);
  img->Allocate();

  DataArrayImageType::IndexType idx;
  idx[0] = 0;

  for (auto cit = m_DataPointerList.cbegin(); cit != m_DataPointerList.cend(); ++cit)
    {
    auto array = *cit;
    auto nTuple = array->GetNumberOfTuples();
    for (auto i = 0; i < nTuple; ++i)
      {
      auto v = array->GetComponent(i, 0);
      img->SetPixel(idx, v);
      idx[0]++;

      if (idx[0] >= n)
        break;
      }
    if (idx[0] >= n)
      break;
    }

  //std::cout << "[MeshLayerDataArrayProp] img" << img << std::endl;

  m_HistogramFilter->SetInput(img);
  m_MinMaxFilter->SetInput(img);

  m_HistogramFilter->SetRangeInputs(m_MinMaxFilter->GetMinimumOutput(),
                                    m_MinMaxFilter->GetMaximumOutput());

  m_HistogramFilter->Update();

  return m_HistogramFilter->GetHistogramOutput();
}



// ========================================
//  PolyDataWrapper Implementation
// ========================================
void PolyDataWrapper::SetPolyData(vtkSmartPointer<vtkPolyData> polydata)
{
  m_PolyData = polydata;
  UpdateDataArrayProperties();
}

vtkSmartPointer<vtkPolyData>
PolyDataWrapper::GetPolyData()
{
  assert(m_PolyData);
  return m_PolyData;
}

void
PolyDataWrapper::UpdateDataArrayProperties()
{
  assert(m_PolyData);

  std::cout << "[PolyDataWrapper] Update Array Props" << std::endl;

  // Process Point Data
  auto pointData = m_PolyData->GetPointData();
  UpdatePropertiesFromVTKData(m_PointDataProperties, pointData,
                              MeshDataType::POINT_DATA);

  // Process Cell Data
  auto cellData = m_PolyData->GetCellData();
  UpdatePropertiesFromVTKData(m_CellDataProperties, cellData,
                              MeshDataType::CELL_DATA);
}

void
PolyDataWrapper::
UpdatePropertiesFromVTKData(MeshDataArrayPropertyMap &propMap,
                            vtkDataSetAttributes *data,
                            MeshDataType type) const
{
  std::cout << "[PolyDataWrapper] Number of Array=" << data->GetNumberOfArrays() << std::endl;

  for (int i = 0; i < data->GetNumberOfArrays(); ++i)
    {
      auto arr = vtkDataArray::SafeDownCast(data->GetAbstractArray(i));

      // Only Process valid vtkdataarray
      if (arr)
        {
        // Get the name of the array
        auto name = arr->GetName();
        if (propMap.count(name))
          {
          // Update the existing entry
          propMap[name]->Update(arr);
          }
        else
          {
          // Create a new property
          auto prop = MeshDataArrayProperty::New();
          prop->Initialize(arr, type);
          prop->SetDataPointer(arr);
          propMap[name] = prop;
          }
        }
    }
}

// ========================================
//  MeshAssembly Implementation
// ========================================
void MeshAssembly::AddMesh(SmartPtr<PolyDataWrapper> mesh, LabelType id)
{
  m_Meshes[id] = mesh;
}

SmartPtr<PolyDataWrapper>
MeshAssembly::GetMesh(LabelType id)
{
  return m_Meshes[id];
}

bool
MeshAssembly::Exist(LabelType id)
{
  return m_Meshes.count(id);
}



// ============================================
//  MeshWrapperBase Implementation
// ============================================

MeshWrapperBase::MeshWrapperBase()
{
  m_DisplayMapping = MeshDisplayMappingPolicy::New();
  m_DisplayMapping->SetMesh(this);
}

MeshWrapperBase::~MeshWrapperBase()
{
}

void
MeshWrapperBase::
MergeDataProperties(MeshLayerDataArrayPropertyMap &dest, MeshDataArrayPropertyMap &src)
{
  for (auto cit = src.cbegin(); cit != src.cend(); ++cit)
    {
    if (dest.count(cit->first))
      {
      // Merge with existing
      dest[cit->first]->Merge(cit->second);
      }
    else
      {
      // Add new entry
      auto newprop = MeshLayerDataArrayProperty::New();
      newprop->Initialize(cit->second);
      dest[cit->first] = newprop;
      m_CombinedDataPropertyMap[++m_CombinedPropID] = newprop;
      }

    }
}

void
MeshWrapperBase::SetMesh(vtkSmartPointer<vtkPolyData> mesh, unsigned int timepoint, LabelType id)
{
  std::cout << "[MeshWrapperBase] SetMesh called" << std::endl;

  auto wrapper = PolyDataWrapper::New();
  wrapper->SetPolyData(mesh);

  auto pointDataProps = wrapper->GetPointDataProperties();
  auto cellDataProps = wrapper->GetCellDataProperties();

  std::cout << "[MeshWrapperBase] pointData size=" << pointDataProps.size() << std::endl;

  // Add or merge data properties
  MergeDataProperties(m_PointDataProperties, pointDataProps);
  MergeDataProperties(m_CellDataProperties, cellDataProps);

  // Add wrapper to mesh assembly
  if (m_MeshAssemblyMap.count(timepoint))
    m_MeshAssemblyMap[timepoint]->AddMesh(wrapper, id);
  else
    {
      auto assembly = MeshAssembly::New();
      assembly->AddMesh(wrapper, id);
      m_MeshAssemblyMap[timepoint] = assembly;
    }

  // Set Default active array id
  std::cout << "[MeshWrapperBase] Combined Map Size=" << m_CombinedDataPropertyMap.size() << std::endl;
  SetActiveMeshLayerDataPropertyId(m_CombinedDataPropertyMap.cbegin()->first);

  InvokeEvent(ValueChangedEvent());
}

SmartPtr<PolyDataWrapper>
MeshWrapperBase::GetMesh(unsigned int timepoint, LabelType id)
{
  PolyDataWrapper *ret = nullptr;

  if (m_MeshAssemblyMap.count(timepoint))
    ret = m_MeshAssemblyMap[timepoint]->GetMesh(id);

  return ret;
}

SmartPtr<MeshAssembly>
MeshWrapperBase::GetMeshAssembly(unsigned int timepoint)
{
  MeshAssembly *ret = nullptr;

  if (m_MeshAssemblyMap.count(timepoint))
    ret = m_MeshAssemblyMap[timepoint];

  return ret;
}

bool
MeshWrapperBase::IsA(const char *type) const
{
  return strcmp("MeshWrapperBase", type) == 0;
}

void
MeshWrapperBase::SetFileName(const std::string &name)
{
  m_FileName = name;
  m_FileNameShort = itksys::SystemTools::GetFilenameWithoutExtension(
        itksys::SystemTools::GetFilenameName(name));
  this->InvokeEvent(WrapperMetadataChangeEvent());
}

const ScalarImageHistogram *
MeshWrapperBase::GetHistogram(size_t nBins)
{
  auto prop = GetActiveDataArrayProperty();
  return prop->GetHistogram(nBins);
}

void
MeshWrapperBase::SetCustomNickname(const std::string &nickname)
{
  // Make sure the nickname is real
  if(nickname == m_FileNameShort)
    m_CustomNickname.clear();
  else
    m_CustomNickname = nickname;

  this->InvokeEvent(WrapperMetadataChangeEvent());
}

const std::string&
MeshWrapperBase::GetNickname() const
{
  if(m_CustomNickname.length())
    return m_CustomNickname;

  else if(m_FileName.length())
    return m_FileNameShort;

  else return m_DefaultNickname;
}

SmartPtr<MeshLayerDataArrayProperty>
MeshWrapperBase::GetActiveDataArrayProperty()
{
  MeshLayerDataArrayProperty *ret = nullptr;
  if (m_CombinedDataPropertyMap.count(m_ActiveDataPropertyId))
    ret = m_CombinedDataPropertyMap[m_ActiveDataPropertyId];

  return ret;
}

void
MeshWrapperBase::
SetActiveMeshLayerDataPropertyId(int id)
{
  if (m_ActiveDataPropertyId == id)
    return;

  m_ActiveDataPropertyId = id;

  // if failed check caller's logic
  assert(m_CombinedDataPropertyMap.count(id));

  // check is point or cell data
  auto prop = m_CombinedDataPropertyMap[id];

  std::cout << "[MeshWrapperBase] SetActiveProp prop=" << prop << std::endl;

  // Change the active array
  if (prop->GetType() == MeshDataArrayProperty::POINT_DATA)
    {
    std::cout << "[MeshWrapperBase] Set Point Data" << std::endl;
    std::cout << "[MeshWrapperBase] MeshAssemblyMapSize=" << m_MeshAssemblyMap.size() << std::endl;
    for (auto cit = m_MeshAssemblyMap.cbegin(); cit != m_MeshAssemblyMap.cend(); ++cit)
      {
      for (auto polyIt = cit->second->cbegin(); polyIt != cit->second->cend(); ++polyIt)
        {

        auto pointData = polyIt->second->GetPolyData()->GetPointData();
        pointData->SetActiveAttribute(prop->GetName(),
                               vtkDataSetAttributes::SCALARS);
        }
      }
    }
  else if (prop->GetType() == MeshDataArrayProperty::CELL_DATA)
    {
    std::cout << "[MeshWrapperBase] Set Cell Data" << std::endl;
    for (auto cit = m_MeshAssemblyMap.cbegin(); cit != m_MeshAssemblyMap.cend(); ++cit)
      for (auto polyIt = cit->second->cbegin(); polyIt != cit->second->cend(); ++polyIt)
        {
        polyIt->second->GetPolyData()->GetCellData()->
            SetActiveAttribute(prop->GetName(),
                               vtkDataSetAttributes::SCALARS);
        }
    }

  auto dmp = GetMeshDisplayMappingPolicy();
  dmp->SetColorMap(prop->GetColorMap());
  dmp->SetIntensityCurve(prop->GetIntensityCurve());

  InvokeEvent(WrapperDisplayMappingChangeEvent());
  InvokeEvent(itk::ModifiedEvent());
}

