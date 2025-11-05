#pragma once

#include "config.h"

// The parameters concerning each LFO.
enum LFOParams
{
  mode = 0,
  freqTempo,
  freqSeconds,
  kNumLFOParams
};

// All parameters of the UDShaper plugin.
enum EParams
{
  // Plugin distortion mode, switches between Up/Down, Left/Right, Mid/Side and +/-.
  distMode = 0,

  // LFO parameters start here.
  LFOsStart = 1,

  // Total number of parameters.
  kNumParams = MAX_NUMBER_LFOS * kNumLFOParams + LFOsStart
};

// Returns the global parameter index of LFO parameters.
// * @param LFOIdx The index of the LFO the parameter belongs to
// * @param type The type of parameter
int getLFOParameterIndex(int LFOIdx, LFOParams type);
