#include "UDShaper.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include "src/GUILayout.h"
#include "src/assets.h"

UDShaper::UDShaper(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  layout.setCoordinates(PLUG_WIDTH, PLUG_HEIGHT);

  IParam* param = GetParam(distMode);
  param->InitEnum("Distortion mode", 0, 4);
  param->SetDisplayText(0, "Up/Down");
  param->SetDisplayText(1, "Left/Right");
  param->SetDisplayText(2, "Mid/Side");
  param->SetDisplayText(3, "+/-");

  param = GetParam(EParams::normalize);
  param->InitBool("Piecewise input normalization", false);

  param = GetParam(activeLFOIdx);
  param->InitInt("Active LFO", 0, 0, MAX_NUMBER_LFOS);

  // Add parameters for the LFOs.
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    std::string parameterName = "LFO " + std::to_string(i + 1) + " loop mode";
    param = GetParam(getLFOParameterIndex(i, mode));
    param->InitEnum(parameterName.c_str(), 0, 2);
    param->SetDisplayText(0, "Tempo");
    param->SetDisplayText(1, "Seconds");

    parameterName = "LFO " + std::to_string(i + 1) + " frequency (beats)";
    param = GetParam(getLFOParameterIndex(i, freqTempo));
    param->InitInt(parameterName.c_str(), 6, 0, 12);
    for (int j = 0; j <= 12; j++)
    {
      param->SetDisplayText(j, FREQUENCY_COUNTER_STRINGS[j].c_str());
    }

    parameterName = "LFO " + std::to_string(i + 1) + " frequency (Seconds)";
    param = GetParam(getLFOParameterIndex(i, freqSeconds));
    param->InitSeconds(parameterName.c_str(), 1., 0.05, 60, 0.05);
  }

  // Add the modulation amount parameters. Every LFO can link to MAX_MODULATION_LINKS parameters
  // and each link has a coefficient multiplied to the LFO amplitude, which is a parameter.
  for (int i = 0; i < MAX_NUMBER_LFOS * MAX_MODULATION_LINKS; i++)
  {
    int modIdx = i % MAX_MODULATION_LINKS;
    int LFOIdx = (i - modIdx) / MAX_MODULATION_LINKS;
    std::string name = "LFO " + std::to_string(LFOIdx + 1) +" mod " + std::to_string(modIdx + 1) + " amount";
    param = GetParam(EParams::modStart + i);
    param->InitDouble(name.c_str(), 0., -1., 1., 0.01);
  }

  // Inform the LFOController about the default parameter values.
  LFOs.refreshInternalState();

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(UDS_GREY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    IRECT b = pGraphics->GetBounds();

    // Draw a frame around the two ShapeEditors.
    pGraphics->AttachControl(new DrawFrame(layout.editorFrameRect, UDS_BLACK, FRAME_WIDTH_NARROW, ALPHA_SHADOW));

    // Draw 3D frames around ShapeEditors and LFOController.
    LFOControlLayout LFOLayout = LFOControlLayout(layout.LFORect, b.R, b.B);
    pGraphics->AttachControl(new Frame3D(shapeEditor1.layout.fullRect, FRAME_WIDTH, UDS_GREY));
    pGraphics->AttachControl(new Frame3D(shapeEditor2.layout.fullRect, FRAME_WIDTH, UDS_GREY));
    pGraphics->AttachControl(new Frame3D(LFOLayout.editorFullRect, FRAME_WIDTH, UDS_GREY));

    menuBar.attachUI(pGraphics);
    shapeEditor1.attachUI(pGraphics);
    shapeEditor2.attachUI(pGraphics);
    LFOs.attachUI(pGraphics);

    // Reveal the modulationAmplitudesUI array to the ShapeEditors, which will use it to render the current curve.
    static_cast<ShapeEditorControl*>(GetUI()->GetControlWithTag(ShapeEditorControl1))->modulationAmplitudes = modulationAmplitudesUI;
    static_cast<ShapeEditorControl*>(GetUI()->GetControlWithTag(ShapeEditorControl2))->modulationAmplitudes = modulationAmplitudesUI;

    pGraphics->EnableMouseOver(true);
  };
