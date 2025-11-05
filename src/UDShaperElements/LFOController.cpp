
#include "LFOController.h"

SecondsBoxControl::SecondsBoxControl(IRECT rect, int parameterIdx)
  : IVNumberBoxControl(rect, parameterIdx, nullptr, "", DEFAULT_STYLE, false, 1., 0.05, 60., "%.2f s", false)
{
  // TODO Why can I not change the idle background color??

  mSmallIncrement = 0.05;
  mLargeIncrement = 0.1;
}

void SecondsBoxControl::OnValueChanged(bool preventAction)
{
  mRealValue = Clip(mRealValue, mMinValue, mMaxValue);

  // Snap to .5 before displaying.
  double outValue = static_cast<double>(static_cast<int>(mRealValue * 20)) / 20;

  // Make increments smaller for small values.
  if (mRealValue <= 1)
  {
    mLargeIncrement = 0.01;
  }
  else if (mRealValue <= 10)
  {
    mLargeIncrement = 0.05;
  }
  else if (mRealValue <= 30)
  {
    mLargeIncrement = 0.1;
  }
  else
  {
    mLargeIncrement = 0.2;
  }

  if (mRealValue >= 1)
  {
    mTextReadout->SetStrFmt(32, fmtSeconds.Get(), outValue);
  }
  else
  {
    mTextReadout->SetStrFmt(32, fmtMilliseconds.Get(), 1000 * outValue);
  }

  if (!preventAction && GetParam())
  {
    SetValue(GetParam()->ToNormalized(outValue));
  }

  SetDirty(!preventAction);
}

void SecondsBoxControl::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  // Mousewheel is always using high precision.
  double inc = (d > 0.f ? 1. : -1.) * mSmallIncrement;
  mRealValue += inc;
  OnValueChanged();
}

void SecondsBoxControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  double gearing = IsFineControl(mod, true) ? mSmallIncrement : mLargeIncrement;
  mRealValue -= (double(dY) * gearing);
  OnValueChanged();
}

BeatsBoxControl::BeatsBoxControl(IRECT rect, int parameterIdx)
  : IVNumberBoxControl(rect, parameterIdx, nullptr, "", DEFAULT_STYLE, false, 6., 0., 12., "1/1", false)
{
  mSmallIncrement = 0.05;
  mLargeIncrement = 1.;
}

void BeatsBoxControl::OnValueChanged(bool preventAction)
{
  mRealValue = Clip(mRealValue, mMinValue, mMaxValue);

  // Value is casted to index which defines which of the predefined values to choose from.
  int idx = static_cast<int>(mRealValue);
  mTextReadout->SetStr(FREQUENCY_COUNTER_STRINGS[idx].data());

  if (!preventAction && GetParam())
  {
    SetValue(GetParam()->ToNormalized(idx));
  }

  SetDirty(!preventAction);
}

void BeatsBoxControl::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  // Mousewheel always increases/ decreases by one full step.
  double inc = (d > 0.f ? 1. : -1.) * mLargeIncrement;
  mRealValue += inc;
  OnValueChanged();
}

void BeatsBoxControl::OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod)
{
  mRealValue -= (double(dY) * mSmallIncrement);
  OnValueChanged();
}

FrequencyPanel::FrequencyPanel(LFOLoopMode initMode, double initValue)
{
  currentLoopMode = initMode;
  counterValue.at(initMode) = initValue;

  // These are set in setCoordinates, since the available rect must be known.
  modeControl = nullptr;
  freqBeatsControl = nullptr;
  freqSecondsControl = nullptr;
}

void FrequencyPanel::setCoordinates(IRECT rect, float GUIWidth, float GUIHeight)
{
  layout.setCoordinates(rect, GUIWidth, GUIHeight);
  modeControl = new ICaptionControl(layout.modeButtonRect, getLFOParameterIndex(0, mode), IText(24.f), DEFAULT_FGCOLOR, false);
  freqBeatsControl = new BeatsBoxControl(layout.counterRect, getLFOParameterIndex(0, freqTempo));
  freqSecondsControl = new SecondsBoxControl(layout.counterRect, getLFOParameterIndex(0, freqSeconds));
}

double FrequencyPanel::getLFOPhase(double beatPosition, double secondsPlayed)
{
  // Convert from beats to bars.
  beatPosition /= 4;

  if (currentLoopMode == LFOFrequencyTempo)
  {
    // TODO this needs to be checked once audio is available.
    double speed = pow(2, counterValue.at(LFOFrequencyTempo) - 6);
    beatPosition = fmod(beatPosition, 1. / speed);
    return beatPosition * speed;
  }
  else if (currentLoopMode == LFOFrequencySeconds)
  {
    return fmod(secondsPlayed, counterValue.at(LFOFrequencySeconds)) / counterValue.at(LFOFrequencySeconds);
  }
  return 0.;
}

// Saves the FrequencyPanel state:
//  - (LFOLoopMode) currentLoopMode
//  counterValue state:
//      - (LFOLoopMode) mode
//      - (double) value
//      for every possible mode
//bool FrequencyPanel::saveState(const clap_ostream_t* stream)
//{
//  if (stream->write(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode))
//    return false;
//  for (auto& value : counterValue)
//  {
//    if (stream->write(stream, &value.first, sizeof(LFOLoopMode)) != sizeof(LFOLoopMode))
//      return false;
//    if (stream->write(stream, &value.second, sizeof(double)) != sizeof(double))
//      return false;
//  }
//  return true;
//}
//
//// Reads the FrequencyPanel state depending on the version the state was saved in.
//bool FrequencyPanel::loadState(const clap_istream_t* stream, int version[3])
//{
//  if (version[0] == 1 && version[1] == 0 && version[2] == 0)
//  {
//    if (stream->read(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode))
//      return false;
//    // In version 1.0.0 two LFOLoopModes are availbale.
//    for (int i = 0; i < 2; i++)
//    {
//      LFOLoopMode mode;
//      double value;
//
//      if (stream->read(stream, &mode, sizeof(LFOLoopMode)) != sizeof(LFOLoopMode))
//        return false;
//      if (stream->read(stream, &value, sizeof(double)) != sizeof(double))
//        return false;
//
//      counterValue[mode] = value;
//    }
//  }
//  return true;
//}

void FrequencyPanel::attachUI(IGraphics* pGraphics)
{
  assert(!layout.fullRect.Empty());

  pGraphics->AttachControl(modeControl, kNoTag, "LFOControls");
  pGraphics->AttachControl(freqSecondsControl, kNoTag, "LFOControls");
  pGraphics->AttachControl(freqBeatsControl, kNoTag, "LFOControls");

  // The controls are modified between instantiation and attaching to graphics,
  // which aparently confuses iPlug. This call makes them render correctly.
  setLoopMode(currentLoopMode);
}

void FrequencyPanel::setDisabled(bool disable)
{
  modeControl->SetDisabled(disable);
  freqSecondsControl->SetDisabled(disable);
  freqBeatsControl->SetDisabled(disable);
}

void FrequencyPanel::setLoopMode(LFOLoopMode mode)
{
  currentLoopMode = mode;

  // Disable all elements.
  setDisabled(true);

  // Enable needed elements:
  // mode control and the control corresponding to the current modes frequency.
  modeControl->SetDisabled(false);

  if (mode == LFOFrequencySeconds)
  {
    freqSecondsControl->SetDisabled(false);
  }
  else if (mode == LFOFrequencyTempo)
  {
    freqBeatsControl->SetDisabled(false);
  }
}

void FrequencyPanel::setValue(LFOLoopMode mode, double value)
{
  counterValue.at(mode) = value;
}