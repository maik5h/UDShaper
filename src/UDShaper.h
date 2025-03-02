#pragma once

#include "../clap/clap.h"
#include "shapeEditor.h"
#include "assets.h"

enum distortionMode {
	upDown,
	midSide,
	leftRight,
	positiveNegative
};

/*
UDShaper is the main plugin class. It holds all parameters that define the current state as well as three main
compartments contributing to the GUI: two ShapeEditors and one EnvelopeManager.
Like the ShapeEditors and EnvelopeManager, UDShaper is an InteractiveGUIElement and reacts to user inputs through the
methods processLeftClick, processMouseDrag, processMouseRelease, processDoubleClick and processRightClick. These
methods are called in gui_w32.cpp, all functions regarding windows.h are in this file only, to make integrating
support for other operating systems as easy as possible. If an action requires a menu to open, the contexMenuType is
stored in the attribute menuRequest, which is accessed through getMenuRequestType in gui_win32.cpp.

renderAudio is the main method of this class, which changes the input audio depending on the state of the ShapeEditors.
*/

class UDShaper : public InteractiveGUIElement {

    private:
    contextMenuType menuRequest = menuNone; // If any action requires a menu to be opened, the type of menu is stored here.

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

	ShapeEditor *shapeEditor1; // ShapeEditor for upwards, left, mid or positive audio.
	ShapeEditor *shapeEditor2; // ShapeEditor for downwards, right, side or negative audio.
	EnvelopeManager *envelopes; // Manages Envelopes and links between Envelopes and parameters.

	distortionMode distortionMode = upDown; // Defines the condition on which it is decided, which of the ShapeEditors to choose for each sample.

	UDShaper(uint32_t windowWidth, uint32_t windowHeight);

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas, double beatPosition = 0);

    contextMenuType getMenuRequestType();

    void renderAudio(const clap_process_t *process);
};