#endif
}

#if IPLUG_DSP
void UDShaper::modulationStep(double (&modAmps)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS], double& beatPosition, double& seconds)
{
  // Calculate modulation amplitudes at current time step.
  LFOs.getModulationAmplitudes(beatPosition, seconds, modAmps, modulationAmounts);

  // Increase time by one sample.
  beatPosition += GetTempo() / GetSampleRate() / 60;
  seconds += 1. / GetSampleRate();
}

void UDShaper::findPOI(iplug::sample* audio, int nFrames, NormalizeInfo& info)
{
  // Check if we are in a increasing or decreasing segment.
  bool increasing = audio[info.idx] < audio[info.idx + 1];

  // Check if we are above or below zero.
  bool negative = audio[info.idx] < 0;

  // Check the following input samples until a spot where the
  // derivative or the sign changes is found.
  while (info.idx < nFrames - 1)
  {
    info.idx++;

    if (increasing != (audio[info.idx] < audio[info.idx + 1]))
    {
      info.ampPrev = info.ampNext;
      info.ampNext = audio[info.idx];
      break;
    }
    if (negative != (audio[info.idx] < 0))
    {
      info.ampPrev = info.ampNext;
      info.ampNext = 0.f;
      break;
    }
  }
}

float UDShaper::normalizeSample(iplug::sample sample, NormalizeInfo info)
{
  if (info.ampPrev == info.ampNext)
  {
    return sample;
  }

  if (sample >= 0)
  {
    return (sample - info.getLower()) / (info.getUpper() - info.getLower());
  }
  else
  {
    return (sample - info.getUpper()) / (info.getUpper() - info.getLower());
  }
}

float UDShaper::revertNormalizeSample(iplug::sample sample, NormalizeInfo info)
{
  if (info.ampPrev == info.ampNext)
  {
    return sample;
  }

  if (sample >= 0)
  {
    return sample * (info.getUpper() - info.getLower()) + info.getLower();
  }
  else
  {
    return sample * (info.getUpper() - info.getLower()) + info.getUpper();
  }
}

