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
    pGraphics->AttachControl(new DrawFrame(layout.editorFrameRect, UDS_BLACK, RELATIVE_FRAME_WIDTH_NARROW * PLUG_WIDTH, ALPHA_SHADOW));

    // Draw 3D frames around ShapeEditors and LFOController.
    LFOControlLayout LFOLayout = LFOControlLayout(layout.LFORect, b.R, b.B);
    pGraphics->AttachControl(new Frame3D(shapeEditor1.layout.fullRect, RELATIVE_FRAME_WIDTH * b.R, UDS_GREY));
    pGraphics->AttachControl(new Frame3D(shapeEditor2.layout.fullRect, RELATIVE_FRAME_WIDTH * b.R, UDS_GREY));
    pGraphics->AttachControl(new Frame3D(LFOLayout.editorFullRect, RELATIVE_FRAME_WIDTH * b.R, UDS_GREY));

    menuBar.attachUI(pGraphics);
    shapeEditor1.attachUI(pGraphics);
    shapeEditor2.attachUI(pGraphics);
    LFOs.attachUI(pGraphics);
  };
#endif
}

#if IPLUG_DSP
void UDShaper::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {}
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
