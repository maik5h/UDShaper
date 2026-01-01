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

void UDShaper::clearBuffer()
{
  // Aparently this is a good way to clear queues.
  std::queue<iplug::sample> emptyL;
  std::swap(mBufferL, emptyL);

  std::queue<iplug::sample> emptyR;
  std::swap(mBufferR, emptyR);

  mPOIOffsetL.clear();
  mPOIOffsetR.clear();
  mPOILevelL.clear();
  mPOILevelR.clear();
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

  bool isPlaying = GetTransportIsRunning();

  for (int i = 0; i < nFrames; i++)
  {
    // Value of the current sample after normalizing (left channel).
    // If the distortion mode is set to mid/side, this represents
    // the mid channel.
    iplug::sample inputSampleL = inputL[i];

    // Value of the current sample after normalizing (right channel).
    // If the distortion mode is set to mid/side, this represents
    // the side channel.
    iplug::sample inputSampleR = inputR[i];

    // If normalization is active, load input audio to buffers, load
    // front sample from buffer and apply normalization.
    if (mNormalize)
    {
      // ----- Load to buffer -----
      // Transform to mid/side if necessary.
      if (mMode == distortionMode::midSide)
      {
        mBufferL.push((inputL[i] + inputR[i]) * 0.5);
        mBufferR.push((inputL[i] - inputR[i]) * 0.5);
      }
      else
      {
        mBufferL.push(inputL[i]);
        mBufferR.push(inputR[i]);
      }

      // ----- Check for POIs -----
      // Check for changes in the sign or direction and update POI buffer.
      // Samples that are zero do not need to be normalized and can be excluded
      // from sign/ direction evaluation.
      bool signChangeL = (mBackSampleL.previousLevel != 0) && (mBackSampleL.isPositive != (mBufferL.back() > 0));
      bool directionChangeL = (mBackSampleL.previousLevel != mBufferL.back()) && (mBackSampleL.isIncreasing != (mBufferL.back() > mBackSampleL.previousLevel));

      // Points can have both a change in sign and direction (e.g. at points
      // where a saw wave jumps). If this happens, the level should be written
      // to the POIAmp buffer. If only the sign changes, write zero.
      if (directionChangeL)
      {
        mPOILevelL.push_back(mBackSampleL.previousLevel);
        mBackSampleL.isIncreasing = mBufferL.back() > mBackSampleL.previousLevel;
        mBackSampleL.isPositive = mBufferL.back() > 0;
        mPOIOffsetL.push_back(mPOIOffsetCountL);
        mPOIOffsetCountL = 0;
      }
      else if (signChangeL)
      {
        mPOILevelL.push_back(0.);
        mBackSampleL.isPositive = mBufferL.back() > 0;
        mPOIOffsetL.push_back(mPOIOffsetCountL);
        mPOIOffsetCountL = 0;
      }

      bool signChangeR = (mBackSampleR.previousLevel != 0) && ((mBackSampleR.isPositive) != (mBufferR.back() > 0));
      bool directionChangeR = (mBackSampleR.previousLevel != mBufferR.back()) && (mBackSampleR.isIncreasing != mBufferR.back() > mBackSampleR.previousLevel);
      if (directionChangeR)
      {
        mPOILevelR.push_back(mBackSampleR.previousLevel);
        mBackSampleR.isIncreasing = mBufferR.back() > mBackSampleR.previousLevel;
        mBackSampleR.isPositive = mBufferR.back() > 0;
        mPOIOffsetR.push_back(mPOIOffsetCountR);
        mPOIOffsetCountR = 0;
      }
      else if (signChangeR)
      {
        mPOILevelR.push_back(0.);
        mBackSampleR.isPositive = mBufferR.back() > 0;
        mPOIOffsetR.push_back(mPOIOffsetCountR);
        mPOIOffsetCountR = 0;
      }

      // Count the samples since the last POI.
      // The offset can not be larger than the buffer size.
      if (mPOIOffsetCountL < LATENCY_NORMALIZE)
      {
        mPOIOffsetCountL++;
      }
      if (mPOIOffsetCountR < LATENCY_NORMALIZE)
      {
        mPOIOffsetCountR++;
      }

      mBackSampleL.previousLevel = mBufferL.back();
      mBackSampleR.previousLevel = mBufferR.back();

      // ----- Update POIs at front of buffer -----
      // Only process if enough samples have been loaded to the buffers.
      if (mBufferL.size() > LATENCY_NORMALIZE)
      {
        inputSampleL = mBufferL.front();
        inputSampleR = mBufferR.front();
        mBufferL.pop();
        mBufferR.pop();

        // Apply normalization to this sample.
        // Count down until the next POI index is reached and update mNormL and
        // mNormR.
        if (!mPOIOffsetL.empty() && --mPOIOffsetL.front() <= 0)
        {
          // The level of the next sample is the second element in mPOILevel.
          if (mPOIOffsetL.size() > 1)
          {
            mNormL.setNextLevel(mPOILevelL.at(1));
            mPOILevelL.pop_front();
            mPOIOffsetL.pop_front();
            mFrontSampleL.isIncreasing = mNormL.isIncreasing();
          }
        }
        if (!mPOIOffsetR.empty() && --mPOIOffsetR.front() <= 0)
        {
          if (mPOIOffsetR.size() > 1)
          {
            mNormR.setNextLevel(mPOILevelR.at(1));
            mPOILevelR.pop_front();
            mPOIOffsetR.pop_front();
            mFrontSampleR.isIncreasing = mNormR.isIncreasing();
          }
        }

        inputSampleL = mNormL.normalize(inputSampleL);
        inputSampleR = mNormR.normalize(inputSampleR);
      }
      else
      {
        continue;
      }
    }

    // If normalization is not used, still determine the increasing state to
    // process up/down distortion properly.
    else
    {
      if ((mFrontSampleL.previousLevel != inputL[i]) && (mFrontSampleL.isIncreasing != inputL[i] > mFrontSampleL.previousLevel))
      {
        mFrontSampleL.isIncreasing = !mFrontSampleL.isIncreasing;
      }

      if ((mFrontSampleR.previousLevel != inputR[i]) && (mFrontSampleR.isIncreasing != inputR[i] > mFrontSampleR.previousLevel))
      {
        mFrontSampleR.isIncreasing = !mFrontSampleR.isIncreasing;
      }
    }

    // Do not process modulation unless the host is playing.
    if (isPlaying)
    {
      modulationStep(modAmps, beatPosition, secondsPlayed);
    }

    // ----- Process audio -----
    switch (mMode)
    {
      // Distortion mode Up/Down:
      // Use shapeEditor1 on samples that have higher or equal value than the
      // previous sample, else use shapeEditor2.
      case upDown:
      {
        // Forward (normalized) input values to the corresponding ShapeEditors.
        if (mFrontSampleL.isIncreasing)
        {
          outputL[i] = shapeEditor1.forward(inputSampleL, modAmps);
        }
        else
        {
          outputL[i] = shapeEditor2.forward(inputSampleL, modAmps);
        }

        if (mFrontSampleR.isIncreasing)
        {
          outputR[i] = shapeEditor1.forward(inputSampleR, modAmps);
        }
        else
        {
          outputR[i] = shapeEditor2.forward(inputSampleR, modAmps);
        }

        if (mNormalize)
        {
          outputL[i] = mNormL.revertNormalize(outputL[i]);
          outputR[i] = mNormR.revertNormalize(outputR[i]);
        }

        mFrontSampleL.previousLevel = inputSampleL;
        mFrontSampleR.previousLevel = inputSampleR;
        
        break;
      }

      // Distortion mode Mid/Side:
      // Use shapeEditor1 on the mid-, shapeEditor2 on the side-channel.
      case midSide:
      {
        iplug::sample mid = 0;
        iplug::sample side = 0;

        // If normalization is used, left/right has already been transformed
        // to mid/side in the buffer. If not, transform here.
        if (mNormalize)
        {
          mid = shapeEditor1.forward(inputSampleL, modAmps);
          side = shapeEditor2.forward(inputSampleR, modAmps);

          mid = mNormL.revertNormalize(mid);
          side = mNormR.revertNormalize(side);
        }
        else
        {
          mid = shapeEditor1.forward((inputSampleL + inputSampleR) * 0.5, modAmps);
          side = shapeEditor2.forward((inputSampleL - inputSampleR) * 0.5, modAmps);
        }

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

        if (mNormalize)
        {
          outputL[i] = mNormL.revertNormalize(outputL[i]);
          outputR[i] = mNormR.revertNormalize(outputR[i]);
        }

        break;
      }

      // Distortion mode +/-:
      // Use shapeEditor one on positive, shapeEditor2 on negative samples.
      case positiveNegative:
      {
        outputL[i] = (inputSampleL > 0) ? shapeEditor1.forward(inputSampleL, modAmps) : shapeEditor2.forward(inputSampleL, modAmps);
        outputR[i] = (inputSampleR > 0) ? shapeEditor1.forward(inputSampleR, modAmps) : shapeEditor2.forward(inputSampleR, modAmps);

        if (mNormalize)
        {
          outputL[i] = mNormL.revertNormalize(outputL[i]);
          outputR[i] = mNormR.revertNormalize(outputR[i]);
        }

        break;
      }
    }
  }
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

      if (mNormalize)
      {
        SetLatency(LATENCY_NORMALIZE);
      }
      else
      {
        SetLatency(0);
        clearBuffer();
      }
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

void UDShaper::OnReset()
{
  clearBuffer();

  mPOIOffsetCountL = 0;
  mPOIOffsetCountR = 0;
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