void UDShaper::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  sample* inputL = inputs[0];
  sample* inputR = inputs[1];

  sample* outputL = outputs[0];
  sample* outputR = outputs[1];

  double beatPosition = GetPPQPos();
  double secondsPlayed = GetSamplePos() / GetSampleRate();

  // Fetch the state of all modulation amplitudes at the given host beatPosition or time.
  double modAmps[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

  // Store information about the position and amplitudes of extrema within
  // the input array.
  NormalizeInfo normL;
  NormalizeInfo normR;
  normL.ampNext = mPreviousBlockPeakL;
  normR.ampNext = mPreviousBlockPeakR;

  // The amplitude of the previous sample (left channel).
  iplug::sample previousLevelL = mPreviousBlockLevelL;

  // The amplitude of the previous sample (right channel).
  iplug::sample previousLevelR = mPreviousBlockLevelR;

  for (int i = 0; i < nFrames; i++)
  {
    modulationStep(modAmps, beatPosition, secondsPlayed);

    // Value of the current sample after normalizing (left channel).
    iplug::sample inputSampleL = inputL[i];

    // Value of the current sample after normalizing (right channel).
    iplug::sample inputSampleR = inputR[i];

    // Gather information needed to normalize the input audio piecewise.
    if (mNormalize && (i < nFrames - 1))
    {
      // Find the next peak if the previous one has been reached.
      if (i == normL.idx)
      {
        findPOI(inputL, nFrames, normL);
      }
      if (i == normR.idx)
      {
        findPOI(inputR, nFrames, normR);
      }
    }

    // Apply the normalization to this sample.
    if (mNormalize)
    {
      inputSampleL = normalizeSample(inputL[i], normL);
      inputSampleR = normalizeSample(inputR[i], normR);
    }

    // Process the audio based on the distortion mode.
    switch (mMode)
    {
      // Distortion mode Up/Down:
      // Use shapeEditor1 on samples that have higher or equal value than the
      // previous sample, else use shapeEditor2.
      case upDown:
      {
        // Forward (normalized) input values to the corresponding ShapeEditors.
        if (inputL[i] >= previousLevelL)
        {
          outputL[i] = shapeEditor1.forward(inputSampleL, modAmps);
        }
        else
        {
          outputL[i] = shapeEditor2.forward(inputSampleL, modAmps);
        }

        if (inputR[i] >= previousLevelR)
        {
          outputR[i] = shapeEditor1.forward(inputSampleR, modAmps);
        }
        else
        {
          outputR[i] = shapeEditor2.forward(inputSampleR, modAmps);
        }

        previousLevelL = inputL[i];
        previousLevelR = inputR[i];
        
        break;
      }

      // Distortion mode Mid/Side:
      // Use shapeEditor1 on the mid-, shapeEditor2 on the side-channel.
      case midSide:
      {
        iplug::sample mid = shapeEditor1.forward((inputSampleL + inputSampleR) / 2, modAmps);
        iplug::sample side = shapeEditor2.forward((inputSampleL - inputSampleR) / 2, modAmps);

        outputL[i] = mid + side;
        outputR[i] = mid - side;

        break;
      }

      // Distortion mode Left/Right:
      // Use shapeEditor1 on the left, shapeEditor2 on the right channel.
      case leftRight:
      {
        outputL[i] = shapeEditor1.forward(inputSampleL, modAmps);
        outputR[i] = shapeEditor2.forward(inputSampleR, modAmps);

        break;
      }

      // Distortion mode +/-:
      // Use shapeEditor one on positive, shapeEditor2 on negative samples.
      case positiveNegative:
      {
        outputL[i] = (inputSampleL > 0) ? shapeEditor1.forward(inputSampleL, modAmps) : shapeEditor2.forward(inputSampleL, modAmps);
        outputR[i] = (inputSampleR > 0) ? shapeEditor1.forward(inputSampleR, modAmps) : shapeEditor2.forward(inputSampleR, modAmps);

        break;
      }
    }

    // If the audio has been normalized before, revert the normalization after
    // shaping.
    if (mNormalize)
    {
      outputL[i] = revertNormalizeSample(outputL[i], normL);
      outputR[i] = revertNormalizeSample(outputR[i], normR);
    }
  }

  // Save the amplitudes of the last sample and peak/ zero point for the next block.
  mPreviousBlockLevelL = previousLevelL;
  mPreviousBlockLevelR = previousLevelR;

  mPreviousBlockPeakL = normL.ampPrev;
  mPreviousBlockPeakR = normR.ampPrev;
}
#endif

void UDShaper::OnParamChange(int idx)
{
  if (idx < EParams::kNumParams)
  {
    if (idx == EParams::distMode)
    {
      mMode = static_cast<distortionMode>(GetParam(idx)->Value());
    }
    if (idx == EParams::normalize)
    {
      mNormalize = GetParam(idx)->Value();
    }
    if (idx == activeLFOIdx)
    {
      int newLFOIdx = GetParam(idx)->Value();

      // Inform the LFOController about the new active LFO.
      // This connects controls with the new LFO in the background.
      LFOs.setActiveLFO(newLFOIdx);
      SendCurrentParamValuesFromDelegate();

      // The new LFO might have a different loop mode selected than the previous.
      // The LFOController must be informed of the new loop mode to update the UI.
      LFOLoopMode loopMode = static_cast<LFOLoopMode>(GetParam(getLFOParameterIndex(newLFOIdx, mode))->Value());
      LFOs.setLoopMode(loopMode);
    }

    // Check for changes of the loop mode for any of the LFOs.
    if (EParams::LFOsStart <= idx && idx < EParams::modStart)
    {
      // The index within the set of LFO parameters.
      int i = idx - LFOsStart;

      // If an LFO loopMode has been changed, update the frequency panel.
      if (i % kNumLFOParams == LFOParams::mode)
      {
        LFOLoopMode loopMode = static_cast<LFOLoopMode>(GetParam(idx)->Value());
        LFOs.setLoopMode(loopMode);
      }
      else if (i % kNumLFOParams == LFOParams::freqTempo)
      {
        LFOs.setFrequencyValue(LFOLoopMode::LFOFrequencyTempo, GetParam(idx)->Value());
      }
      else if (i % kNumLFOParams == LFOParams::freqSeconds)
      {
        LFOs.setFrequencyValue(LFOLoopMode::LFOFrequencySeconds, GetParam(idx)->Value());
      }
    }

    // Udpade the internal modulation amount state if a link knob has been changed.
    else if (idx >= EParams::modStart)
    {
      modulationAmounts[idx - EParams::modStart] = GetParam(idx)->Value();
    }
  }
}

