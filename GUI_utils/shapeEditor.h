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
    curveCenter, // adjusts curveCenter and therefore parameter of curve function of next point to the right
    modulate // select a parameter to be modulated by an Envelope TODO this would be better as a boolean field in EnvelopeManager?
};

// Behaviour of EnvelopeManager::processMouseDrag is dependent on the place the mouse has clicked when started dragging. The different dragging modes are stored here.
enum envelopeManagerDraggingMode{
    envNone, // Dont do anything.
    addLink, // Trying to find a parameter the activeEnvelope can be linked to.
    moveKnob // Moving a knob and updating the modulation amount of the corresponding LinkedParameter.
};

// Different parameters modulated by envelopes require different treatment. These are the types of parameters that can be modulated.
enum modulationMode{
    modCurveCenterY,
    modPosX,
    modPosY
};

// Types of context menus that can be displayed when righclicking the GUI.
enum contextMenuType{
    menuNone, // Dont show any menu
    menuShapePoint, // Show menu corresponding to a shapePoint, in which the point can be deleted and the interpolation mode can be changed.
    menuLinkKnob //Show menu corresponding to a linkKnob, in which the link can be removed.
};

// Corresponds to the options showed in the context menu appearing when rightclicking onto the linkKnobs.
enum menuLinkKnobOptions{
    removeLink // Remove the rightclicked link.
};

class ShapePoint;

class ShapeEditor{
    protected:
        uint32_t *canvas; // List of pixel values on the window.

        ShapePoint *currentlyDragging = nullptr; // Pointer to the shapePoint that is currently edited by the user.
        ShapePoint *rightClicked = nullptr;

    public:
        uint32_t XYXY[4]; // Box coordinates of the area where the shapePoints are defined, in XYXY notation.
        uint32_t XYXYFull[4]; // Box coordinates of the XYXY field extended by a margin

        bool pointDeleted = false; // Set to true if a point has been deleted

        draggingMode currentDraggingMode = none; // Editing mode of the shapePoint that is currently edited by the user.

        ShapePoint *shapePoints;

        ShapeEditor(uint32_t position[4]);
        void drawGraph(uint32_t *bits);
        ShapePoint* getClosestPoint(uint32_t x, uint32_t y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);
        void processMouseClick(int32_t x, int32_t y);
        ShapePoint* processDoubleClick(uint32_t x, uint32_t y);
        contextMenuType processRightClick(uint32_t x, uint32_t y);
        void processMenuSelection(WPARAM wParam);
        void processMouseRelease();
        void drawConnection(uint32_t *bits, ShapePoint *point, uint32_t color = 0x000000, float thickness = 5);
        void processMouseDrag(int32_t x, int32_t y);
        float forward(float input);
};

/*Class that stores all information necessary to modulate a parameter by an Envelope, i.e. pointers to the affected shape points, amount of modulation and modulation mode.
Every modulatable parameter has a counterpart called [parameter_name_here]Mod. This value is the "active" value of the parameter and is used for displaying on GUI and rendering audio. It is recalculated at every audio sample. The raw parameter value is the "default" to which the modulation offset is added.

The "modulation" im referring to here is conceptually a modulation, but not technically speaking. These parameters are NOT reported to the host as modulatable or automatable parameter. The modulation is exclusively handled inside the plugin.*/
class LinkedParameter{
    private:
        ShapePoint *point; // The parent point of the linked parameter.
        float amount; // Amount of modulation.
        float amountDragged; // Amount of modulation used when value is changed, is copied to amount when mouse is released.
        modulationMode mode; // The mode in which the point is modulated.

    public:
        LinkedParameter(ShapePoint *inPoint, float inAmount, modulationMode inMode);
        void modulate(float modOffset);
        void setAmount(float amount);
        float getAmount();
        ShapePoint *getLinkedPoint();
        void processMouseRelease();
        void reset();
};

// Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove and update controlled parameters.
class Envelope : public ShapeEditor{
    public:
        std::vector<LinkedParameter> linkedParameters; // Vector of all LinkedParameters associated with this Envelope.
        Envelope(uint32_t size[4]);
        void addLinkedParameter(ShapePoint *point, float amount, modulationMode mode);
        void setLinkedParameterAmount(int index, float amount);
        void removeLinkedParameter(int index);
        int getLinkedParameterNumber();
        float getModAmount(int index);
        void modulateLinkedParameters(double beatPosition, double sampleTimeOffset = 0);
        void resetLinkedParameters();
        void processRightClickMod(int32_t x, int32_t y);
        void processMouseRelease();
};

// Class in which Envelopes can be edited and linked to Parameters of the ShapeEditors. Only one Envelope is displayed and editable, user inputs are forwarded to this Envelope.
class EnvelopeManager{
    private:
        std::vector<Envelope> envelopes;
        uint32_t XYXY[4]; // Size and position of this EnvelopeManager in XYXY notation.
        uint32_t envelopeXYXY[4]; // Size and position at which the active Envelope is displayed.
        uint32_t selectorXYXY[4]; // Size and position at which the Envelope selection panel is displayed.
        uint32_t toolsXYXY[4]; // Size and position of the tool panel below the Envelope.
        
        uint32_t clickedX; // x-position of last mouseclick
        uint32_t clickedY; // y-position of last mouseclick
        
        void setActiveEnvelope(int index);
        void addEnvelope();
        void drawKnobs(uint32_t *canvas);
        int selectedKnob = -1; // Index of the knob which is currently edited (rightclicked or moved). Set to -1 if no knob is edited.
        int activeEnvelopeIndex; // Index of the displayed and editable Envelope.
        bool updateGUIElements = false; // Set to true if an action requires the GUI elements to rerender. Will rerender selector and 3D frame around Envelope.
        bool toolsUpdated; // Set to true if any of the tools has been updated and needs to be rerendered.

    public:
        envelopeManagerDraggingMode currentDraggingMode = envNone;
        uint32_t draggedToX; // Last x-position the mouse was dragged to after clicking onto the EnvelopeManager GUI. Is used to determine if it has been dragged to a ShapePoint for modulation. 
        uint32_t draggedToY; // Last y-position the mouse was dragged to after clicking onto the EnvelopeManager GUI. Is used to determine if it has been dragged to a ShapePoint for modulation. 

        EnvelopeManager(uint32_t XYXY[4]);
        Envelope* getActiveEnvelope();
        void setupFrames(uint32_t *canvas);
        void renderGUI(uint32_t	*canvas);
        void processMouseClick(uint32_t x, uint32_t y);
        void processDoubleClick(uint32_t x, uint32_t y);
        contextMenuType processRightClick(uint32_t x, uint32_t y);
        void processMenuSelection(WPARAM wParam);
        void processMouseRelease();
        void processMouseDrag(uint32_t x, uint32_t y);
        void addLinkedParameter(ShapePoint *point, float amount, modulationMode mode);
        void clearLinksToPoint(ShapePoint *point);
        void modulateLinkedParameters(double beatPosition, double sampleTimeOffset = 0);
        void resetLinkedParameters();
};
