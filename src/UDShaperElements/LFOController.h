#pragma once

#include <map>
#include "../GUILayout.h"
#include "../UDShaperParameters.h"
#include "../string_presets.h"
#include "IControls.h"
using namespace iplug;
using namespace igraphics;

// Modes which define the base to express the frequency of an LFO controller.
// TODO add Hz and maybe a mode where its an arbitrary integer multiple of the beats, not a power of 2.
enum LFOLoopMode
{
  LFOFrequencyTempo = 0, // Envelope frequency is a multiple of a beat.
  LFOFrequencySeconds    // Envelope frequency is set in seconds.
};

// An IVNumberBoxControl tailored to display time.
class SecondsBoxControl : public IVNumberBoxControl
{
protected:
  WDL_String fmtSeconds = WDL_String ("%.2f s");
  WDL_String fmtMilliseconds = WDL_String("%2.f ms");

  // Modified such that the format changes depending on the value.
  void OnValueChanged(bool preventAction = false);

public:
  SecondsBoxControl(IRECT rect, int parameterIdx);

  // Override these so that they call the modified OnValueChanged method instead of the
  // IVNumberBoxControl one. Implementation is very similar to IVNumberBoxControl.
  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
};

// An IVNumberBoxControl tailored to display beat factors.
//
// Possible values are natural mutliples or fractions of beats e.g.
// ..., 4/1, 2/1, 1/1, 1/2, 1/4, ...
//
// The internal Value of this class is in [0, 13] and describes the index
// in a list of output values.
class BeatsBoxControl : public IVNumberBoxControl
{
protected:
  void OnValueChanged(bool preventAction = false);

public:
  BeatsBoxControl(IRECT rect, int parameterIdx);

  // Override these so that they call the modified OnValueChanged method instead of the
  // IVNumberBoxControl one. Implementation is very similar to IVNumberBoxControl.
  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
};

// Control panel to select a frequency for an LFO.
//
// Consists of two elements that are drawn to the GUI:
//  - A dropdown menu that lets the user select the loop mode.
//  - A "counter" which displays a number that translates to a frequency based on the loop mode
//    (for example "4/1" = four times per beat or "3s" = every three seconds).
class FrequencyPanel
{
private:
  // Stores the box coordinates of elements belonging to this FrequencyPanel instance.
  FrequencyPanelLayout layout;

  // Mapping between all loop modes and their current frequency values.
  std::map<LFOLoopMode, double> counterValue = {{LFOFrequencyTempo, 6.}, {LFOFrequencySeconds, 1.}};

  // Control to select the frequency mode.
  ICaptionControl* modeControl;

  // Control to select the loop frequency in beats.
  BeatsBoxControl* freqBeatsControl;

  // Control to select the loop frequency in seconds.
  SecondsBoxControl* freqSecondsControl;

  // The current loop mode, can be based on tempo or seconds.
  LFOLoopMode currentLoopMode = LFOFrequencyTempo;

public:
  // Constructs a FrequencyPanel
  // * @param initMode Initial loop mode
  // * @param initValue Initial loop frequency value
  FrequencyPanel(LFOLoopMode initMode = LFOFrequencyTempo, double initValue = 6.);

  // Assign an IRECT to this instance.
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);

  void attachUI(IGraphics* pGraphics);

  // Disable all attached controls.
  void setDisabled(bool disable);

  // Change the current loop mode of this FrequencyPanel.
  //
  // Enables the controls needed to set the loop frequency for this mode.
  void setLoopMode(LFOLoopMode mode);

  // Set the frequency of mode to value.
  void setValue(LFOLoopMode mode, double value);

  // Returns the phase value at beatPosition/ secondsPlayed normalized to [0, 1]
  double getLFOPhase(double beatPosition, double secondsPlayed);

  // bool saveState(const clap_ostream_t *stream);
  // bool loadState(const clap_istream_t *stream, int version[3]);
};
