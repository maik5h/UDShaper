#pragma once

#include <clap/clap.h>
#include <exception>
#include "UDShaperElements.h"
#include "mutexMacros.h"
#include "assets.h"
#include "GUILayout.h"
#include "logging.h"
#include "../color_palette.h"

// Returns the current time since Unix epoch in milliseconds.
long getCurrentTime();


// UDShaper is the main plugin class. It holds all parameters that define the current state of the plugin,
// as well as the main compartments contributing to the GUI: two ShapeEditors, an EnvelopeManager and
// a TopMenuBar (see UDShaperElements.h).
//
// UDShaper reacts to user inputs through the methods processLeftClick, processMouseDrag,
// processMouseRelease, processDoubleClick and processRightClick.
//
// renderAudio is used to process the input audio depending on the state of the ShapeEditors.
class UDShaper {
    public:
	clap_plugin_t plugin;
	const clap_host_t *host;
	clap_id timerID;
	struct GUI *gui = nullptr;
	bool GUIInitialized = false;
	UDShaperLayout layout;

	// Extensions have to be assigned in clap_plugin_t::init, when host is fully accessable.
	const clap_host_timer_support_t *hostTimerSupport;
	const clap_host_params_t *hostParams;
	const clap_host_log_t *hostLog;
	const clap_host_state_t *hostState;

	bool mouseDragging = false; // true if a left click was performed which has not been released yet.
	distortionMode currentDistortionMode = upDown; // Current distortion mode of the plugin. Can be upDown, leftRight, midSide, positiveNegative.

	// The four elements that inherit from InteractiveGUIElement and make up the UDShaper GUI:
	TopMenuBar *topMenuBar;		// TopMenuBar of the plugin, has button to select distortion mode.
	ShapeEditor *shapeEditor1;	// ShapeEditor for upwards, left, mid or positive audio.
	ShapeEditor *shapeEditor2;	// ShapeEditor for downwards, right, side or negative audio.
	EnvelopeManager *envelopes;	// Manages Envelopes and links between Envelopes and parameters.

	Mutex synchProcessStartTime;
	long startedPlaying = 0;		// The time the host started playback.
	double initBeatPosition;		// The song position at which the host started playing in beats.
	bool hostPlaying;
	double currentTempo;
	double samplerate;

	// Initializes the InteractiveGUIElement members at positions and with sizes according to the initial window size.
	UDShaper();

	// Frees the memory allocated by the InteractiveGUIElements.
	~UDShaper();

	// The following methods forward user inputs to all members inheriting that that are supposed to react to it.
    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas);
	void rescaleGUI(uint32_t width, uint32_t height);

	// After being called, will inform subelements that they have to rerender their GUI. Must be called when showing
	// the plugin window for the first time or after being hidden.
	void setupForRerender();

	// Takes the input audio from the given process, applies distortion and writes
	// the output audio to process->audio_outputs.
	//
	// The output is mainly dependent on two things: The state of both ShapeEditors
	// (which depend on the host playback position if linked to an Envellope) and the
	// distortion mode soterd in TopMenuBar:
	//  - upDown:			shapeEditor1 is used on all samples that are higher than the previous sample,
	//						shapeEditor 2 on samples that are lower than the previous one.
	//  - leftRight:		shapeEditor1 is used on the left channel, shapeEditor2 on the right channel.
	//  - midSide:			shapeEditor1 is used on the mid channel, shapeEditor2 on the side channel.
	//  - positiveNegative:	shapeEditor1 is used on samples > 0, shapeEditor2 on samples < 0. samples = 0 stay 0.
    void renderAudio(const clap_process_t *process);

	// Saves the UDShaper state by consecutively calling the saveState methods of the
	// InteractiveGUIElement members.
	bool saveState(const clap_ostream_t *stream);

	// Loads the UDShaper state. All InteractiveGUIElements have to be initialized by the time
	// this function is called.
	bool loadState(const clap_istream_t *stream);
};