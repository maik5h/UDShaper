#include "UDShaper.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include "src/GUILayout.h"
#include "src/assets.h"

UDShaper::UDShaper(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
  , menuBar()
  , shapeEditor1(0)
  , shapeEditor2(1)
  , testFP()
{
  layout.setCoordinates(PLUG_WIDTH, PLUG_HEIGHT);
  
  menuBar.setCoordinates(layout.topMenuRect, PLUG_WIDTH, PLUG_HEIGHT);
  shapeEditor1.setCoordinates(layout.editor1Rect, PLUG_WIDTH, PLUG_HEIGHT);
  shapeEditor2.setCoordinates(layout.editor2Rect, PLUG_WIDTH, PLUG_HEIGHT);
  testFP.setCoordinates(layout.envelopeRect, PLUG_WIDTH, PLUG_HEIGHT);

  IParam* param = GetParam(distMode);
  param->InitEnum("Distortion mode", 0, 4);
  param->SetDisplayText(0, "Up/Down");
  param->SetDisplayText(1, "Left/Right");
  param->SetDisplayText(2, "Mid/Side");
  param->SetDisplayText(3, "+/-");

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

    // This should be 3D frame.
    //float margin = RELATIVE_FRAME_WIDTH * PLUG_WIDTH;
    // TODO I do not know why this factor of two is necessary. No idea.
    //pGraphics->AttachControl(new DrawFrame(se2Layout.fullRect, UDS_BLACK, 2 * RELATIVE_FRAME_WIDTH * b.R));

    menuBar.attachUI(pGraphics);
    shapeEditor1.attachUI(pGraphics);
    shapeEditor2.attachUI(pGraphics);
    testFP.attachUI(pGraphics);
  };
#endif
}

#if IPLUG_DSP
void UDShaper::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {}
#endif

void UDShaper::OnParamChange(int idx)
{
  // If an LFO loopMode has been changed, update the frequency panel.
  if (idx == getLFOParameterIndex(0, mode))
  {
    LFOLoopMode loopMode = static_cast<LFOLoopMode>(GetParam(idx)->Value());
    testFP.setLoopMode(loopMode);
  }

  // If an LFO frequency value has changed, update the internal value.
  if (idx == getLFOParameterIndex(0, freqSeconds))
  {
    testFP.setValue(LFOFrequencySeconds, GetParam(idx)->Value());
  }
  if (idx == getLFOParameterIndex(0, freqTempo))
  {
    testFP.setValue(LFOFrequencyTempo, GetParam(idx)->Value());
  }
}
