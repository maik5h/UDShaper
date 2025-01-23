/*
Everything concerning the shape editors.
A shape editor is a region on the UI that can be used to design a curve by adding, moving and deleting points between
which the curve is interpolated. The way the curve is interpolated in each region between two points ("curve segment") can be
chosen from a dialog box if rightclicking the point to the right of the segment. There are the options power and sine, see implementation for further explanations.
Some shapes can further be edited by dragging the center of the curve.

Envelopes are shapeEditors that can be used to modulate certain parameters.
*/

#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <tuple>
#include <cmath>
#include <iostream>
#include <assert.h>
#include <set>
#include <windows.h>
#include "../config.h"

// Function each curve segment between two points can follow
enum Shapes{
    shapePower, // curve follows shape of f(x) = (x < 0.5) ? x^power : 1-(1-x)^power, for 0 <= x <= 1, streched to the corresponding x and y intervals
    shapeSine,
    shapeBezier // i think i will drop this because i do NOT feel like programming this... more points and some more clamping to the box etc
};

// Decide which parameters to change when MouseDrag is processed depending on these values 
enum draggingMode{
    none, // ignore
    position, // moves position of selected point
    curveCenter // adjusts curveCenter and therefore parameter of curve function of next point to the right
};

// Different parameters modulated by envelopes require different treatment. These are the types of parameters that can be modulated.
enum modulationMode{
    modCurveCenterY,
    modPosX,
    modPosY
};

class ShapePoint;

class ShapeEditor{
    private:
        static float requiredSquaredDistance; // Square of the minimum distance the mouse needs to have from a point in order to connect any input to this point

        uint32_t *canvas; // List of pixel values on the window.

        ShapePoint *currentlyDragging = nullptr; // Pointer to the shapePoint that is currently edited by the user.
        draggingMode currentDraggingMode = none; // Editing mode of the shapePoint that is currently edited by the user.
        ShapePoint *rightClicked = nullptr;

    public:
        uint16_t XYXY[4];
        std::vector<ShapePoint> shapePoints; // All points of this editor are stored in a vector, they must always be sorted by ther absolute x-position.
        ShapePoint *pointStorage; // The memory address of the vector memory is stored here and updated every time an element is added or removed, so the correct memory block can always be accessed from outside the object through this value.


        ShapeEditor(uint16_t position[4], std::vector<ShapePoint> points = {});
        void drawGraph(uint32_t *bits);
        std::tuple<float, int> getClosestPoint(uint32_t x, uint32_t y);
        void processMousePress(int32_t x, int32_t y);
        int processDoubleClick(uint32_t x, uint32_t y);
        ShapePoint* processRightClick(uint32_t x, uint32_t y);
        void processMenuSelection(WPARAM wParam);
        void processMouseRelease();
        void drawConnection(uint32_t *bits, int index, uint32_t color = 0x000000, float thickness = 5);
        void processMouseDrag(int32_t x, int32_t y);
        float forward(float input);
};

class ModulatedParameter;

class Envelope : public ShapeEditor{
    private:
        std::vector<ModulatedParameter> modulatedParameters;

    public:
        Envelope(uint16_t size[4]);
        void addControlledParameter(ShapePoint **point, int index, float amount, modulationMode mode);
        void removeControlledParameter(int index);
        void updateModulatedParameters(double beatPosition);
        void resetModulatedParameters();
        void processMousePressMod(int32_t x, int32_t y);
        void processRightClickMod(int32_t x, int32_t y);
        void updateIndexAfterPointAdded(int addedIndex);

};
