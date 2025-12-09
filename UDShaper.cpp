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

  param = GetParam(activeLFOIdx);
  param->InitInt("Active LFO", 0, 0, MAX_NUMBER_LFOS);

  // Add parameters for the LFOs.
  for (int i = 0; i < MAX_NUMBER_LFOS; i++)
  {
    int test = getLFOParameterIndex(i, mode);
    param = GetParam(getLFOParameterIndex(i, mode));
    param->InitEnum("LFO loop mode", 0, 2);
    param->SetDisplayText(0, "Tempo");
    param->SetDisplayText(1, "Seconds");

    param = GetParam(getLFOParameterIndex(i, freqTempo));
    param->InitInt("LFOrfequency (beats)", 6, 0, 12);

    param = GetParam(getLFOParameterIndex(i, freqSeconds));
    param->InitSeconds("LFO frequency (seconds)", 1., 0.05, 60, 0.05);
  }

  // Add the modulation amount parameters. Every LFO can link to MAX_MODULATION_LINKS parameters
  // and each link has a coefficient multiplied to the LFO amplitude, which is a parameter.
  for (int i = 0; i < MAX_NUMBER_LFOS * MAX_MODULATION_LINKS; i++)
  {
    param = GetParam(EParams::modStart + i);
    param->InitDouble("", 0., -1., 1., 0.01);
  }

  // Inform the LFOController about the default parameter values.
  LFOs.refreshInternalState();

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
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

  switch (static_cast<distortionMode>(GetParam(EParams::distMode)->Value()))
  {
  // TODO Buffer is parallelized into pieces of ~200 samples. I dont know how to access all of them in
  // one place so i can not properly decide which shape to choose.

  // Distortion mode Up/Down:
  // Use shapeEditor1 on samples that have higher or equal value than the previous sample, else use
  // shapeEditor2.
  case upDown: {
    // Since data from last buffer is not available, take first samples as a reference, works most
    // of the time but is not optimal.
    bool processShape1L = (inputL[1] >= inputL[0]);
    bool processShape1R = (inputR[1] >= inputR[0]);

    float previousLevelL;
    float previousLevelR;

    for (uint32_t index = 0; index < nFrames; index++)
    {
      modulationStep(modAmps, beatPosition, secondsPlayed);

      if (index > 0)
      {
        processShape1L = (inputL[index] >= previousLevelL);
        processShape1R = (inputR[index] >= previousLevelR);
      }

      outputL[index] = processShape1L ? shapeEditor1.forward(inputL[index], modAmps) : shapeEditor2.forward(inputL[index], modAmps);
      outputR[index] = processShape1R ? shapeEditor1.forward(inputR[index], modAmps) : shapeEditor2.forward(inputR[index], modAmps);

      previousLevelL = inputL[index];
      previousLevelR = inputR[index];
    }
    break;
  }

  // Distortion mode Mid/Side:
  // Use shapeEditor1 on the mid-, shapeEditor2 on the side-channel.
  case midSide: {
    float mid;
    float side;

    for (uint32_t index = 0; index < nFrames; index++)
    {
      modulationStep(modAmps, beatPosition, secondsPlayed);

      mid = shapeEditor1.forward((inputL[index] + inputR[index]) / 2, modAmps);
      side = shapeEditor2.forward((inputL[index] - inputR[index]) / 2, modAmps);

      outputL[index] = mid + side;
      outputR[index] = mid - side;
    }
    break;
  }

  // Distortion mode Left/Right:
  // Use shapeEditor1 on the left, shapeEditor2 on the right channel.
  case leftRight: {
    for (uint32_t index = 0; index < nFrames; index++)
    {
      modulationStep(modAmps, beatPosition, secondsPlayed);

      outputL[index] = shapeEditor1.forward(inputL[index], modAmps);
      outputR[index] = shapeEditor2.forward(inputR[index], modAmps);
    }
    break;
  }

  // Distortion mode +/-:
  // Use shapeEditor one on positive, shapeEditor2 on negative samples.
  case positiveNegative: {
    for (uint32_t index = 0; index < nFrames; index++)
    {
      modulationStep(modAmps, beatPosition, secondsPlayed);

      outputL[index] = (inputL[index] > 0) ? shapeEditor1.forward(inputL[index], modAmps) : shapeEditor2.forward(inputL[index], modAmps);
      outputR[index] = (inputR[index] > 0) ? shapeEditor1.forward(inputR[index], modAmps) : shapeEditor2.forward(inputR[index], modAmps);
    }
    break;
  }
  }
}
#endif

void UDShaper::OnParamChange(int idx)
{
  if (idx < EParams::kNumParams)
  {
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
  if (msgTag == EControlMsg::LFOAttemptConnect)
  {
    // Forward attempted LFO connection to the two ShapeEditors.
    GetUI()->GetDelegate()->SendControlMsgFromDelegate(EControlTags::ShapeEditorControl1, LFOAttemptConnect, dataSize, pData);
    GetUI()->GetDelegate()->SendControlMsgFromDelegate(EControlTags::ShapeEditorControl2, LFOAttemptConnect, dataSize, pData);
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
    int knobIdx = *static_cast<const int*>(pData);
    int linkIdx = static_cast<int>(GetParam(EParams::activeLFOIdx)->Value()) * MAX_MODULATION_LINKS + knobIdx;

    // Disable the link on all concerned elements.
    shapeEditor1.disconnectLink(linkIdx);
    shapeEditor2.disconnectLink(linkIdx);
    LFOs.setLinkActive(linkIdx, false);

    // Reset the modulation amount to zero.
    GetParam(EParams::modStart + linkIdx)->Set(0.);
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