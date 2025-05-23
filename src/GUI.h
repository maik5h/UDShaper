// Functions to manage GUI, definitions are platform dependend.
// So far, only windows is implemented.

#pragma once

#include "UDShaper.h"
#include <windowsx.h>
#include "assets.h"
#include <clap/clap.h>
#include "UDShaperElements.h"

struct GUI {
	HWND window; // GUI window.
	uint32_t *bits; // GUI bitmap buffer. Has size GUI_WIDTH * GUI_HEIGHT with eight bit for each color channel and alpha channel.
	uint32_t width;		// Width of the GUI.
	uint32_t height;	// Height of the GUI.
};

void GUIPaint(UDShaper *plugin, bool internal);
void GUICreate(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);
void GUIDestroy(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor);
void GUIOnPOSIXFD(UDShaper *plugin);

#ifdef _WIN32
#define GUISetParent(plugin, parent) SetParent((plugin)->gui->window, (HWND) (parent)->win32)
#define GUISetVisible(plugin, visible) ShowWindow((plugin)->gui->window, (visible) ? SW_SHOW : SW_HIDE)
#define GUI_API CLAP_WINDOW_API_WIN32
#endif
