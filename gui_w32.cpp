#define GUI_API CLAP_WINDOW_API_WIN32

#include <windowsx.h>
#include "assets.h"

struct GUI {
	HWND window;
	uint32_t *bits;
};

static int globalOpenGUICount = 0;

static void GUIPaint(MyPlugin *plugin, bool internal) {
	if (internal) PluginPaint(plugin, plugin->gui->bits);
	RedrawWindow(plugin->gui->window, 0, 0, RDW_INVALIDATE);
}

void showShapeMenu(HWND hwnd, int xPos, int yPos) {
    HMENU hMenu = CreatePopupMenu();

    // title and separator
    AppendMenu(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, "Function:");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);

    AppendMenu(hMenu, MF_STRING, shapePower, "Power");
    AppendMenu(hMenu, MF_STRING, shapeSine, "Sine");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    DestroyMenu(hMenu);
}

void showLinkKnobMenu(HWND hwnd, int xPos, int yPos) {
    HMENU hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING, removeLink, "Remove Link");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    DestroyMenu(hMenu);
}

void showPointPositionModMenu(HWND hwnd, int xPos, int yPos, bool hideX = false) {
	HMENU hMenu = CreatePopupMenu();

	if (!hideX) {
		AppendMenu(hMenu, MF_STRING, modPosX, "X");
	}
	AppendMenu(hMenu, MF_STRING, modPosY, "Y");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    DestroyMenu(hMenu);
}

