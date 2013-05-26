#include "SnakeWizardModel.h"
#include "GlobalUIModel.h"
#include "IRISException.h"
#include "IRISApplication.h"
#include "GlobalState.h"
#include "GenericImageData.h"
#include "SNAPImageData.h"
#include "SmoothBinaryThresholdImageFilter.h"
#include "EdgePreprocessingImageFilter.h"
#include "ColorMap.h"
#include "SlicePreviewFilterWrapper.h"
#include "UnsupervisedClustering.h"

SnakeWizardModel::SnakeWizardModel()
{
  // Set up the child models
  m_ThresholdUpperModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetThresholdUpperValueAndRange,
        &Self::SetThresholdUpperValue,
        ThresholdSettingsUpdateEvent(),
        ThresholdSettingsUpdateEvent());

  m_ThresholdLowerModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetThresholdLowerValueAndRange,
        &Self::SetThresholdLowerValue,
        ThresholdSettingsUpdateEvent(),
        ThresholdSettingsUpdateEvent());

  m_ThresholdSmoothnessModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetThresholdSmoothnessValueAndRange,
        &Self::SetThresholdSmoothnessValue,
        ThresholdSettingsUpdateEvent(),
        ThresholdSettingsUpdateEvent());

  m_ThresholdModeModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetThresholdModeValue,
        &Self::SetThresholdModeValue,
        ThresholdSettingsUpdateEvent(),
        ThresholdSettingsUpdateEvent());

  m_PreviewModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetPreviewValue,
        &Self::SetPreviewValue);

  // TODO: which events from the parent model should be rebroadcast by the
  // preview model?

  // EdgePreprocessingSettingsUpdateEvent(),
  // EdgePreprocessingSettingsUpdateEvent()

  m_EdgePreprocessingSigmaModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetEdgePreprocessingSigmaValueAndRange,
        &Self::SetEdgePreprocessingSigmaValue,
        EdgePreprocessingSettingsUpdateEvent(),
        EdgePreprocessingSettingsUpdateEvent());

  m_EdgePreprocessingKappaModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetEdgePreprocessingKappaValueAndRange,
        &Self::SetEdgePreprocessingKappaValue,
        EdgePreprocessingSettingsUpdateEvent(),
        EdgePreprocessingSettingsUpdateEvent());

  m_EdgePreprocessingExponentModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetEdgePreprocessingExponentValueAndRange,
        &Self::SetEdgePreprocessingExponentValue,
        EdgePreprocessingSettingsUpdateEvent(),
        EdgePreprocessingSettingsUpdateEvent());

  m_SnakeTypeModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetSnakeTypeValueAndRange,
        &Self::SetSnakeTypeValue);

  m_ActiveBubbleModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetActiveBubbleValue,
        &Self::SetActiveBubbleValue,
        ActiveBubbleUpdateEvent());

  m_BubbleRadiusModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetBubbleRadiusValueAndRange,
        &Self::SetBubbleRadiusValue,
        BubbleDefaultRadiusUpdateEvent(),
        BubbleDefaultRadiusUpdateEvent());

  m_StepSizeModel = NewRangedConcreteProperty(1, 1, 100, 1);

  // Need to define a null setter function
  void (Self::*nullsetter)(int) = NULL;

  m_EvolutionIterationModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetEvolutionIterationValue,
        nullsetter,
        EvolutionIterationEvent());

  m_NumberOfClustersModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetNumberOfClustersValueAndRange,
        &Self::SetNumberOfClustersValue);

  m_NumberOfGMMSamplesModel = wrapGetterSetterPairAsProperty(
        this,
        &Self::GetNumberOfGMMSamplesValueAndRange,
        &Self::SetNumberOfGMMSamplesValue);

}

