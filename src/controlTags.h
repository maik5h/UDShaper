#pragma once

// Tags to access IControls.
enum EControlTags
{
  ShapeEditorControl1 = 0,
  ShapeEditorControl2,
  LFOSelectorControlTag,
  LFOEditorControlTag,
  FPModeControlTag,
  FPBeatsControlTag,
  FPSecondsControlTag,

  // Start of controls corresponding to LFO link knobs.
  // There are MAX_MODULATION_LINKS knobs and two controls per knob.
  LFOKnobStart,
  kNumControls = LFOKnobStart + 2 * MAX_MODULATION_LINKS
};