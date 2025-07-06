// Functions to manage GUI, definitions are platform dependend.
// So far, only windows is implemented.

#pragma once

#include <stdexcept>
#include "UDShaper.h"
#include "assets.h"
#include <clap/clap.h>
#include "UDShaperElements.h"

void GUIPaint(UDShaper *plugin, bool internal);
void GUICreate(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);
void GUIDestroy(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);
void GUIOnPOSIXFD(UDShaper *plugin);

#ifdef _WIN32

#include <windowsx.h>

// Struct containing a window, the window bitmap and the current GUI width and height values.
// This implementation uses windowsx.h.
struct GUI {
	HWND window;		// GUI window.
	uint32_t *bits;		// GUI bitmap buffer. Has size GUI_WIDTH * GUI_HEIGHT with eight bit for each color channel and alpha channel.
	uint32_t width;		// Width of the GUI.
	uint32_t height;	// Height of the GUI.

	void resize(uint32_t newWidth, uint32_t newHeight);
	void setParentWindow(const clap_window_t *parentWindow);
	void showWindow(bool visible);
};

#define GUI_API CLAP_WINDOW_API_WIN32

#else

static_assert(false, "GUI is not implemented for any operating system besides Windows.");

#endif
