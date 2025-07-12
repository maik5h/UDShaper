#include "contextMenus.h"

contextMenuType MenuRequest::requestedMenu = menuNone;
contextMenuType MenuRequest::currentMenu = menuNone;
InteractiveGUIElement *MenuRequest::requestOwner = nullptr;

void showShapeMenu(HWND hwnd, int xPos, int yPos) {
    HMENU hMenu = CreatePopupMenu();

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

void showPointPositionModMenu(HWND hwnd, int xPos, int yPos) {
	HMENU hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_STRING, modPosX, "X");
	AppendMenu(hMenu, MF_STRING, modPosY, "Y");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void showLoopModeMenu(HWND hwnd, int xPos, int yPos) {
	HMENU hMenu = CreatePopupMenu();

	AppendMenu(hMenu, MF_STRING, envelopeFrequencyTempo, "Tempo");
	AppendMenu(hMenu, MF_STRING, envelopeFrequencySeconds, "Seconds");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void showDistortionModeMenu(HWND hwnd, int xPos, int yPos) {
	HMENU hMenu = CreatePopupMenu();

	AppendMenu(hMenu, MF_STRING, upDown, "Up/Down");
	AppendMenu(hMenu, MF_STRING, leftRight, "Left/Right");
	AppendMenu(hMenu, MF_STRING, midSide, "Mid/Side");
	AppendMenu(hMenu, MF_STRING, positiveNegative, "+/-");

    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);
    DestroyMenu(hMenu);
}

void MenuRequest::sendRequest(InteractiveGUIElement *owner, contextMenuType type) {
    requestOwner = owner;
    requestedMenu = type;
}

void MenuRequest::showMenu(HWND window, uint32_t x, uint32_t y) {
    if (requestedMenu == menuNone) return;

    RECT rect;
    GetWindowRect(window, &rect);

    switch (requestedMenu) {
        case menuDistortionMode: {
            showDistortionModeMenu(window, rect.left + x, rect.top + y);
            break;
        }
        case menuEnvelopeLoopMode: {
            showLoopModeMenu(window, rect.left + x, rect.top + y);
            break;
        }
        case menuLinkKnob: {
            showLinkKnobMenu(window, rect.left + x, rect.top + y);
            break;
        }
        case menuPointPosMod: {
            showPointPositionModMenu(window, rect.left + x, rect.top + y);
            break;
        }
        case menuShapePoint: {
            showShapeMenu(window, rect.left + x, rect.top + y);
            break;
        }
    }
    currentMenu = requestedMenu;
    requestedMenu = menuNone;
}

void MenuRequest::processSelection(int menuItem) {
    requestOwner->processMenuSelection(currentMenu, menuItem);
    currentMenu = menuNone;
}
