#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <tuple>
#include <cmath>
#include <iostream>
#include <assert.h>
#include <map>
#include <windows.h>
#include "../config.h"
#include "assets.h"

/*
Everything concerning the shape editors.
A shape editor is a region on the UI that can be used to design a curve by adding, moving and deleting points between
which the curve is interpolated. The way the curve is interpolated in each region between two points ("curve segment")
can be chosen from a dialog box by rightclicking the point to the right of the segment. There are the options power
and sine, see implementation for further explanations. Some shapes can further be edited by dragging the center of the
curve.

Envelopes are shapeEditors that can be used to modulate certain parameters, but are not intended to be modulated
themself.
*/

// Interpolation modes between two points.
enum Shapes{
    shapePower, // Curve follows shape of f(x) = (x < 0.5) ? x^power : 1-(1-x)^power, for 0 <= x <= 1, streched to the corresponding x and y intervals.
    shapeSine,  // Curve is a sine that continuously connects the previous and next point. 
};

// Decide the actions performed when MouseDrag is processed by a ShapeEditor based on these values.
enum shapeEditorDraggingMode{
    none,           // ignore
    position,       // Moves position of selected point.
    curveCenter,    // Adjusts curveCenter and therefore parameter of curve function of next point to the right.
    modulate        // Selects a parameter to be modulated by the active Envelope
};

// Decide the actions performed when MouseDrag is processed by a Envelope based on these values.
enum envelopeManagerDraggingMode{
    envNone, // Dont do anything.
    addLink, // Trying to find a parameter the activeEnvelope can be linked to.
    moveKnob // Moving a knob and updating the modulation amount of the corresponding ModulatedParameter.
};

// Different parameters modulated by envelopes require different treatment. These are the types of parameters that can be modulated.
enum modulationMode{
    modCurveCenterY,    // Modulation affects whatever value is associated with the curveCenterPosY in the current interpolation mode.
    modPosX,            // Modulation changes the x-position of the modulated ShapePoint.
    modPosY             // Modulation changes the y-position of the modulated ShapePoint.
};

// Types of context menus that can be displayed when righclicking the GUI.
enum contextMenuType{
    menuNone,       // Dont show a menu.
    menuShapePoint, // Show menu corresponding to a shapePoint, in which the point can be deleted and the interpolation mode can be changed.
    menuLinkKnob,   // Show menu corresponding to a linkKnob, in which the link can be removed.
    menuPointPosMod // Show menu to select either x or y-direction for point position modeulation.
};

// Corresponds to the options showed in the context menu appearing when rightclicking onto the linkKnobs.
// TODO add something like set, copy and paste value.
enum menuLinkKnobOptions{
    removeLink // Remove the rightclicked link.
};

class ShapePoint;

class ShapeEditor : public InteractiveGUIElement {
    protected:
    ShapePoint *currentlyDragging = nullptr; // Pointer to the ShapePoint that is currently edited by the user.
    ShapePoint *rightClicked = nullptr; // Pointer to the ShapePoint that has been rightclicked by the user.

    contextMenuType menuRequest = menuNone; // If any action requires a menu to be opened, the type of menu is stored here.
    ShapePoint *deletedPoint = nullptr; // If a point was deleted, store a pointer to it here, so that the plugin can remove all Envelope links after the input is fully processed.

    void drawConnection(uint32_t *canvas, ShapePoint *point, double beatPosition = 0., uint32_t color = 0x000000, float thickness = 5);

    public:
    uint32_t XYXY[4]; // Box coordinates of the area where the shapePoints are defined, in XYXY notation.
    uint32_t XYXYFull[4]; // Box coordinates of the XYXY field extended by a margin.

    shapeEditorDraggingMode currentDraggingMode = none; // Indicates which action should be performed when processing the mouse drag. Is set based on the position where the dragging started.

    /*Pointer to the first ShapePoint in this ShapeEditor. ShapePoints are stored in a doubly linked list.

    The first point must be excluded from all methods since it can not be edited and displayed. All loops over the ShapePoints must therefore start at shapePoints->next.
    The last point must not be moved in x-direction or deleted. */
    ShapePoint *shapePoints;

    ShapeEditor(uint32_t position[4]);
    ShapePoint* getClosestPoint(uint32_t x, uint32_t y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas, double beatPosition = 0);

    contextMenuType getMenuRequestType();
    ShapePoint *getDeletedPoint();
    
    void processMenuSelection(WPARAM wParam);
    float forward(float input, double beatPosition = 0.);
};

class ModulatedParameter;

// Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove and update controlled parameters.
class Envelope : public ShapeEditor{
    public:
    std::vector<ModulatedParameter*> modulatedParameters;
    Envelope(uint32_t size[4]);
    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode);
    void setModulatedParameterAmount(int index, float amount);
    void removeModulatedParameter(int index);
    int getModulatedParameterNumber();
    float getModAmount(int index);
    float modForward(double beatPosition = 0);
    void processRightClickMod(int32_t x, int32_t y);
};

// Class in which Envelopes can be edited and linked to Parameters of the ShapeEditors.
//
// Consists of three parts: An Envelope, a selection panel left to the Envelope and a tool panel below Envelope and selection panel.
// Only one Envelope is displayed and editable, the active Envelope can be switched on the selection panel. The tool panel shows knobs for all ModulatedParameters that are linked to the active Envelope. These knobs control the modulation amount of the corresponding parameter.
class EnvelopeManager : public InteractiveGUIElement {
    private:
    std::vector<Envelope> envelopes; // Vector of Envelopes. To prevent reallocation and dangling pointers in ModulatedParameters, MAX_NUMBER_ENVELOPES spots is reserved and no more Envelopes can be added.
    uint32_t XYXY[4]; // Size and position of this EnvelopeManager in XYXY notation.
    uint32_t envelopeXYXY[4]; // Size and position at which the active Envelope is displayed.
    uint32_t selectorXYXY[4]; // Size and position at which the Envelope selection panel is displayed.
    uint32_t toolsXYXY[4]; // Size and position of the tool panel below the Envelope.
    
    uint32_t clickedX; // x-position of last mouseclick
    uint32_t clickedY; // y-position of last mouseclick

    contextMenuType menuRequest = menuNone;
    
    void setActiveEnvelope(int index);
    void addEnvelope();
    void drawKnobs(uint32_t *canvas);
    int selectedKnob = -1; // Index of the knob which is currently edited (rightclicked or moved). Set to -1 if no knob is edited.
    float selectedKnobAmount; // Amount the clicked knob was set to. Is only updated on mouseClick, not mouseDrag and can be used to correctly update the current value while dragging the knob.
    int activeEnvelopeIndex; // Index of the displayed and editable Envelope.
    bool updateGUIElements = false; // Set to true if an action requires the GUI elements to rerender. Will rerender selector and 3D frame around Envelope.
    bool toolsUpdated; // Set to true if any of the tools has been updated and needs to be rerendered.


    public:
    envelopeManagerDraggingMode currentDraggingMode = envNone;
    ShapePoint *attemptedToModulate = nullptr; // Points to a ShapePoint that was linked to the active Envelope by the user, but is waiting for the user to select the X or Y direction in the menu before it can actually be added as a ModulatedParameter.

    EnvelopeManager(uint32_t XYXY[4]);
    void setupFrames(uint32_t *canvas);
    
    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t	*canvas, double beatPosition = 0);

    void processMenuSelection(WPARAM wParam);
    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode);
    void clearLinksToPoint(ShapePoint *point);

    contextMenuType getMenuRequestType();
};