void SnakeWizardModel::SetParentModel(GlobalUIModel *model)
{
  m_Parent = model;
  m_Driver = m_Parent->GetDriver();
  m_GlobalState = m_Driver->GetGlobalState();

  // Layer changes are rebroadcast as model changes, causing all child
  // models to update themselves.
  Rebroadcast(m_Driver, LayerChangeEvent(), ModelUpdateEvent());

  // Model update events are the "big events", and are rebroadcast
  // as the specialized events as well.
  Rebroadcast(this, ModelUpdateEvent(), ThresholdSettingsUpdateEvent());

  // Changes to the threshold settings are rebroadcast as own own events
  Rebroadcast(m_Driver->GetThresholdSettings(),
              itk::ModifiedEvent(), ThresholdSettingsUpdateEvent());

  // Changes to the preview pipeline (preview status) are broadcast as events
  Rebroadcast(m_Driver->GetPreprocessingFilterPreviewer(PREPROCESS_THRESHOLD),
              itk::ModifiedEvent(), ThresholdSettingsUpdateEvent());

  // Repeat the same code for the edge preprocessing
  Rebroadcast(this, ModelUpdateEvent(), EdgePreprocessingSettingsUpdateEvent());

  Rebroadcast(m_Driver->GetEdgePreprocessingSettings(),
              itk::ModifiedEvent(), EdgePreprocessingSettingsUpdateEvent());

  Rebroadcast(m_Driver->GetPreprocessingFilterPreviewer(PREPROCESS_EDGE),
              itk::ModifiedEvent(), EdgePreprocessingSettingsUpdateEvent());

  Rebroadcast(m_Driver->GetPreprocessingFilterPreviewer(PREPROCESS_GMM),
              itk::ModifiedEvent(), GMMModifiedEvent());

  // Changes to the snake mode are cast as model update events
  Rebroadcast(m_GlobalState->GetSnakeTypeModel(),
              ValueChangedEvent(), ModelUpdateEvent());

  // We also need to rebroadcast these events as state change events
  Rebroadcast(this, ThresholdSettingsUpdateEvent(), StateMachineChangeEvent());
  Rebroadcast(this, EdgePreprocessingSettingsUpdateEvent(), StateMachineChangeEvent());
  Rebroadcast(this, ModelUpdateEvent(), StateMachineChangeEvent());
  Rebroadcast(this, ActiveBubbleUpdateEvent(), StateMachineChangeEvent());
}


bool SnakeWizardModel::CheckState(SnakeWizardModel::UIState state)
{
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  switch(state)
    {
    case UIF_THESHOLDING_ENABLED:
      return AreThresholdModelsActive();
    case UIF_LOWER_THRESHOLD_ENABLED:
      return ts->IsLowerThresholdEnabled();
    case UIF_UPPER_THRESHOLD_ENABLED:
      return ts->IsUpperThresholdEnabled();
    case UIF_EDGEPROCESSING_ENABLED:
      return AreEdgePreprocessingModelsActive();
    case UIF_SPEED_AVAILABLE:
      return m_GlobalState->GetSpeedValid();
    case UIF_PREPROCESSING_ACTIVE:
      return m_Driver->GetPreprocessingMode() != PREPROCESS_NONE;
    case UIF_BUBBLE_SELECTED:
      return m_GlobalState->GetActiveBubble() >= 0;
    case UIF_INITIALIZATION_VALID:
      return m_GlobalState->GetSnakeInitializedWithManualSegmentation()
          || m_Driver->GetBubbleArray().size() > 0;
    }

  return false;
}


void SnakeWizardModel::OnUpdate()
{
  // If there is a change in available layers, we must rebuild the list
  // of available components.
  if(m_EventBucket->HasEvent(LayerChangeEvent()))
    {
    m_ComponentInfo.clear();
    LayerIterator it = m_Driver->GetSNAPImageData()->GetLayers(
          LayerIterator::MAIN_ROLE | LayerIterator::OVERLAY_ROLE);
    for(; !it.IsAtEnd(); ++it)
      {
      if(it.GetLayerAsVector())
        {
        for(int comp = 0; comp < it.GetLayerAsVector()->GetNumberOfComponents(); ++comp)
          {
          m_ComponentInfo.push_back(std::make_pair(it.GetLayer(), comp));
          }
        }
      else
        {
        m_ComponentInfo.push_back(std::make_pair(it.GetLayer(), 0));
        }
      }
    }
}


bool SnakeWizardModel::AreThresholdModelsActive()
{
  return (m_Driver->IsSnakeModeActive() &&
          m_Driver->GetSnakeMode() == IN_OUT_SNAKE);
}

bool SnakeWizardModel::AreEdgePreprocessingModelsActive()
{
  return (m_Driver->IsSnakeModeActive() &&
          m_Driver->GetSnakeMode() == EDGE_SNAKE);
}

ScalarImageWrapperBase *SnakeWizardModel::GetSelectedScalarLayer()
{
  // TODO: this should be set by the wizard through user interaction.
  // This is just a placeholder
  return m_Driver->GetCurrentImageData()->GetMain()->GetDefaultScalarRepresentation();
}

