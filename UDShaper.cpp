#include "UDShaper.h"
#include "IPlug_include_in_plug_src.h"
#include "IControls.h"

#include "src/GUILayout.h"
#include "src/assets.h"

UDShaper::UDShaper(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(distMode)->InitEnum("Distortion mode", 0, 4);
  GetParam(distMode)->SetDisplayText(0, "Up/Down");
  GetParam(distMode)->SetDisplayText(1, "Left/Right");
  GetParam(distMode)->SetDisplayText(2, "Mid/Side");
  GetParam(distMode)->SetDisplayText(3, "+/-");

#if IPLUG_EDITOR // http://bit.ly/2S64BDd
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };
  
  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(UDS_GREY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    IRECT b = pGraphics->GetBounds();

    UDShaperLayout layout = UDShaperLayout();

    TopMenuBarLayout tmLayout = TopMenuBarLayout();
    ShapeEditorLayout se1Layout = ShapeEditorLayout();
    ShapeEditorLayout se2Layout = ShapeEditorLayout();
    layout.setCoordinates(b.R, b.B);
    tmLayout.setCoordinates(layout.topMenuRect, b.R, b.B);
    se1Layout.setCoordinates(layout.editor1Rect, b.R, b.B);
    se2Layout.setCoordinates(layout.editor2Rect, b.R, b.B);

    // Draw a frame around the two ShapeEditors.
    pGraphics->AttachControl(new DrawFrame(layout.editorFrameRect, UDS_BLACK, RELATIVE_FRAME_WIDTH_NARROW * PLUG_WIDTH, ALPHA_SHADOW));

    // This should be 3D frame.
    float margin = RELATIVE_FRAME_WIDTH * PLUG_WIDTH;
    // TODO I do not know why this factor of two is necessary. No idea.
    pGraphics->AttachControl(new DrawFrame(se2Layout.fullRect, UDS_BLACK, 2 * RELATIVE_FRAME_WIDTH * b.R));

    menuBar.setCoordinates(layout.topMenuRect, b.R, b.B);
    menuBar.attachUI(pGraphics);

    shapeEditor1.setCoordinates(layout.editor1Rect, b.R, b.B);
    shapeEditor1.attachUI(pGraphics);

    shapeEditor2.setCoordinates(layout.editor2Rect, b.R, b.B);
    shapeEditor2.attachUI(pGraphics);
  };
#endif
}

#if IPLUG_DSP
void UDShaper::ProcessBlock(sample** inputs, sample** outputs, int nFrames) {}
#endif
