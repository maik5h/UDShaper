#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "src/color_palette.h"
#include "src/string_presets.h"
#include "src/UDShaperElements/TopMenuBar.h"
#include "src/UDShaperElements/ShapeEditor.h"
#include "src/UDShaperElements/LFOController.h"
#include "src/UDShaperParameters.h"
#include "src/controlMessageTags.h"
#include "src/performance/timer.h"
#include <filesystem>

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

  // Array mirroring the state of all link knob parameters (EParams::modStart).
  // This is updated on parameter changes and read only in ProcessBlock.
  // Supposed to avoid frequent GetParam calls on the audio thread.
  double modulationAmounts[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

  // Path to the csv file for performance data.
  std::string evalPath = std::filesystem::path(__FILE__).parent_path().string() + "\\src\\performance\\data\\data_point_vector.csv";
  ShapeEditorTimer editorTimer = ShapeEditorTimer(evalPath, shapeEditor1);

  // Used to skip ShapeEditorTimer::measure() calls if the method is already running.
  std::atomic_flag timerLock = ATOMIC_FLAG_INIT;

public:
  UDShaper(const InstanceInfo& info);

  void OnParamChange(int paramIdx) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;
  void OnIdle() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

#if IPLUG_DSP // http://bit.ly/2S64BDd

  // Updates the modulation amplitudes and increases the beatPosition and seconds by one sample.
  void modulationStep(double (&modulationAmplitudes)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] , double& beatPosition, double& seconds);

  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
#endif
};