bool SnakeWizardModel
::GetThresholdUpperValueAndRange(
    double &x, NumericValueRange<double> *range)
{
  if(!AreThresholdModelsActive())
    return false;

  ScalarImageWrapperBase *iw = this->GetSelectedScalarLayer();
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();

  // The thresholds are stored in internal image representation, but are
  // presented to the user in native image representation.
  x = iw->GetNativeIntensityMapping()->MapInternalToNative(ts->GetUpperThreshold());

  if(range)
    {
    range->Minimum = iw->GetImageMinNative();
    range->Maximum = iw->GetImageMaxNative();
    range->StepSize = CalculatePowerOfTenStepSize(range->Minimum, range->Maximum, 100);
    }

  return true;
}

bool SnakeWizardModel
::GetThresholdLowerValueAndRange(
    double &x, NumericValueRange<double> *range)
{
  if(!AreThresholdModelsActive())
    return false;

  ScalarImageWrapperBase *iw = this->GetSelectedScalarLayer();
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();

  // The thresholds are stored in internal image representation, but are
  // presented to the user in native image representation.
  x = iw->GetNativeIntensityMapping()->MapInternalToNative(ts->GetLowerThreshold());

  if(range)
    {
    range->Minimum = iw->GetImageMinNative();
    range->Maximum = iw->GetImageMaxNative();
    range->StepSize = CalculatePowerOfTenStepSize(range->Minimum, range->Maximum, 100);
    }

  return true;
}

void SnakeWizardModel
::SetThresholdUpperValue(double x)
{
  // Map the value to internal format
  ScalarImageWrapperBase *iw = this->GetSelectedScalarLayer();
  float z = (float) iw->GetNativeIntensityMapping()->MapNativeToInternal(x);

  // Get the current settings
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  if(z < ts->GetLowerThreshold())
    ts->SetLowerThreshold(z);

  ts->SetUpperThreshold(z);
}

void SnakeWizardModel
::SetThresholdLowerValue(double x)
{
  // Map the value to internal format
  ScalarImageWrapperBase *iw = this->GetSelectedScalarLayer();
  float z = (float) iw->GetNativeIntensityMapping()->MapNativeToInternal(x);

  // Get the current settings
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  if(z > ts->GetUpperThreshold())
    ts->SetUpperThreshold(z);

  ts->SetLowerThreshold(z);
}

bool
SnakeWizardModel
::GetThresholdSmoothnessValueAndRange(double &x, NumericValueRange<double> *range)
{
  if(!AreThresholdModelsActive())
    return false;

  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  x = ts->GetSmoothness();
  if(range)
    range->Set(0, 10, 0.1);
  return true;
}

void SnakeWizardModel::SetThresholdSmoothnessValue(double x)
{
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  ts->SetSmoothness(x);
}

bool SnakeWizardModel::GetThresholdModeValue(ThresholdSettings::ThresholdMode &x)
{
  if(!AreThresholdModelsActive())
    return false;

  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  x = ts->GetThresholdMode();
  return true;
}

void SnakeWizardModel::SetThresholdModeValue(ThresholdSettings::ThresholdMode x)
{
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  ts->SetThresholdMode(x);
}


bool SnakeWizardModel::GetPreviewValue(bool &value)
{
  PreprocessingMode mode = m_Driver->GetPreprocessingMode();
  if(mode != PREPROCESS_NONE)
    {
    value = m_Driver->GetPreprocessingFilterPreviewer(mode)->IsPreviewMode();
    return true;
    }
  return false;
}

void SnakeWizardModel::SetPreviewValue(bool value)
{
  PreprocessingMode mode = m_Driver->GetPreprocessingMode();
  if(mode != PREPROCESS_NONE)
    {
    m_Driver->GetPreprocessingFilterPreviewer(mode)->SetPreviewMode(value);
    }
}

