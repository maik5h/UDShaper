#pragma once

#include <map>
#include "../GUILayout.h"
#include "../UDShaperParameters.h"
#include "../string_presets.h"
#include "ShapeEditor.h"
#include "IControls.h"
using namespace iplug;
using namespace igraphics;

// Modes which define the base to express the frequency of an LFO controller.
// TODO add Hz and maybe a mode where its an arbitrary integer multiple of the beats, not a power of 2.
enum LFOLoopMode
{
  // LFO frequency is a multiple of a beat.
  LFOFrequencyTempo = 0,

  // LFO frequency is set in seconds.
  LFOFrequencySeconds
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

  // Override to redraw contents after enabling.
  void SetDisabled(bool disable) override;

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

  // Override to redraw contents after enabling.
  void SetDisabled(bool disable) override;

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
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  // * @param initMode Initial loop mode
  // * @param initValue Initial loop frequency value
  FrequencyPanel(IRECT rect, float GUIWidth, float GUIHeight, LFOLoopMode initMode = LFOFrequencyTempo, double initValue = 6.);

  void attachUI(IGraphics* pGraphics);

  // Disable all attached controls.
  void setDisabled(bool disable);

  // Set which LFO corresponds to this IControl.
  //
  // A single FrequencyPanel instance may be used to control multiple LFOs.
  // Since communication with DSP works purely through IParams, the FrequencyPanel
  // must only know the parameter index to access a specific LFO.
  // * @param idx The index of the target LFO
  void setLFOIdx(int idx);

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

// A panel on the UI from which the current LFO can be selected.
class LFOSelectorControl : public IControl
{
  // IRECT that marks the highlighted area corresponding to the active LFO.
  IRECT activeRect;

public:
  LFOSelectorControl(IRECT rect);

  // Total number of LFOs displayed.
  int numberLFOs = 3;

  // Index of the active LFO.
  int activeLFOIdx = 0;

  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
};

// Class to access all LFO controls of the plugin.
//
// It holds several ShapeEditors, which are used to define a LFO curves.
// Only one editor is displayed, the panel on the left allows to switch
// the displayed editors. The LFO frequency can be set by the controls
// at the very bottom.
class LFOController
{
  // Stores the box coordinates of elements belonging to this LFOController instance.
  LFOControlLayout layout;

  // How many of the MAX_NUMBER_LFOS LFOs are active.
  int numberLFOs = 3;

  // Index of the currently displayed LFO.
  int activeLFOIdx = 0;

  // Vector of ShapeEditors representing the LFO curve editors.
  // MAX_NUMBER_LFOS spots are added.
  std::vector<ShapeEditor> editors;

  // Panel to set LFO frequency.
  FrequencyPanel frequencyPanel;

  // Panel to switch the active LFO.
  LFOSelectorControl selectorControl;

  // UI representation of the active LFO curve.
  ShapeEditorControl editorControl;

public:
  LFOController(IRECT rect, float GUIWidth, float GUIHeight);

  void attachUI(IGraphics* pGraphics);

  // Set the loop mode.
  // This must be called whenever the loop mode or the active LFO parameter changes
  // to properly update the FrequencyPanelControl.
  void setLoopMode(LFOLoopMode mode);

  // Sets the LFO at index as active.
  void setActiveLFO(int index);
};
