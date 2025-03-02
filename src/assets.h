#pragma once

#include <cstdint>
#include <string>
#include <cmath>
#include <windows.h>
#include "../config.h"

// Abstract base class for all elements that contribute to the plugin GUI and react to user inputs.
class InteractiveGUIElement {
    virtual void processLeftClick(uint32_t x, uint32_t y) = 0;
    virtual void processMouseDrag(uint32_t x, uint32_t y) = 0;
    virtual void processMouseRelease(uint32_t x, uint32_t y) = 0;
    virtual void processDoubleClick(uint32_t x, uint32_t y) = 0;
    virtual void processRightClick(uint32_t x, uint32_t y) = 0;
    virtual void renderGUI(uint32_t *canvas, double beatPosition) = 0;
};

// Checks if the coordinates x, y are in the given box.
inline bool isInBox(int x, int y, uint32_t box[4]){
    return (x > box[0]) && (x < box[2]) && (y > box[1]) && (y < box[3]);
}

// Checks if the distance from (x, y) to (pointX, pointY) is smaller or equal to radius;
inline bool isInPoint(int x, int y, int pointX, int pointY, int radius){
    return ((x - pointX)*(x - pointX) + (y - pointY)*(y - pointY) <= radius*radius);
};

void fillRectangle(uint32_t *canvas, uint32_t XYXY[4], uint32_t color = colorBackground);

void drawPoint(uint32_t *canvas, float x, float y, uint32_t color = 0x000000, float size = 5);

void drawFrame(uint32_t *canvas, uint32_t innerRectangle[4], int thickness = 15, uint32_t color = 0x000000, float alpha = 1);

void draw3DFrame(uint32_t *canvas, uint32_t innerRectangle[4], uint32_t baseColor, int thickness = 15);

void drawGrid(uint32_t *canvas, uint32_t box[4], int numberLines, int thickness, uint32_t color, float alpha=1);

void drawArrow(uint32_t *canvas, uint32_t box[4], bool up, float sizeFactor);

void drawLinkKnob(uint32_t *canvas, uint32_t x, uint32_t y, uint32_t size, float value, uint32_t color = colorEditorBackground);

void drawText(HWND hwnd, int x, int y, LPCSTR text, uint32_t color);