bool UDShaper::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  // Some messages can be forwarded directly to the ShapeEditorControls.
  if (
    (msgTag == EControlMsg::LFOConnectInit)
    || (msgTag == EControlMsg::LFOConnectAttempt)
    || (msgTag == EControlMsg::linkKnobMouseOver)
    || (msgTag == EControlMsg::linkKnobMouseOut)
    )
  {
    SendControlMsgFromDelegate(EControlTags::ShapeEditorControl1, msgTag, dataSize, pData);
    SendControlMsgFromDelegate(EControlTags::ShapeEditorControl2, msgTag, dataSize, pData);
    return true;
  }

  // Enable an LFO modulation link if connecting was successfull.
  else if (msgTag == EControlMsg::LFOConnectSuccess)
  {
    int connectedIdx = *static_cast<const int*>(pData);
    LFOs.setLinkActive(connectedIdx, true);
  }
  else if (msgTag == EControlMsg::LFODisconnect)
  {
    int linkIdx = *static_cast<const int*>(pData);

    // Disable the link on all concerned elements.
    shapeEditor1.disconnectLink(linkIdx);
    shapeEditor2.disconnectLink(linkIdx);
    LFOs.setLinkActive(linkIdx, false);

    // Reset the modulation amount to zero.
    GetParam(EParams::modStart + linkIdx)->Set(0.);
    modulationAmounts[linkIdx] = 0.;
  }

  // If a point has been deleted, set the modulation links it has been connected to inactive.
  else if (msgTag == EControlMsg::editorPointDeleted)
  {
    // If pData is nullptr, no LFO was connected to this point and no action is
    // required.
    if (pData)
    {
      const int* indices = static_cast<const int*>(pData);
      for (int i = 0; i < dataSize; i++)
      {
        LFOs.setLinkActive(*(indices + i), false);
        GetParam(EParams::modStart + *(indices + i))->Set(0.);
        modulationAmounts[*(indices + i)] = 0.;
      }
    }
  }
  return false;
}

void UDShaper::OnIdle()
{
  // Refresh the UI modulation amplitudes used for rendering.
  const double beatPosition = GetPPQPos();
  const double secondsPlayed = GetSamplePos() / GetSampleRate();
  LFOs.getModulationAmplitudes(beatPosition, secondsPlayed, modulationAmplitudesUI, modulationAmounts);

  // TODO for testing only
  GetUI()->GetControlWithTag(ShapeEditorControl1)->SetDirty(false);
  GetUI()->GetControlWithTag(ShapeEditorControl2)->SetDirty(false);
}

bool UDShaper::SerializeState(IByteChunk& chunk) const
{
  int version = PLUG_VERSION_HEX;
  chunk.Put(&version);

  shapeEditor1.serializeState(chunk);
  shapeEditor2.serializeState(chunk);
  LFOs.serializeState(chunk);

  return SerializeParams(chunk);
}

int UDShaper::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int version = 0;
  startPos = chunk.Get(&version, startPos);

  startPos = shapeEditor1.unserializeState(chunk, startPos, version);
  startPos = shapeEditor2.unserializeState(chunk, startPos, version);
  startPos = LFOs.unserializeState(chunk, startPos, version);

  // LFOs must be informed which links are active.
  std::set<int> activeLinks = {};
  shapeEditor1.getLinks(activeLinks);
  shapeEditor2.getLinks(activeLinks);

  for (int link : activeLinks)
  {
    LFOs.setLinkActive(link);
  }

  startPos = UnserializeParams(chunk, startPos);

  return startPos;
}