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
  LFOKnobStart,
  kNumControls = LFOKnobStart + MAX_MODULATION_LINKS
};