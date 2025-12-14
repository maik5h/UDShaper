#pragma once

/**
 * @file LFOController.h
 * @brief This file contains the LFOController implementation, along with some IControls belonging to this class.
 *
 * The LFO controller provides an UI to edit the shape and frequency of different LFOs and link them to ModulatedParameters.
 * Each LFO has a fixed number of available links (MAX_MODULATION_LINKS). The modulation state at a specific host beat
 * position or time can be requested via LFOController::getModulationAmounts. This method writes the modulation amounts of all
 * possible links into an array, where entries of links that are not connected are set to 0. This array can be forwarded to
 * ModulatedParameters which choose the entries they are connected with to determine their current modulated value.
 */

#include <map>
#include "../GUILayout.h"
#include "../UDShaperParameters.h"
#include "../controlTags.h"
#include "../controlMessageTags.h"
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
  WDL_String fmtSeconds = WDL_String("%.2f s");
  WDL_String fmtMilliseconds = WDL_String("%2.f ms");

  // Modified such that the format changes depending on the value.
  void OnValueChanged(bool preventAction = false);

public:
  SecondsBoxControl(IRECT rect, int parameterIdx);

  // Override to redraw contents after enabling.
  void SetDisabled(bool disable) override;

  // Override to add opaque background.
  void Draw(IGraphics& g) override;

  // Override to disable text entry.
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override;

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
  // Construct a BeatsBoxControl object.
  // * @param rect The IRECT of this IControl
  // * @param parameterIdx Initial index of the parameter corresponding to this instance.
  // * @param initValue The initial parameter value, corresponds to an index for FREQUENCY_COUNTER_STRINGS and must be in [0, 13].
  BeatsBoxControl(IRECT rect, int parameterIdx, int initValue);

  // Override to redraw contents after enabling.
  void SetDisabled(bool disable) override;

  // Override to add opaque background.
  void Draw(IGraphics& g) override;

  // Override to disable text entry.
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override;

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

  // Pointer to the parent plugin.
  IPluginBase* mPlugin;

  // Arrays that mirror the parameter state, used to prevent frequent GetParam calls on the audio threads.
  // These are set on parameter changes and read only while processing audio.

  // The current loop mode for each LFO.
  LFOLoopMode currentMode[MAX_NUMBER_LFOS] = {};

  // The current frequency value for the loop mode LFOFrequencyTempo for each LFO.
  double freqTempo[MAX_NUMBER_LFOS] = {};

  // The current frequency value for the loop mode LFOFrequencySeconds for each LFO.
  double freqSeconds[MAX_NUMBER_LFOS] = {};


public:
  // Index of the LFO displayed on the UI.
  int activeLFOIdx = 0;

  // Constructs a FrequencyPanel
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  // * @param plugin Pointer to the parent IPlugBase object.
  FrequencyPanel(IRECT rect, float GUIWidth, float GUIHeight, IPluginBase* plugin);

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

  // Set the frequency value belonging to the given mode on the active LFO.
  void setFrequencyValue(LFOLoopMode mode, double value);

  // Returns the phase value at beatPosition/ secondsPlayed normalized to [0, 1]
  //
  // * @param LFOIdx Index of the LFO of which the phase is requested
  // * @param beatPosition The host playback position in beats
  // * @param secondsPlayed The host playback position in seconds
  // * @returns The phase of the LFO at LFOIdx
  double getLFOPhase(int LFOIdx, double beatPosition, double secondsPlayed) const;

  // Refresh the state of the internally stored parameters.
  //
  // This is meant to be called after the plugin has initialized the parameters
  // to sync the state. Checks all LFO parameters so it should only be called
  // if necessary. Parameter changes are directly reported to the internal arrays
  // in UDShaper::OnParamChange.
  void refreshInternalState();

  // bool saveState(const clap_ostream_t *stream);
  // bool loadState(const clap_istream_t *stream, int version[3]);
};

// A panel on the UI from which the current LFO can be selected.
class LFOSelectorControl : public IControl
{
  // IRECT that marks the highlighted area corresponding to the active LFO.
  IRECT activeRect;

  // Keeps track if the user is currently dragging.
  bool isDragging = false;

  // Send this struct when attempting to connect an LFO link.
  LFOConnectInfo connectInfo = {};

  // Keeps track if the available modulation links are connected to a parameter.
  bool (&linkActive)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS];

  // Keep track if the mouse is over the rect of this control while dragging.
  bool mouseOver = false;

