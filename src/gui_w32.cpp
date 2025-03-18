#include "GUI.h"

static int globalOpenGUICount = 0;

// Calls renderGUI on the plugin and redraws the window.
void GUIPaint(UDShaper *plugin, bool internal) {
	if (plugin->gui){
		if (internal) plugin->renderGUI(plugin->gui->bits);
		RedrawWindow(plugin->gui->window, 0, 0, RDW_INVALIDATE);
	}
}

// Shows a context menu from which the shape of the curve corresponding to the right clicked point can be selected.
// Current shapes are:
// 	-Power (default)
// 	-Sine (not implemented)
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

// Shows a context corresponding to a link knob. Currently the only option is to remove the knob.
void showLinkKnobMenu(HWND hwnd, int xPos, int yPos) {
    HMENU hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING, removeLink, "Remove Link");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    DestroyMenu(hMenu);
}

// Shows a context menu from which the modulation mode of a ShapePoint can be selected, if the user tries to link an Envelope to this point.
void showPointPositionModMenu(HWND hwnd, int xPos, int yPos, bool hideX = false) {
	HMENU hMenu = CreatePopupMenu();

	if (!hideX) {
		AppendMenu(hMenu, MF_STRING, modPosX, "X");
	}
	AppendMenu(hMenu, MF_STRING, modPosY, "Y");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    DestroyMenu(hMenu);
}

// Draws a textbox to plugin->gui->bits. xMin, yMin, xMax and yMax determine the size of the textbox. A frame is drawn outside of these coordinates.
// The textsize is dependent on the height of the given box.
void drawTextBox(UDShaper *plugin, uint32_t xMin, uint32_t yMin, uint32_t xMax, uint32_t yMax){
	// Make compatible DC and bitmap to store current GUI bitmap in
	HDC hdcWindow = GetDC(plugin->gui->window);
	HDC hdcGUI = CreateCompatibleDC(hdcWindow);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, GUI_WIDTH, GUI_HEIGHT);
	HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcGUI, hBitmap); // Select bitmap into memory DC
	BITMAPINFO info = { { sizeof(BITMAPINFOHEADER), GUI_WIDTH, -GUI_HEIGHT, 1, 32, BI_RGB } };

	// load previous gui bitmap into hdcGUI
	SetDIBits(hdcGUI, hBitmap, 0, GUI_HEIGHT, plugin->gui->bits, &info, DIB_RGB_COLORS);

	// draw text to hdcGUI
	RECT rect{xMin, yMin, xMax, yMax};
	HFONT hFont = CreateFont(
        yMax-yMin, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TEXT("Arial")
    );
    SelectObject(hdcGUI, hFont);
	SetTextColor(hdcGUI, RGB(255, 255, 255));
	SetBkMode(hdcGUI, TRANSPARENT);
    DrawText(hdcGUI, "Left/Right", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Load updated gui bitmap back into plugin->gui->bits
	GetDIBits(hdcGUI, hBitmap, 0, GUI_HEIGHT, plugin->gui->bits, &info, DIB_RGB_COLORS);

    SelectObject(hdcGUI, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcGUI);

	// Draw a frame around the text.
	uint32_t box[4] = {xMin, yMin, xMax, yMax};
	drawFrame(plugin->gui->bits, box, 5, 0x000000, 0.45);
}

