#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "src/color_palette.h"
#include "src/UDShaperElements/TopMenuBar.h"
#include "src/UDShaperParameters.h"

const int kNumPresets = 1;

using namespace iplug;
using namespace igraphics;

class UDShaper final : public Plugin
{
  TopMenuBar menuBar = TopMenuBar();

public:
  UDShaper(const InstanceInfo& info);

#if IPLUG_DSP // http://bit.ly/2S64BDd
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