public:
  // Total number of LFOs displayed.
  int numberLFOs = 3;

  // Index of the active LFO.
  int activeLFOIdx = 0;

  // Create an LFOSelectorControl
  //
  // * @param rect IRECT of this control
  // * @param linkActive Reference to an array stored in LFOController.
  // Needed to keep the state outside of IControls, in case they get deleted.
  LFOSelectorControl(IRECT rect, bool (&linkActive)[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS]);

  // Copies the state of the modulation links associated with the active LFO to the input array.
  const void getActiveLinks(bool (&isActive)[MAX_MODULATION_LINKS]);

  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;
};

// Class to draw a knob.
//
// Currently this is identical to IVKnobControl, it is a placeholder for later when
// custom graphics are added.
class LinkKnobVisualLayer : public IVKnobControl
{
public:
  LinkKnobVisualLayer(IRECT rect, int paramIdx);

  void Draw(IGraphics& g) override;
};

// Class to put on top of a LinkKnobVisualLayer to manage user inputs.
//
// For some reason it is not clear at all how IVKnobControls handle popup menus and
// they do also not accept right clicks. My solution is to have this class on top
// of a knob, that eats all inputs and forwards them EXCEPT the rightclicks, on
// which it opens the menu.
class LinkKnobInputLayer : public IControl
{
public:
  LinkKnobInputLayer(IRECT rect, int visualLayerTag, int knobIdx);

  void Draw(IGraphics& g) override;

  // Forward the inputs to the control at visTag.
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;
  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override;
  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
  void OnPopupMenuSelection(IPopupMenu* pSelectedMenu, int valIdx) override;

  // Send a message to ShapeEditors to highlight the point connected to this knob.
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;

  // Send a message to ShapeEditors to stop highlighting.
  void OnMouseOut() override;

private:
  // Tag of the LinkKnobVisualLayer associated with this InputLayer.
  int visTag;

  // Index of this knob.
  int kIdx;

  // Menu to display when rightclicking a link knob.
  IPopupMenu menu = IPopupMenu();
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

  // Index of the currently displayed LFO.
  int activeLFOIdx = 0;

  // Vector of ShapeEditors representing the LFO curve editors.
  // MAX_NUMBER_LFOS spots are added.
  std::vector<ShapeEditor> editors;

  // Panel to set LFO frequency.
  FrequencyPanel frequencyPanel;

  // Pointer to parent plugin.
  IPluginBase* mPlugin;

  // Keeps track if the available modulation links are connected to a parameter.
  // There are MAX_NUMBER_LFOS different LFOs and MAX_MODULATION_LINKS links per
  // LFO. All these parameters are appended in this array.
  bool linkActive[MAX_NUMBER_LFOS * MAX_MODULATION_LINKS] = {};

public:
  LFOController(IRECT rect, float GUIWidth, float GUIHeight, IPluginBase* plugin);

  void attachUI(IGraphics* pGraphics);

  // Set the loop mode.
  // This must be called whenever the loop mode or the active LFO parameter changes
  // to properly update the FrequencyPanelControl.
  void setLoopMode(LFOLoopMode mode);

  // Set the frequency value belonging to the given mode on the active LFO.
  void setFrequencyValue(LFOLoopMode mode, double value);

  // Sets the LFO at index as active.
  void setActiveLFO(int index);

  // Synchronizes the internal FrequencyPanel parameters with the iPlug2 parameters.
  //
  // This is meant to be called after the plugin has initialized the parameters
  // to sync the state. Checks all LFO parameters so it should only be called
  // if necessary. Parameter changes are directly reported to the internal arrays
  // in UDShaper::OnParamChange.
  void refreshInternalState();

  // Get the amplitudes of all available modulation links.
  //
  // Each of the MAX_NUMBER_LFOS LFOs can modulate MAX_MODULATION_LINKS different parameters.
  // The input array 'amplitudes' provides storage for all possible modulation slots, regardless if
  // they have been linked to a parameter or not.
  // If an LFO has not all available links active, the inactive links will still be reported
  // with an amplitude of 0.
  //
  // * @param beatPosition The song position in beats at which the amplitudes are calculated
  // * @para, secondsPlayed The song position in seconds at which the amplitudes are calculated
  // * @param amplitudes Array in which the amplitudes will be copied. Must have size MAX_NUMBER_LFOS * MAX_MODULATION_LINKS
  // * @param factors Array that contains the modulation amounts corresponding to each LFO link. Must have the same size as amplitudes.
  const void getModulationAmplitudes(double beatPosition, double secondsPlayed, double* amplitudes, double* factors);

  // Enable the modulation link at idx.
  void setLinkActive(int idx, bool active = true);

  bool serializeState(IByteChunk& chunk) const;
  int unserializeState(const IByteChunk& chunk, int startPos, int version);
};
