// Functions to manage GUI, definitions are platform dependend.
// So far, only windows is implemented.

#pragma once

#include <stdexcept>
#include <windowsx.h>
#include "UDShaperCore.h"
#include "assets.h"
#include <clap/clap.h>
#include "UDShaperElements.h"
#include "contextMenus.h"

void GUIPaint(UDShaper *plugin, bool internal);
void GUICreate(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);
void GUIDestroy(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);

// Struct containing a window, the window bitmap and the current GUI width and height values.
// This implementation uses windowsx.h.
struct GUI {
	HWND window;		// GUI window.
	uint32_t *bits;		// GUI bitmap buffer. Has size GUI_WIDTH * GUI_HEIGHT with eight bit for each color and alpha channel.
	uint32_t width;		// Width of the GUI.
	uint32_t height;	// Height of the GUI.

	// Resizes the plugin window.
	void resize(uint32_t newWidth, uint32_t newHeight);

	// Register parent window to embed plugin GUI in.
	void setParentWindow(const clap_window_t *parentWindow);

	// Toggles the visibility of the plugin window.
	void showWindow(bool visible);
};

#define GUI_API CLAP_WINDOW_API_WIN32
