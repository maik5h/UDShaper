/*
UDShaper is the main plugin class. It holds all parameters that define the current state of the plugin,
as well as three main compartments contributing to the GUI: two ShapeEditors and one EnvelopeManager.

Like the ShapeEditors and EnvelopeManager, UDShaper reacts to user inputs through the methods processLeftClick,
processMouseDrag, processMouseRelease, processDoubleClick and processRightClick.
These methods are called in the window procedure in gui_w32.cpp, all functions regarding windows.h are in this
file only, to make integrating support for other operating systems as easy as possible. If an action requires a
menu to open, the contexMenuType is stored in the static attribute requesteMenu of the MenuRequest class.

renderAudio is the main method of this class, which changes the input audio depending on the state of the ShapeEditors.
*/

#pragma once

#include "../clap/clap.h"
#include "shapeEditor.h"
#include "assets.h"

long getCurrentTime();

class UDShaper {
    public:
	clap_plugin_t plugin; // The PluginClass, which interacts with the host, to create, destroy, process, etc. the plugin.
	const clap_host_t *host;

	struct GUI *gui; // Pointer to the GUI, which contains the HWND and bit array of the window.

	// Extensions have to be assigned in clap_plugin_t::init, when host is fully accessable.
	const clap_host_posix_fd_support_t *hostPOSIXFDSupport;
	const clap_host_timer_support_t *hostTimerSupport;
	const clap_host_params_t *hostParams;
	const clap_host_log_t *hostLog;

	bool mouseDragging = false; // true if a left click was performed, which has not yet been released.
	clap_id timerID;
	distortionMode currentDistortionMode = upDown; // Current distortion mode of the plugin. Can be upDown, leftRight, midSide, positiveNegative.

	TopMenuBar *topMenuBar; // TopMenuBar of the plugin, has button to select distortion mode.
	ShapeEditor *shapeEditor1; // ShapeEditor for upwards, left, mid or positive audio.
	ShapeEditor *shapeEditor2; // ShapeEditor for downwards, right, side or negative audio.
	EnvelopeManager *envelopes; // Manages Envelopes and links between Envelopes and parameters.

	HANDLE synchProcessStartTime;
	long startedPlaying = 0;
	bool hostPlaying;
	double initBeatPosition;
	double currentTempo;

	UDShaper(uint32_t windowWidth, uint32_t windowHeight);
	~UDShaper();

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas);

	void processMenuSelection(WPARAM wparam);

    void renderAudio(const clap_process_t *process);
};