LRESULT CALLBACK GUIWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	MyPlugin *plugin = (MyPlugin *) GetWindowLongPtr(window, 0);

	if (!plugin) {
		return DefWindowProc(window, message, wParam, lParam);
	}

	if (message == WM_PAINT) {
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(window, &paint);
		BITMAPINFO info = { { sizeof(BITMAPINFOHEADER), GUI_WIDTH, -GUI_HEIGHT, 1, 32, BI_RGB } };
		StretchDIBits(dc, 0, 0, GUI_WIDTH, GUI_HEIGHT, 0, 0, GUI_WIDTH, GUI_HEIGHT, plugin->gui->bits, &info, DIB_RGB_COLORS, SRCCOPY);
		EndPaint(window, &paint);
	} else if (message == WM_MOUSEMOVE) {
		PluginProcessMouseDrag(plugin, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		GUIPaint(plugin, true);
	} else if (message == WM_LBUTTONDOWN) {
		SetCapture(window); 
		PluginProcessMousePress(plugin, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		GUIPaint(plugin, true);
	} else if (message == WM_LBUTTONDBLCLK){
		SetCapture(window); 
		PluginProcessDoubleClick(plugin, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		GUIPaint(plugin, true);
	} else if (message == WM_RBUTTONUP){
		SetCapture(window);

		uint32_t x = GET_X_LPARAM(lParam);
		uint32_t y = GET_Y_LPARAM(lParam);

		// check all ShapeEditors and Envelopes for rightclicked points
		contextMenuType menuType = plugin->shapeEditor1.processRightClick(x, y);
		menuType = menuType == menuNone ? plugin->shapeEditor2.processRightClick(x, y) : menuType;

		menuType = menuType == menuNone ? plugin->envelopes.processRightClick(x, y) : menuType;
		
		// If any of the processRightClick functions returned true, open the context menu for points. Information about which point was rightclicked is handled inside the ShapeEditor and Envelope instances.
		if (menuType != menuNone){
			RECT *rect = new RECT(); // rect to store window coordinates
  			GetWindowRect(window, rect);
			// Mouse coordinates are relative to window, so add window coordinates stored in rect
			if (menuType == menuShapePoint){
				showShapeMenu(window, rect->left + GET_X_LPARAM(lParam), rect->top + GET_Y_LPARAM(lParam));
			}
			else if (menuType == menuLinkKnob){
				showLinkKnobMenu(window, rect->left + GET_X_LPARAM(lParam), rect->top + GET_Y_LPARAM(lParam));
			}
			delete rect;
			rect = nullptr;
		}
		GUIPaint(plugin, true);
	} else if (message == WM_COMMAND){
		// Process the menu selection for all ShapeEditor and Envelope instances, does only affect the point which has been rightclicked.
		plugin->shapeEditor1.processMenuSelection(wParam);
		plugin->shapeEditor2.processMenuSelection(wParam);
		plugin->envelopes.processMenuSelection(wParam);

		GUIPaint(plugin, true);
	}
	else if (message == WM_LBUTTONUP) {
		ReleaseCapture(); 
		if (PluginProcessMouseRelease(plugin) == menuPointPosMod){
			RECT *rect = new RECT(); // rect to store window coordinates
			GetWindowRect(window, rect);
			showPointPositionModMenu(window, rect->left + GET_X_LPARAM(lParam), rect->top + GET_Y_LPARAM(lParam));
		}
		GUIPaint(plugin, true);
	} else {
		return DefWindowProc(window, message, wParam, lParam);
	}

	return 0;
}

static void GUICreate(MyPlugin *plugin) {
	assert(!plugin->gui);
	plugin->gui = (GUI *) calloc(1, sizeof(GUI));

	if (globalOpenGUICount++ == 0) {
		WNDCLASS windowClass = {};
		windowClass.lpfnWndProc = GUIWindowProcedure;
		windowClass.cbWndExtra = sizeof(MyPlugin *);
		windowClass.lpszClassName = pluginDescriptor.id;
		windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
		windowClass.style = CS_DBLCLKS;
		RegisterClass(&windowClass);
	}

	plugin->gui->window = CreateWindow(pluginDescriptor.id, pluginDescriptor.name, WS_CHILDWINDOW | WS_CLIPSIBLINGS, 
			CW_USEDEFAULT, 0, GUI_WIDTH, GUI_HEIGHT, GetDesktopWindow(), NULL, NULL, NULL);
	plugin->gui->bits = (uint32_t *) calloc(1, GUI_WIDTH * GUI_HEIGHT * 4);
	SetWindowLongPtr(plugin->gui->window, 0, (LONG_PTR) plugin);

	// Set up elements that do not change over time. They will not be rerendered every frame.
	uint32_t pluginSize[4] = {0, 0, GUI_WIDTH, GUI_HEIGHT};
	fillRectangle(plugin->gui->bits, pluginSize, colorBackground);

	draw3DFrame(plugin->gui->bits, plugin->shapeEditor1.XYXYFull, colorEditorBackground);
	draw3DFrame(plugin->gui->bits, plugin->shapeEditor2.XYXYFull, colorEditorBackground);
	plugin->envelopes.setupFrames(plugin->gui->bits);

	uint32_t outerFrameOffset = 28;
	uint32_t outerFrame[4] = {plugin->shapeEditor1.XYXYFull[0] - outerFrameOffset, plugin->shapeEditor1.XYXYFull[1] - outerFrameOffset, plugin->shapeEditor2.XYXYFull[2] + outerFrameOffset, plugin->shapeEditor2.XYXYFull[3] + outerFrameOffset};
	drawFrame(plugin->gui->bits, outerFrame, 5, 0x000000, 0.45);

	PluginPaint(plugin, plugin->gui->bits);
}

static void GUIDestroy(MyPlugin *plugin) {
	assert(plugin->gui);
	DestroyWindow(plugin->gui->window);
	free(plugin->gui->bits);
	free(plugin->gui);
	plugin->gui = nullptr;

	if (--globalOpenGUICount == 0) {
		UnregisterClass(pluginDescriptor.id, NULL);
	}
}

#define GUISetParent(plugin, parent) SetParent((plugin)->gui->window, (HWND) (parent)->win32)
#define GUISetVisible(plugin, visible) ShowWindow((plugin)->gui->window, (visible) ? SW_SHOW : SW_HIDE)
static void GUIOnPOSIXFD(MyPlugin *) {}