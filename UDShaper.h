#pragma once

#include <deque>
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

  // The distortion mode of the plugin.
  distortionMode mMode = distortionMode::upDown;

  // Apply piecewise normalization before audio processing.
  bool mNormalize = true;

  // The amplitude of the last sample in the previous block (left channel).
  iplug::sample mPreviousBlockLevelL = 0.f;

  // The amplitude of the last sample in the previous block (right channel).
  iplug::sample mPreviousBlockLevelR = 0.f;

  // Audio buffers for two channels.
  // Deques because traversal must be possible.
  std::deque<iplug::sample> mBufferL = {};
  std::deque<iplug::sample> mBufferR = {};

  // Flag to safely clear buffers after normalization has been turned off.
  // Will clear buffers at the start of ProcessBlock on the audio thread.
  bool mClearBuffer = false;

  // Array holding the amplitudes of all LFO modulation links. See src/LFOController.h.
  // Is updated on the UI thread and provides modulation amplitudes for IControls.
  double modulationAmplitudesUI[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

  // Array mirroring the state of all link knob parameters (EParams::modStart).
  // This is updated on parameter changes and read only in ProcessBlock.
  // Supposed to avoid frequent GetParam calls on the audio thread.
  double modulationAmounts[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

public:
  UDShaper(const InstanceInfo& info);

  void OnParamChange(int paramIdx) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;
  void OnIdle() override;
  void OnReset() override;
  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;

#if IPLUG_DSP
  // Stores amplitudes and positions of extrema needed to normalize audio.
  struct NormalizeInfo
  {
    // Amplitude of the previous extremum or zero point.
    iplug::sample ampPrev = 0.f;

    // Amplitude of the next extremum or zero point.
    iplug::sample ampNext = 1.f;

    // Index of the next extremum or zero point in the array.
    int idx = 0;

    // * @return The lower of the two amplitudes ampPrev and ampNext
    iplug::sample getLower() const { return (ampPrev > ampNext) ? ampNext : ampPrev; }

    // * @return The higher of the two amplitudes ampPrev and ampNext
    iplug::sample getUpper() const { return (ampPrev < ampNext) ? ampNext : ampPrev; }
  };

  // The amplitude and position of the last extremum/ zero point
  // of the last processed block (left channel).
  NormalizeInfo mNormPrevL = {};

  // The amplitude and position of the last extremum/ zero point
  // of the last processed block (right channel).
  NormalizeInfo mNormPrevR = {};

  // Updates the modulation amplitudes and increases the beatPosition and seconds by one sample.
  void modulationStep(double (&modulationAmplitudes)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] , double& beatPosition, double& seconds);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;

  // Find the next "point of interest" in the given block of audio.
  //
  // POIs are local minima, local maxima and zero points. The next POI
  // after info.idx is determined and written to the same NormalizeInfo
  // object.
  // * @param audio Audio buffer
  // * @param info NormalizeInfo object to which the position and amplitude of
  // the next POI will be written
  static void findPOI(std::deque<iplug::sample> audio, NormalizeInfo& info);

  // Normalize a sample to the surrounding extrema/ zero points as given in
  // info.
  // * @param sample The input sample amplitude
  // * @param info NormalizeInfo object storing information about the
  // surrounding minima/ maxima/ zero points
  // * @return The input normalized to the surrounding minima/ maxima/ zero
  // points
  static float normalizeSample(iplug::sample sample, NormalizeInfo info);

  // Reverts the normalization of normalizeSample.
  // * @param sample Value of a sample normalized to info
  // * @param info NormalizeInfo object holding the normalization bounds
  // * @return The input sample reverted to the original scale
  static float revertNormalizeSample(iplug::sample sample, NormalizeInfo info);
#endif
};
