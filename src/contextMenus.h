#pragma once

#include <windows.h>
#include "assets.h"
#include "enums.h"

// Shows a context menu from which the shape of the curve corresponding to the right clicked point can be selected.
// Current shapes are:
// 	-Power (default)
// 	-Sine (not implemented)
void showShapeMenu(HWND hwnd, int xPos, int yPos);

// Shows a context corresponding to a link knob. Currently the only option is to remove the knob.
void showLinkKnobMenu(HWND hwnd, int xPos, int yPos);

// Shows a context menu from which the modulation mode of a ShapePoint can be selected, if the user
// tries to link an Envelope to this point.
void showPointPositionModMenu(HWND hwnd, int xPos, int yPos);

// Shows a context menu to select an Envelope loop mode.
// Menu options are:
//  - Tempo
//  - Seconds
void showLoopModeMenu(HWND hwnd, int xPos, int yPos);

// Shows a context menu to selectthe plugin distortion mode.
// Menu options are:
//  - Up/Down
//  - Left/Right
//  - Mid/Side
//  - +/-
void showDistortionModeMenu(HWND hwnd, int xPos, int yPos);


// MenuRequest is a static class to manage context menus. It acts as a bridge between the plugin
// logic and windows dependent features.
//
// The intended life-cycle of a context menu is:
//  - An InteractiveGUIElement sends a request through MenuRequest::sendRequest.
//  - MenuRequest::showMenu is called from within the window procedure to open the menu.
//  - If the user selects an option, MenuRequest::processSelection is called, which forwards the
//    selected menu item to the InteractiveGUIElement that initially requested the menu.
class MenuRequest {
    static InteractiveGUIElement *requestOwner; // The element that submitted the request.
    static contextMenuType requestedMenu;       // The type of menu to be displayed.
    static contextMenuType currentMenu;         // The type of menu currently being displayed.

    public:

    // Request a context menu to be displayed.
    //  - owner:    Pointer to the InteractiveGUIElement that submitted the request.
    //  - type:     Type of menu to be shown.
    static void sendRequest(InteractiveGUIElement *owner, contextMenuType type);

    // Show the currently requested context menu.
    //  - window:   The target window on which the menu is shown.
    //  - x:        x-position of the context menu within the window.
    //  - y:        y-position of the context menu within the window.
    static void showMenu(HWND window, uint32_t x, uint32_t y);

    // Process the menu item selected by the user. Forwards the selected item to the
    // InteractiveGUIElement that sent the request to open this context menu.
    //  - menuItem: The selected menu item cast to int.
    static void processSelection(int menuItem);
};