void SnakeWizardModel
::EvaluateThresholdFunction(unsigned int n, float *x, float *y)
{
  assert(m_Driver->IsSnakeModeActive());

  ScalarImageWrapperBase *grey = this->GetSelectedScalarLayer();
  ThresholdSettings *ts = m_Driver->GetThresholdSettings();
  SpeedImageWrapper *speed = m_Driver->GetSNAPImageData()->GetSpeed();

  double imin = grey->GetImageMinAsDouble();
  double imax = grey->GetImageMaxAsDouble();

  SmoothBinaryThresholdFunctor<float> functor;
  functor.SetParameters(ts, imin, imax);

  for(int i = 0; i < n; i++)
    {
    float t = i * 1.0 / (n - 1);
    float x_internal = imin + t * (imax - imin);
    x[i] = grey->GetNativeIntensityMapping()->MapInternalToNative(x_internal);
    y[i] = speed->GetNativeIntensityMapping()->MapInternalToNative(functor(x_internal));
    }
}

bool
SnakeWizardModel
::GetEdgePreprocessingSigmaValueAndRange(
    double &x, NumericValueRange<double> *range)
{
  if(!AreEdgePreprocessingModelsActive())
    return false;

  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  x = eps->GetGaussianBlurScale();
  if(range)
    range->Set(0.1, 3, 0.1);

  return true;
}

void
SnakeWizardModel
::SetEdgePreprocessingSigmaValue(double x)
{
  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  eps->SetGaussianBlurScale(x);
}

bool
SnakeWizardModel
::GetEdgePreprocessingKappaValueAndRange(
    double &x, NumericValueRange<double> *range)
{
  if(!AreEdgePreprocessingModelsActive())
    return false;

  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  x = eps->GetRemappingSteepness();
  if(range)
    range->Set(0.001, 0.2, 0.001);

  return true;
}

void
SnakeWizardModel
::SetEdgePreprocessingKappaValue(double x)
{
  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  eps->SetRemappingSteepness(x);
}


bool
SnakeWizardModel
::GetEdgePreprocessingExponentValueAndRange(
    double &x, NumericValueRange<double> *range)
{
  if(!AreEdgePreprocessingModelsActive())
    return false;

  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  x = eps->GetRemappingExponent();
  if(range)
    range->Set(1, 4, 0.01);

  return true;
}

void
SnakeWizardModel
::SetEdgePreprocessingExponentValue(double x)
{
  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  eps->SetRemappingExponent(x);
}


void SnakeWizardModel
::EvaluateEdgePreprocessingFunction(unsigned int n, float *x, float *y)
{
  assert(m_Driver->IsSnakeModeActive());

  EdgePreprocessingSettings *eps = m_Driver->GetEdgePreprocessingSettings();
  ScalarImageWrapperBase *grey = this->GetSelectedScalarLayer();
  SpeedImageWrapper *speed = m_Driver->GetSNAPImageData()->GetSpeed();

  // Get the range of gradient magnitude in native units
  double xlim = grey->GetImageGradientMagnitudeUpperLimitNative();
  EdgeRemappingFunctor<float> functor;
  functor.SetParameters(0, xlim,
                        eps->GetRemappingExponent(),
                        eps->GetRemappingSteepness());

  for(int i = 0; i < n; i++)
    {
    float t = i * 1.0 / (n - 1);
    float x_internal = t * xlim;
    x[i] = x_internal;
    y[i] = speed->GetNativeIntensityMapping()->MapInternalToNative(functor(x_internal));
    }
}

void SnakeWizardModel::ApplyThresholdPreprocessing()
{
  // Compute the speed image
  m_Driver->ApplyCurrentPreprocessingModeToSpeedVolume(m_Parent->GetProgressCommand());

  // Invoke an event so we get a screen update
  InvokeEvent(ModelUpdateEvent());
}

bool SnakeWizardModel::GetSnakeTypeValueAndRange(
    SnakeType &value, GlobalState::SnakeTypeDomain *range)
{
  return m_GlobalState->GetSnakeTypeModel()->GetValueAndDomain(value, range);
}

void SnakeWizardModel::SetSnakeTypeValue(SnakeType value)
{
  m_Driver->SetSnakeMode(value);
}

void SnakeWizardModel::OnPreprocessingDialogClose()
{
  // Disconnect preview pipeline
  m_Driver->EnterPreprocessingMode(PREPROCESS_NONE);
  InvokeEvent(ModelUpdateEvent());
}

void SnakeWizardModel::OnThresholdingPageEnter()
{
  m_Driver->EnterPreprocessingMode(PREPROCESS_THRESHOLD);
  InvokeEvent(GMMModifiedEvent());
  InvokeEvent(ModelUpdateEvent());
}

void SnakeWizardModel::OnEdgePreprocessingPageEnter()
{
  m_Driver->EnterPreprocessingMode(PREPROCESS_EDGE);
  InvokeEvent(GMMModifiedEvent());
  InvokeEvent(ModelUpdateEvent());
}

