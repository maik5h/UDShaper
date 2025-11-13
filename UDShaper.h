#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "src/color_palette.h"
#include "src/string_presets.h"
#include "src/UDShaperElements/TopMenuBar.h"
#include "src/UDShaperElements/ShapeEditor.h"
#include "src/UDShaperElements/LFOController.h"
#include "src/UDShaperParameters.h"
#include "src/controlMessageTags.h"

const int kNumPresets = 1;

using namespace iplug;
using namespace igraphics;

class UDShaper final : public Plugin
{
  UDShaperLayout layout = UDShaperLayout(PLUG_WIDTH, PLUG_HEIGHT);

  TopMenuBar menuBar = TopMenuBar(layout.topMenuRect, PLUG_WIDTH, PLUG_HEIGHT);
  ShapeEditor shapeEditor1 = ShapeEditor(layout.editor1Rect, PLUG_WIDTH, PLUG_HEIGHT, 0);
  ShapeEditor shapeEditor2 = ShapeEditor(layout.editor2Rect, PLUG_WIDTH, PLUG_HEIGHT, 1);
  LFOController LFOs = LFOController(layout.LFORect, PLUG_WIDTH, PLUG_HEIGHT, this);

  // Array holding the amplitudes of all LFO modulation links. See src/LFOController.h.
  // Is updated on the UI thread and provides modulation amplitudes for IControls.
  double modulationAmplitudesUI[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

public:
  UDShaper(const InstanceInfo& info);

  void OnParamChange(int paramIdx) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;
  void OnIdle() override;

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
