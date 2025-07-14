#include "GUI.h"

static int globalOpenGUICount = 0;

// Calls renderGUI on the plugin and redraws the window.
void GUIPaint(UDShaper *plugin, bool internal) {
	if (plugin->gui){
		if (internal) plugin->renderGUI(plugin->gui->bits);
		RedrawWindow(plugin->gui->window, 0, 0, RDW_INVALIDATE);
	}
}

void GUI::resize(uint32_t newWidth, uint32_t newHeight) {
	SetWindowPos(window, NULL, 0, 0, newWidth, newHeight, SWP_NOMOVE | SWP_NOZORDER);
}

void GUI::setParentWindow(const clap_window_t *parentWindow) {
	SetParent(window, static_cast<HWND>(parentWindow->win32));
}

void GUI::showWindow(bool visible) {
	ShowWindow(window, (visible) ? SW_SHOW : SW_HIDE);
}

void drawTextBox(uint32_t *canvas, TextBoxInfo info){
	HDC hdcGUI = CreateCompatibleDC(NULL);
	BITMAPINFO bitmapInfo = { { sizeof(BITMAPINFOHEADER), info.GUIWidth, -info.GUIHeight, 1, 32, BI_RGB } };

	void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcGUI, &bitmapInfo, DIB_RGB_COLORS, &pBits, NULL, 0);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(hdcGUI, hBitmap);

	// Load GUI bits from canvas to pBits.
	memcpy(pBits, canvas, info.GUIWidth * info.GUIHeight * sizeof(uint32_t));

	// Draw text to hdcGUI.
	RECT rect{info.position[0], info.position[1], info.position[2], info.position[3]};
	uint32_t textHeight = static_cast<uint32_t>(info.textHeight * info.position[3]-info.position[1]);
	HFONT hFont = CreateFont(
        textHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, TEXT("Arial")
    );
    SelectObject(hdcGUI, hFont);
	SetTextColor(hdcGUI, RGB((info.colorText & 0x00FF0000) >> 16, (info.colorText & 0x0000FF00) >> 8, (info.colorText & 0x000000FF)));
	SetBkMode(hdcGUI, TRANSPARENT);
    DrawText(hdcGUI, info.text.c_str(), -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	// Load updated gui bitmap back into canvas.
	memcpy(canvas, pBits, info.GUIWidth * info.GUIHeight * sizeof(uint32_t));

    SelectObject(hdcGUI, oldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcGUI);

	if (info.frameWidth) {
		drawFrame(canvas, info.GUIWidth, info.position, info.frameWidth, info.colorFrame, info.alphaFrame);
	}
}

LRESULT CALLBACK GUIWindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
	UDShaper *plugin = (UDShaper *) GetWindowLongPtr(window, 0);

	if (!plugin) return DefWindowProc(window, message, wParam, lParam);

	switch (message){
		// Copies plugin->gui->bits to the window DC.
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC dc = BeginPaint(window, &paint);
			BITMAPINFO info = { { sizeof(BITMAPINFOHEADER), plugin->gui->width, -plugin->gui->height, 1, 32, BI_RGB } };
			StretchDIBits(dc, 0, 0, plugin->gui->width, plugin->gui->height, 0, 0, plugin->gui->width, plugin->gui->height, plugin->gui->bits, &info, DIB_RGB_COLORS, SRCCOPY);
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
		case WM_RBUTTONUP: {
			SetCapture(window);

			plugin->processRightClick(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			MenuRequest::showMenu(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

			GUIPaint(plugin, true);
			break;
		}
		// Process the menu selection for all ShapeEditor and Envelope instances.
		case WM_COMMAND: {
			MenuRequest::processSelection(static_cast<int>(wParam));
			GUIPaint(plugin, true);
			break;
		}
		case WM_LBUTTONUP: {
			ReleaseCapture();

			plugin->processMouseRelease(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			MenuRequest::showMenu(window, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

			GUIPaint(plugin, true);
			break;
		}
		default:
			return DefWindowProc(window, message, wParam, lParam);
	}
	return 0;
}

// Creates windowclass and window and draws elements on the GUI that are not updated afterwards,
// such as frames around elements.
void GUICreate(UDShaper *plugin, clap_plugin_descriptor_t pluginDescriptor) {
	plugin->gui = new GUI();
	plugin->gui->width = GUI_WIDTH_INIT;
	plugin->gui->height = GUI_HEIGHT_INIT;

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
			CW_USEDEFAULT, 0, plugin->gui->width, plugin->gui->height, GetDesktopWindow(), NULL, NULL, NULL);
	plugin->gui->bits = (uint32_t *) calloc(1, plugin->gui->width * plugin->gui->height * 4);
	SetWindowLongPtr(plugin->gui->window, 0, (LONG_PTR) plugin);

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

// Changes the cursor to IDC_HAND to indicate dragging.
void setCursorDragging() {
	// Attempt to load dragging cursor IDC_HAND and set cursor if successful.
	HCURSOR hCursor = LoadCursor(NULL, IDC_HAND);
	if (hCursor) SetCursor(hCursor);
}