void SnakeWizardModel::OnClusteringPageEnter()
{
  m_Driver->EnterPreprocessingMode(PREPROCESS_GMM);

  // Explicitly initialize the EM object
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();

  InvokeEvent(GMMModifiedEvent());
  InvokeEvent(ModelUpdateEvent());
}

bool
SnakeWizardModel
::GetActiveBubbleValue(int &value)
{
  // This is irrelevant when the snake is inactive
  if(!m_Driver->IsSnakeModeActive())
    return false;

  // This may be -1 if no bubbles are selected
  value = m_GlobalState->GetActiveBubble();
  return true;
}

void
SnakeWizardModel
::SetActiveBubbleValue(int value)
{
  m_GlobalState->SetActiveBubble(value);
  InvokeEvent(ActiveBubbleUpdateEvent());
}

void SnakeWizardModel::AddBubbleAtCursor()
{
  // Create a new bubble, using the default radius value
  Bubble bub;
  bub.center = to_int(m_Driver->GetCursorPosition());
  bub.radius = m_BubbleRadiusDefaultValue;

  // Add the bubble to the global state
  m_Driver->GetBubbleArray().push_back(bub);

  // Set the bubble's position
  m_GlobalState->SetActiveBubble(m_Driver->GetBubbleArray().size() - 1);

  // Update the bubble list in the GUI
  InvokeEvent(ActiveBubbleUpdateEvent());
  InvokeEvent(BubbleListUpdateEvent());
  InvokeEvent(BubbleDefaultRadiusUpdateEvent());
}

void SnakeWizardModel::RemoveBubbleAtCursor()
{
  int ibub = m_GlobalState->GetActiveBubble();
  IRISApplication::BubbleArray &ba = m_Driver->GetBubbleArray();

  if(ibub >= 0 && ibub < (int) ba.size())
    {
    // Remove the bubble from the global state
    ba.erase(ba.begin() + ibub);

    // Update the active bubble
    if(ibub == (int) ba.size())
      m_GlobalState->SetActiveBubble(ibub - 1);

    // Update the bubble list in the GUI
    InvokeEvent(ActiveBubbleUpdateEvent());
    InvokeEvent(BubbleListUpdateEvent());
    InvokeEvent(BubbleDefaultRadiusUpdateEvent());
    }
  else
    {
    throw IRISException("Invalid bubble index %d selected for removal.", ibub);
    }
}

bool SnakeWizardModel::UpdateBubble(int index, Bubble bubble)
{
  if(m_Driver->GetCurrentImageData()->GetImageRegion().IsInside(
       to_itkIndex(bubble.center)))
    {
    m_Driver->GetBubbleArray()[index] = bubble;
    InvokeEvent(BubbleDefaultRadiusUpdateEvent());
    return true;
    }
  return false;
}

void SnakeWizardModel::OnSnakeModeEnter()
{
  // Initialize the image data
  // TODO: how to deal with the progress dialog?
  m_Driver->InitializeSNAPImageData(
        m_Driver->GetGlobalState()->GetSegmentationROISettings());

  m_Driver->SetCurrentImageDataToSNAP();

  // Upon entering this mode, we need reset the active tools
  m_Parent->SetToolbarMode(CROSSHAIRS_MODE);
  m_Parent->SetToolbarMode3D(TRACKBALL_MODE);

  // Some preparatory stuff
  this->ComputeBubbleRadiusDefaultAndRange();
}

