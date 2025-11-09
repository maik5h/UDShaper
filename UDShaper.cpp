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
  };
#endif
}

#if IPLUG_DSP
void UDShaper::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  sample* inputL = inputs[0];
  sample* inputR = inputs[1];

  sample* outputL = outputs[0];
  sample* outputR = outputs[1];

  // For testing.
  double beatPosition = 0.;
  double secondsPlayed = 0.;

  // beatPosition += tempo / samplerate / 60;
  // secondsPlayed += 1. / samplerate;

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
      if (index > 0)
      {
        processShape1L = (inputL[index] >= previousLevelL);
        processShape1R = (inputR[index] >= previousLevelR);
      }

      outputL[index] = processShape1L ? shapeEditor1.forward(inputL[index], beatPosition, secondsPlayed) : shapeEditor2.forward(inputL[index], beatPosition, secondsPlayed);
      outputR[index] = processShape1R ? shapeEditor1.forward(inputR[index], beatPosition, secondsPlayed) : shapeEditor2.forward(inputR[index], beatPosition, secondsPlayed);

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
      mid = shapeEditor1.forward((inputL[index] + inputR[index]) / 2, beatPosition);
      side = shapeEditor2.forward((inputL[index] - inputR[index]) / 2, beatPosition);

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
      outputL[index] = shapeEditor1.forward(inputL[index], beatPosition);
      outputR[index] = shapeEditor2.forward(inputR[index], beatPosition);
    }
    break;
  }

  // Distortion mode +/-:
  // Use shapeEditor one on positive, shapeEditor2 on negative samples.
  case positiveNegative: {
    for (uint32_t index = 0; index < nFrames; index++)
    {
      outputL[index] = (inputL[index] > 0) ? shapeEditor1.forward(inputL[index], beatPosition) : shapeEditor2.forward(inputL[index], beatPosition);
      outputR[index] = (inputR[index] > 0) ? shapeEditor1.forward(inputR[index], beatPosition) : shapeEditor2.forward(inputR[index], beatPosition);
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

    // The index within the set of LFO parameters.
    int i = idx - LFOsStart;

    // If an LFO loopMode has been changed, update the frequency panel.
    if (i % kNumLFOParams == mode)
    {
      LFOLoopMode loopMode = static_cast<LFOLoopMode>(GetParam(idx)->Value());
      LFOs.setLoopMode(loopMode);
    }
  }
}