LRESULT CALLBACK GUIWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	UDShaper *plugin = (UDShaper *) GetWindowLongPtr(window, 0);

	if (!plugin) {
		return DefWindowProc(window, message, wParam, lParam);
	}

	switch (message){
		// Copies plugin->gui->bits to the window DC.
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC dc = BeginPaint(window, &paint);
			BITMAPINFO info = { { sizeof(BITMAPINFOHEADER), GUI_WIDTH, -GUI_HEIGHT, 1, 32, BI_RGB } };
			StretchDIBits(dc, 0, 0, GUI_WIDTH, GUI_HEIGHT, 0, 0, GUI_WIDTH, GUI_HEIGHT, plugin->gui->bits, &info, DIB_RGB_COLORS, SRCCOPY);
			EndPaint(window, &paint);
			break;
		}
		case WM_MOUSEMOVE: {
			plugin->processMouseDrag(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			GUIPaint(plugin, true);
			break;
		}
		case WM_LBUTTONDOWN: {
			SetCapture(window); 
			plugin->processLeftClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			GUIPaint(plugin, true);
			break;
		}
		case WM_LBUTTONDBLCLK: {
			SetCapture(window); 
			plugin->processDoubleClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			GUIPaint(plugin, true);
			break;
		}
		// Can open one of the following context menus:
		// 	-menu to select curve shape, when rightclicked on a ShapePoint
		// 	-menu with link knob options, when rightclicked on a link knob
		case WM_RBUTTONUP: {
			SetCapture(window);

			plugin->processRightClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			contextMenuType menuType = plugin->getMenuRequestType();

			// If any of the processRightClick functions returned true, open the context menu for points. Information about which point was rightclicked is handled inside the ShapeEditor and Envelope instances.
			if (menuType != menuNone){
				RECT rect; // rect to store window coordinates
				GetWindowRect(window, &rect);
				if (menuType == menuShapePoint){
					showShapeMenu(window, rect.left + GET_X_LPARAM(lParam), rect.top + GET_Y_LPARAM(lParam));
				}
				else if (menuType == menuLinkKnob){
					showLinkKnobMenu(window, rect.left + GET_X_LPARAM(lParam), rect.top + GET_Y_LPARAM(lParam));
				}
			}
			GUIPaint(plugin, true);
			break;
		}
		// Process the menu selection for all ShapeEditor and Envelope instances, does only affect the point which has been rightclicked.
		case WM_COMMAND: {
			plugin->shapeEditor1->processMenuSelection(wParam);
			plugin->shapeEditor2->processMenuSelection(wParam);
			plugin->envelopes->processMenuSelection(wParam);

			GUIPaint(plugin, true);
			break;
		}
		// Opens a context menu to select either X or Y direction, if mouse has dragged from an Envelope and was released on a ShapePoint.
		case WM_LBUTTONUP: {
			ReleaseCapture();
			plugin->processMouseRelease(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			if (plugin->getMenuRequestType() == menuPointPosMod){
				RECT rect;
				GetWindowRect(window, &rect);
				showPointPositionModMenu(window, rect.left + GET_X_LPARAM(lParam), rect.top + GET_Y_LPARAM(lParam));
			}
			GUIPaint(plugin, true);
			break;
		}
		default:
			return DefWindowProc(window, message, wParam, lParam);
	}

	return 0;
}

// Creates windowclass and window and draws some elements on the GUI, that are not updated afterwards, such as frames around elements.
void GUICreate(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor) {
	plugin->gui = new GUI();

	if (globalOpenGUICount++ == 0) {
		WNDCLASS windowClass = {};
		windowClass.lpfnWndProc = GUIWindowProcedure;
		windowClass.cbWndExtra = sizeof(UDShaper *);
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

	draw3DFrame(plugin->gui->bits, plugin->shapeEditor1->XYXYFull, colorEditorBackground);
	draw3DFrame(plugin->gui->bits, plugin->shapeEditor2->XYXYFull, colorEditorBackground);
	plugin->envelopes->setupFrames(plugin->gui->bits);

	uint32_t outerFrameOffset = 28;
	uint32_t outerFrame[4] = {plugin->shapeEditor1->XYXYFull[0] - outerFrameOffset, plugin->shapeEditor1->XYXYFull[1] - outerFrameOffset, plugin->shapeEditor2->XYXYFull[2] + outerFrameOffset, plugin->shapeEditor2->XYXYFull[3] + outerFrameOffset};
	drawFrame(plugin->gui->bits, outerFrame, 5, 0x000000, 0.45);

	plugin->renderGUI(plugin->gui->bits);
}

void GUIDestroy(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor) {
	assert(plugin->gui);
	DestroyWindow(plugin->gui->window);
	free(plugin->gui->bits);
	delete plugin->gui;
	plugin->gui = nullptr;

	if (--globalOpenGUICount == 0) {
		UnregisterClass(pluginDescriptor.id, NULL);
	}
}

// Does nothing on windows.
void GUIOnPOSIXFD(UDShaper *) {}