void SnakeWizardModel::ComputeBubbleRadiusDefaultAndRange()
{
  // Set bubble radius range according to volume dimensions (world dimensions)
  Vector3ui size = m_Driver->GetSNAPImageData()->GetVolumeExtents();
  Vector3d voxdims = m_Driver->GetSNAPImageData()->GetImageSpacing();
  double mindim =
      vector_multiply_mixed<double,unsigned int,3>(voxdims, size).min_value();

  // The largest value of the bubble radius is mindim / 2
  double xBubbleMax = 0.5 * mindim ;

  // The unit step should be equal or smaller than the smallest voxel edge length
  // divided by two, and should be a power of 10. Since FLTK accepts rational step
  // size, we compute it as a ratio two numbers
  double xMinVoxelEdge = 0.5 * voxdims.min_value();
  int xBubbleStepA = 1, xBubbleStepB = 1;
  int xLogVoxelEdge = (int) floor(log10(xMinVoxelEdge));
  if(xLogVoxelEdge > 0)
    xBubbleStepA = (int)(0.5 + pow(10.0, xLogVoxelEdge));
  else if(xLogVoxelEdge < 0)
    xBubbleStepB = (int)(0.5 + pow(10.0, -xLogVoxelEdge));

  // It is likely however that 0.1 is not an appropriate step size when min
  // voxel size is 0.99, so we try 0.5 and 0.2 as candidates
  if(xBubbleStepA * 5.0 / xBubbleStepB <= xMinVoxelEdge)
    xBubbleStepA *= 5;
  else if(xBubbleStepA * 2.0 / xBubbleStepB <= xMinVoxelEdge)
    xBubbleStepA *= 2;

  // Set the bubble min value
  double xBubbleStep = xBubbleStepA * 1.0 / xBubbleStepB;
  double xBubbleMin = xBubbleStep;

  // Set the default value so that it falls on the step boundary
  m_BubbleRadiusDefaultValue = floor(0.25 * xBubbleMax / xBubbleStep) * xBubbleStep;

  // Set the domain and value for the radius slider
  m_BubbleRadiusDomain.Set(xBubbleMin, xBubbleMax, xBubbleStep);

  // Let the GUI know that the values have changed
  InvokeEvent(BubbleDefaultRadiusUpdateEvent());
}


bool
SnakeWizardModel
::GetBubbleRadiusValueAndRange(
    double &value, NumericValueRange<double> *range)
{
  // Bail out if not in snake mode
  if(!m_Driver->IsSnakeModeActive())
    return false;

  int activeBubble;
  if(m_ActiveBubbleModel->GetValueAndDomain(activeBubble, NULL)
     && activeBubble >= 0)
    {
    // If a bubble is currently selected, we change the value of the
    // currently selected bubble
    value = m_Driver->GetBubbleArray()[activeBubble].radius;
    }
  else
    {
    // Otherwise, we return the default value computed for this image
    value = m_BubbleRadiusDefaultValue;
    }

  // Set the range as well
  if(range)
    *range = m_BubbleRadiusDomain;

  return true;
}

void
SnakeWizardModel
::SetBubbleRadiusValue(double value)
{
  int activeBubble;
  if(m_ActiveBubbleModel->GetValueAndDomain(activeBubble, NULL)
     && activeBubble >= 0)
    {
    // There is an active bubble - change its radius
    m_Driver->GetBubbleArray()[activeBubble].radius = value;
    InvokeEvent(BubbleListUpdateEvent());
    }

  // Always store as the default value
  m_BubbleRadiusDefaultValue = value;

  // Radius has updated
  InvokeEvent(BubbleDefaultRadiusUpdateEvent());
}

void SnakeWizardModel::OnEvolutionPageEnter()
{
  // Initialize the segmentation
  if(!m_Driver->InitializeActiveContourPipeline())
    {
    throw IRISException("Failed to initialize the active contour. "
                        "Check that the initialization bubbles are "
                        "present and cover the image region.");
    }
}

void SnakeWizardModel::PerformEvolutionStep()
{
  // Do the segmentation step!
  m_Driver->GetSNAPImageData()->RunSegmentation(m_StepSizeModel->GetValue());

  // Fire an event
  InvokeEvent(EvolutionIterationEvent());
}

int SnakeWizardModel::GetEvolutionIterationValue()
{
  if(m_Driver->IsSnakeModeActive() &&
     m_Driver->GetSNAPImageData()->IsSegmentationActive())
    {
    return m_Driver->GetSNAPImageData()->GetElapsedSegmentationIterations();
    }
  else return 0;
}

void SnakeWizardModel::OnEvolutionPageBack()
{
  if(m_Driver->GetSNAPImageData()->IsSegmentationActive())
    m_Driver->GetSNAPImageData()->TerminateSegmentation();
}

void SnakeWizardModel::OnEvolutionPageFinish()
{
  // Stop the segmentation pipeline
  if(m_Driver->GetSNAPImageData()->IsSegmentationActive())
    m_Driver->GetSNAPImageData()->TerminateSegmentation();

  // Update IRIS with SNAP images
  m_Driver->UpdateIRISWithSnapImageData(NULL);

  // Set an undo point
  m_Driver->StoreUndoPoint("Automatic Segmentation");

  // Return to IRIS mode
  m_Driver->SetCurrentImageDataToIRIS();
  m_Driver->ReleaseSNAPImageData();
}

