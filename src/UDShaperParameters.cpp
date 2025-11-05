#include "UDShaperParameters.h"

int getLFOParameterIndex(int LFOIdx, LFOParams type)
{
  int categoryOffset = EParams::LFOsStart;
  int instanceOffset = LFOIdx * LFOParams::kNumLFOParams;
  int typeOffset = static_cast<int>(type);

  return categoryOffset + instanceOffset + typeOffset;
}