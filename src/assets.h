// Helpful functions to draw different elements to a window or the window bitmap.
// Most functions use the array of bitmap values to draw. In order to correspond an index to a (x, y) coordinate
// on the window, they take the current width of the GUI as an argument.

#pragma once

#include <cstdint>
#include <string>
#include <cmath>
#include "../config.h"
#include "../color_palette.h"
#include "enums.h"

// Blends between originalColor and newColor, such that for alpha = 0 the original color is preserved
// and for alpha = 1 the new color is chosen.
uint32_t blendColor(uint32_t originalColor, uint32_t newColor, double alpha);

// Abstract base class for all elements that contribute to the plugin GUI and react to user inputs.
class InteractiveGUIElement {
    public:
    virtual void processLeftClick(uint32_t x, uint32_t y) = 0;
    virtual void processMouseDrag(uint32_t x, uint32_t y) = 0;
    virtual void processMouseRelease(uint32_t x, uint32_t y) = 0;
    virtual void processDoubleClick(uint32_t x, uint32_t y) = 0;
    virtual void processRightClick(uint32_t x, uint32_t y) = 0;
    virtual void renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed) = 0;
    virtual void rescaleGUI(uint32_t *canvas, uint32_t newWidth, uint32_t newHeight) = 0;
    virtual void processMenuSelection(contextMenuType menuType, int menuItem) = 0;
};

// Checks if the coordinates x, y are in the given box.
inline bool isInBox(int x, int y, uint32_t box[4]){
    return (x > box[0]) && (x < box[2]) && (y > box[1]) && (y < box[3]);
}

// Checks if the distance from (x, y) to (pointX, pointY) is smaller or equal to radius;
inline bool isInPoint(int x, int y, int pointX, int pointY, int radius){
    return ((x - pointX)*(x - pointX) + (y - pointY)*(y - pointY) <= radius*radius);
};


// The following functions act on a bitmap and do therefore not rely on platform dependent functions.
// All following functions receive the arguments
//  - canvas (uint32_t*):   Array of AARRGGBB colors with 0.5 * GUIWidth^2 elements.
//  - GUIWidth (uint32_t):  current width of the GUI.
//
// These functions do not check if the given position of the element lies within the plugin GUI.

// Fills the area given by XYXY with color.
void fillRectangle(uint32_t *canvas, uint32_t GUIWidth, uint32_t XYXY[4], uint32_t color = colorBackground);

// Draws a point of color and size at position (x, y) onto canvas.
void drawPoint(uint32_t *canvas, uint32_t GUIWidth, float x, float y, uint32_t color = 0xFF000000, float size = 5);

// Draws a circle with a given width and radius to position (x, y) on canvas. radius gives the radius of the
// outer edge.
void drawCircle(uint32_t *canvas, uint32_t GUIWidth, uint32_t x, uint32_t y, uint32_t color = 0xFF000000, uint32_t radius = 20, uint32_t width = 4);

// Draws a simple frame to canvas. The frame extends the number of pixel given in thickness from the innerBox.
void drawFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerRectangle[4], int thickness = 15, uint32_t color = 0xFF000000, float alpha = 1);

// Draw a frame that has each side shaded in a different color to create the impression of depth.
void draw3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerRectangle[4], uint32_t baseColor, int thickness = 15);

// Draws a 3D frame similar to the function draw3DFrame, but the segment on the right is missing and connects to
// another 3D frame positioned to the right.
void drawPartial3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerRectangle[4], uint32_t baseColor, uint32_t thickness = 15);

// Draws a grid inside box to canvas. Thickness is not yet implemented.
void drawGrid(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], int numberLines, int thickness, uint32_t color, float alpha=1);

void drawArrow(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], bool up, float sizeFactor);

// Draws a link knob at (x, y) to canvas. value is in [-1, 1] and represents the value associated with the knob
// and determines the knob position.
// Link knobs are used to control the links between Envelopes and Parameters, hence the name.
void drawLinkKnob(uint32_t *canvas, uint32_t GUIWidth, uint32_t x, uint32_t y, uint32_t size, float value, uint32_t color = colorEditorBackground);


// The following functions rely on platform dependent packages and are therefore not defined in assets.cpp
// but win_gui32.cpp. They are declared here instead of GUI.h to avoid circular imports.

// Container for text box settings that is passed to drawTextBox.
struct TextBoxInfo {
    uint32_t GUIWidth;                  // Width of the full plugin window.
    uint32_t GUIHeight;                 // Height of the full plugin window.
    std::string text;                   // The test to be displayed as string.
    uint32_t position[4];               // The size and position of the textbox in XYXY notation.
    uint32_t frameWidth = 5;            // The width of the frame around the textbox. 0 draws no frame.
    float textHeight = 1;               // Height of the text relative to the textbox height.
    uint32_t colorText = 0xFFFFFFFF;    // Color of the text.
    uint32_t colorFrame = 0xFF000000;   // Color of the frame.
    float alphaFrame = 1;               // Alpha value of the frame.
};

// Draws a textbox to canvas. info defines size and colors of text and surrounding frame.
// The textsize is dependent on the height of the given box. The width of the box must be large enough to contain the
// full text, else it will be cut off at the sides.
void drawTextBox(uint32_t *canvas, TextBoxInfo info);

// Changes the cursor icon to indicate dragging.
void setCursorDragging();