void SnakeWizardModel::RewindEvolution()
{
  if(m_Driver->GetSNAPImageData()->IsSegmentationActive())
    m_Driver->GetSNAPImageData()->RestartSegmentation();

  // Fire an event
  InvokeEvent(EvolutionIterationEvent());
}


bool SnakeWizardModel
::GetNumberOfClustersValueAndRange(
    int &value, NumericValueRange<int> *range)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  if(uc)
    {
    value = uc->GetNumberOfClusters();
    if(range)
      range->Set(2, 20, 1);
    return true;
    }

  return false;
}

void SnakeWizardModel
::SetNumberOfClustersValue(
    int value)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  uc->SetNumberOfClusters(value);
  uc->InitializeClusters();
  this->TagGMMPreprocessingFilterModified();
  this->InvokeEvent(GMMModifiedEvent());
}

bool SnakeWizardModel::GetNumberOfGMMSamplesValueAndRange(int &value, NumericValueRange<int> *range)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  if(uc)
    {
    value = uc->GetNumberOfSamples();
    if(range)
      {
      int nvox = m_Driver->GetCurrentImageData()->GetMain()->GetNumberOfVoxels();
      range->Set(std::min(nvox, 5000), nvox, 5000);
      }

    return true;
    }

  return false;
}

void SnakeWizardModel::SetNumberOfGMMSamplesValue(int value)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  uc->SetNumberOfSamples(value);
  uc->InitializeClusters();
  this->TagGMMPreprocessingFilterModified();
  this->InvokeEvent(GMMModifiedEvent());
}

#include "PreprocessingFilterConfigTraits.h"
#include "GMMClassifyImageFilter.h"

void SnakeWizardModel::TagGMMPreprocessingFilterModified()
{
  // TODO: this is not the right way to do this! Make MixtureModel an itkObject
  // and an inout to the filter, so we don't have to update the filter itself!!
  // THIS IS HACKY!!!
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  typedef SlicePreviewFilterWrapper<GMMPreprocessingFilterConfigTraits>
                                            GMMPreprocessingPreviewWrapperType;
  GMMPreprocessingPreviewWrapperType *junk =
      (GMMPreprocessingPreviewWrapperType *) m_Driver->GetPreprocessingFilterPreviewer(PREPROCESS_GMM);
  junk->SetParameters(uc->GetMixtureModel());
}


void SnakeWizardModel::PerformClusteringIteration()
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  uc->Iterate();
  this->InvokeEvent(GMMModifiedEvent());

  TagGMMPreprocessingFilterModified();
}

bool SnakeWizardModel::SetClusterForegroundState(int cluster, bool state)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  GaussianMixtureModel *gmm = uc->GetMixtureModel();

  // Currently this implements mutually exclusive behavior
  if(state && !gmm->IsForeground(cluster))
    {
    for(int i = 0; i < gmm->GetNumberOfGaussians(); i++)
      {
      if(cluster == i)
        gmm->SetForeground(i);
      else
        gmm->SetBackground(i);
      }

    TagGMMPreprocessingFilterModified();
    this->InvokeEvent(GMMModifiedEvent());
    return true;
    }
  else
    {
    return false;
    }
}

bool SnakeWizardModel::SetClusterWeight(int cluster, double weight)
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  GaussianMixtureModel *gmm = uc->GetMixtureModel();

  if(weight != gmm->GetWeight(cluster))
    {
    gmm->SetWeightAndRenormalize(cluster, weight);

    TagGMMPreprocessingFilterModified();
    this->InvokeEvent(GMMModifiedEvent());
    return true;
    }
  else
    return false;
}

void SnakeWizardModel::ReinitializeClustering()
{
  UnsupervisedClustering *uc = m_Driver->GetClusteringEngine();
  assert(uc);

  uc->InitializeClusters();
  this->InvokeEvent(GMMModifiedEvent());

  TagGMMPreprocessingFilterModified();
}


int SnakeWizardModel::GetNumberOfComponentsForSegmentation()
{
  this->Update();
  return m_ComponentInfo.size();
}

SnakeWizardModel::ComponentInfo
SnakeWizardModel::GetLayerAndIndexForNthComponent(int n)
{
  this->Update();
  assert(n < m_ComponentInfo.size());
  return m_ComponentInfo[n];
}




