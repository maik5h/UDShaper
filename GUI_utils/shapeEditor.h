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

// Shape for interpolating between two points
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
        uint16_t XYXY[4]; // Box coordinates of the area where the shapePoints are defined, in XYXY notation.
        uint16_t XYXYFull[4]; // Box coordinates of the XYXY field extended by a margin

        ShapePoint *shapePoints;

        ShapeEditor(uint16_t position[4]);
        void drawGraph(uint32_t *bits);
        std::tuple<float, ShapePoint*> getClosestPoint(uint32_t x, uint32_t y);
        void processMousePress(int32_t x, int32_t y);
        ShapePoint* processDoubleClick(uint32_t x, uint32_t y);
        bool processRightClick(uint32_t x, uint32_t y);
        void processMenuSelection(WPARAM wParam);
        void processMouseRelease();
        void drawConnection(uint32_t *bits, ShapePoint *point, uint32_t color = 0x000000, float thickness = 5);
        void processMouseDrag(int32_t x, int32_t y);
        float forward(float input);
};

/*Class that stores all information necessary to modulate a parameter, i.e. pointers to the affected shape points, amount of modulation and modulation mode.
Every modulatable parameter has a counterpart called [parameter_name_here]Mod. This value is the "active" value of the parameter and is used for displaying on GUI and rendering audio. It is recalculated at every audio sample. The raw parameter value is the "default" to which the modulation offset is added.

The "modulation" im referring to here is conceptually a modulation, but not technically speaking. These parameters are NOT reported to the host as modulatable or automatable parameter. The modulation is exclusively handled inside the plugin.*/
class ModulatedParameter{
    private:
    ShapePoint *point; // The point which is modulated
    float amount;
    modulationMode mode; // The mode in which the point is modulated

    public:
    ModulatedParameter(ShapePoint *inPoint, float inAmount, modulationMode inMode);
    void modulate(float modOffset);
    void reset();
};

// Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove and update controlled parameters
class Envelope : public ShapeEditor{
    private:
        std::vector<ModulatedParameter> modulatedParameters;

    public:
        Envelope(uint16_t size[4]);
        void addControlledParameter(ShapePoint *point, int index, float amount, modulationMode mode);
        void removeControlledParameter(int index);
        void updateModulatedParameters(double beatPosition);
        void resetModulatedParameters();
        void processMousePressMod(int32_t x, int32_t y);
        void processRightClickMod(int32_t x, int32_t y);
};
