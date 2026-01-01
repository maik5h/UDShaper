#pragma once

#include <queue>
#include <deque>
#include <algorithm>
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
  // ----- normalization attributes -----
  // When normalizing audio, it must be known if
  // - the input audio is currently increasing or decreasing
  // - the input audio is currently positive or negative
  // for every sample going in and out of the buffer.
  // Samples where one of these states change are referred to as 'points of
  // interest' (POIs).

  // Stores two POI levels and normalizes samples to their interval.
  // 'points of interest' (POIs) are samples that are either minima, maxima or
  // zero points.
  struct POINormalizer
  {
    // Set the value of the next POI.
    void setNextLevel(iplug::sample newLevel)
    {
      levelPrev = levelNext;
      levelNext = newLevel;

      increasing = levelPrev < levelNext;

      range = levelNext - levelPrev;
      range = (range > 0) ? range : -range;

      iplug::sample absolutePrev = (levelPrev > 0) ? levelPrev : -levelPrev;
      iplug::sample absoluteNext = (levelNext > 0) ? levelNext : -levelNext;
      offset = (absolutePrev < absoluteNext) ? levelPrev : levelNext;
    }

    // Normalize a sample to the current POI range. The sign is preserved,
    // i.e. ouput is either in [-1, 0] or [0, 1].
    // * @param input Input value, should be within the current POI range
    // * @return The value of the input sample normalized to the POI range
    iplug::sample normalize(iplug::sample input) const
    {
      if (range == 0.)
      {
        return input;
      }
      return (input - offset) / range;
    }

    // * @param input Sample normalized to [0, 1] or [-1, 0]
    // * @return The value of a normalized sample scaled back to the POI range
    iplug::sample revertNormalize(iplug::sample input) const
    {
      if (range == 0.)
      {
        return input;
      }
      return input * range + offset;
    }

    bool isIncreasing() const
    {
      return increasing;
    }

  private:
    iplug::sample levelPrev = 0.;
    iplug::sample levelNext = 0.;
    iplug::sample range = 0.;
    iplug::sample offset = 0.;
    bool increasing = false;
  };

  // Stores information regarding the state of a single sample.
  // Contains flags to indicate if the sample is on a positive segment of
  // audio, if the audio is increasing at this sample and the level of the
  // previous sample.
  struct SampleState
  {
    // Indicates if the sample is on a positive segment. This can be true
    // even if the audio is zero, in which case it means that the previous
    // samples were all >= zero. This must be tracked to correctly detect
    // points at which the sign changes.
    bool isPositive = false;

    bool isIncreasing = false;
    iplug::sample previousLevel = 0.;
  };

  // Audio buffer for the left channel.
  // If mid/side mode is active, this represents the mid-channel.
  std::queue<iplug::sample> mBufferL;

  // Audio buffer for the right channel.
  // If mid/side mode is active, this represents the side-channel.
  std::queue<iplug::sample> mBufferR;

  // POINormalizer for the left channel.
  POINormalizer mNormL;

  // POINormalizer for the right channel.
  POINormalizer mNormR;

  // State of the sample about to be processed at the front of the buffer.
  SampleState mFrontSampleL;
  SampleState mFrontSampleR;

  // State of the sample at the back of the buffer.
  SampleState mBackSampleL;
  SampleState mBackSampleR;

  // The position of points of interest in the audio buffer
  // (left channel).
  // Each entry corresponds to one POI and gives the relative
  // offset in samples from the previous POI.
  std::deque<int> mPOIOffsetL;

  // The position of points of interest in the audio buffer
  // (right channel).
  // Each entry corresponds to one POI and gives the relative
  // offset in samples from the previous POI.
  std::deque<int> mPOIOffsetR;

  // The level of samples at POIs in the buffer (left channel).
  std::deque<iplug::sample> mPOILevelL;

  // The level of samples at POIs in the buffer (right channel).
  std::deque<iplug::sample> mPOILevelR;

  // The number of samples passed since the last POI on the left
  // channel has been found.
  int mPOIOffsetCountL = 0;

  // The number of samples passed since the last POI on the right
  // channel has been found.
  int mPOIOffsetCountR = 0;

  // Updates the modulation amplitudes and increases the beatPosition and seconds by one sample.
  void modulationStep(double (&modulationAmplitudes)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] , double& beatPosition, double& seconds);

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;

  // Clears all audio and POI buffers.
  void clearBuffer();
#endif
};
