// Subelements of the UDShaper plugin are declared in this file.
// All subelements are derived from InteractiveGUIElement (see assets.h) and are assigned a fixed region on
// the UDShaper GUI on which they can draw their GUI.
// There are the classes TopMenuBar, ShapeEditor, Envelope, FreuqnecyPanel and EnvelopeManager.
//
// ShapeEditors are visual graph editors. Envelopes are visual graph editors that can modulate ShapeEditor parameters.
// The modulation speed of Envelopes can be set with a FrequencyPanel. Envelopes and FrequencyPanels are stored
// and managed inside EnvelopeManager.

#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <tuple>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <assert.h>
#include <map>
#include <clap/clap.h>
#include "config.h"
#include "color_palette.h"
#include "assets.h"
#include "string_presets.h"
#include "GUILayout.h"
#include "contextMenus.h"
#include "logging.h"
#include "enums.h"

// Renders the menu bar at the top of the plugin and handles all user inputs on this area.
// The menu bar will consist of: The plugin logo (TODO), a button to select the distortion mode and a panel to select presets (TODO).
class TopMenuBar: public InteractiveGUIElement {
    TopMenuBarLayout layout;        // Stores the box coordinates of elements belonging to this TopMenuBar instance.
    bool updateModeButton = true;   // Rerenders mode button in next renderGUI call if true.
    bool updateLogo = true;         // Rerenders plugin logo in next renderGUI call if true.


    public:
    distortionMode mode = upDown;   // UDShaper distortion mode.

    TopMenuBar(uint32_t inXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);

    // Opens a menu to select the plugin distortion mode if clicked on button.
    void processLeftClick(uint32_t x, uint32_t y);

    // The following four methods don't do anything.
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);

    void renderGUI(uint32_t *canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    // After being called, the full TopMenuBar GUI will be rerendered from scratch on the next renderGUI call.
    void setupForRerender();
};

// A ShapePoint is a point on a ShapeEditor that marks the transition between two curve segments.
// Every ShapePoint stores information about the curve segment to its left.
// A ShapeEditor has at least two ShapePoints:
//  1. one at (0, 0) which can not be moved or edited
//  2. one at (1, y) which can only be moved along the y-axis.
// An arbitrary amount of points may be added in between.
class ShapePoint;


// ShapeEditors are graph editors that can be used to design functions on the user interface (effect plugins utilizing
// these kind of interfaces are called waveshaper, hence the name ShapeEditor).
// The function defined is a mapping from [0, 1] to [0, 1], which is applied to the input audio.
//
// ShapeEditors are built on ShapePoints, which are points that exist on the interface of the editor. The graph is
// interpolated between neighbouring points with one of a few possible interpolation functions. The default
// interpolation is linear.
// Points can be added, removed and moved freely between the neighbouring points.
class ShapeEditor : public InteractiveGUIElement {
    protected:
    ShapePoint *currentlyDragging = nullptr;    // Pointer to the ShapePoint that is currently edited by the user.
    ShapePoint *rightClicked = nullptr;         // Pointer to the ShapePoint that has been rightclicked by the user.
    ShapePoint *deletedPoint = nullptr;         // If a point was deleted, store a pointer to it here, so that the plugin can remove all Envelope links after the input is fully processed.
    bool GUIInitialized = false;

    // Draws the curve segment connecting the given point with the previous one to canvas.
    void drawConnection(uint32_t *canvas, ShapePoint *point, double beatPosition = 0., double secondsPlayed = 0, uint32_t color = 0xFF000000, float thickness = 5);

    public:
    ShapeEditorLayout layout;   // Stores the box coordinates of GUI elements of this ShapeEditor instance.
    const int index;            // Can be used to distinguish this instance of ShapeEditor to others.
    shapeEditorDraggingMode currentDraggingMode = none; // Indicates which action should be performed when processing the mouse drag. Is set based on the position where the dragging started.
    bool highlightModulatedParameters = false; // Set to true to draw circles around ShapePoints and curve center points.

    //Pointer to the first ShapePoint in this ShapeEditor. ShapePoints are stored in a doubly linked list.
    //
    //The first point must be excluded from all methods since it can not be edited and displayed. All loops over the ShapePoints must therefore start at shapePoints->next.
    //The last point must not be moved in x-direction or deleted.
    ShapePoint *shapePoints;

    ShapeEditor(uint32_t position[4], uint32_t GUIWidth, uint32_t GUIHeight, int shapeEditorIndex);

    // Deletes all ShapePoints and frees the allocated memory.
    ~ShapeEditor();

    // Finds the closest ShapePoint or curve center point to the coordinates (x, y). Returns pointer to the
    // corresponding point if it is closer than the squareroot of minimumDistance, else nullptr. The field
    // currentDraggingMode of this ShapeEditor is set to either position or curveCenter, based on whether
    // the point or the curve center are closer.
    ShapePoint *getClosestPoint(uint32_t x, uint32_t y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);

    // Returns the pointer to the ShapePoint that was most recently deleted.
    // Returns the given pointer once and nullptr afterwards.
    ShapePoint *getDeletedPoint();

    // Initializes the dragging of a point if clicked close to a ShapePoint.
    void processLeftClick(uint32_t x, uint32_t y);

    // Can either move a ShapePoint or change the interpolation shape between two points.
    void processMouseDrag(uint32_t x, uint32_t y);

    // Stops dragging.
    void processMouseRelease(uint32_t x, uint32_t y);

    // Can either:
    //  - Delete a ShapePoint if (x, y) is close to point.
    //  - Reset the interpolation shape if (x, y) is close to a curve center point.
    //  - Add a new ShapePoint at (x, y) if none of the above is true.
    void processDoubleClick(uint32_t x, uint32_t y);

    // Can either:
    //  - Open a context menu to select an interpolation mode if (x, y) is close to a ShapePoint.
    //  - Reset the interpolation shape if (x, y) is close to a curve center point.
    void processRightClick(uint32_t x, uint32_t y);

    void renderGUI(uint32_t *canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    // Passes input to the function defined by the graph and returns the function value at this position.
    // Input is clamped to [0, 1].
    float forward(float input, double beatPosition = 0, double secondsPlayed = 0);

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3]);
};

// Modulation:
// Some parameters can be "modulated", which means they are changed by internal functions without explicit user input.
// The modulation is controlled by Envelopes, which are graph editors that can be linked to parameters of ShapeEditors.
// The modulation frequency can be changed by the user by interacting with a FrequencyPanel instance.
//
// ModulatedParameters are NOT reported to the host as modulatable or automatable parameters, they are only handled
// inside the plugin, which makes implementation a lot more straightforward, while not impacting the user experience
// too much.

// Holds a base value, min and max values and a list of *Envelope that are linked to this parameter.
// The current value at any given time is calculated by adding all modulation offsets of the linked
// Envelopes, weighted by a factor ("modulation amount") to the base value.
class ModulatedParameter;

// FrequencyPanel lets the user change the frequency with which the corresponding Envelope loops.
// There is one FrequencyPanel for each Envelope in EnvelopeManager, of which only the one corresponding to the
// active Envelope is displayed.
//
// Consists of two elements that are drawn to the GUI:
//  - A dropdown menu, that lets the user select the loop mode.
//  - A "counter", which displays a number that translates to a frequency based on the loop mode
//    (for example "4/1" = four times per beat or "3s" = every three seconds).
class FrequencyPanel : public InteractiveGUIElement {
    private:
    FrequencyPanelLayout layout;    // Stores the box coordinates of elements belonging to this FrequencyPanel instance.
    uint32_t clickedX = 0;          // x-position of last mouseclick
    uint32_t clickedY = 0;          // y-position of last mouseclick

    bool currentlyDraggingCounter = false;  // True if user is dragging on counter and value has to be updated.
    bool updateCounter = true;              // Indicates whether the counter has to be redrawn on the GUI.
    bool updateButton = true;               // Indicates whether the button should be rerendered in case of a switch between modes.

    std::map<envelopeLoopMode, double> counterValue =  {{envelopeFrequencyTempo, 0},
                                                        {envelopeFrequencySeconds, 1.}};
    
    
    double previousValue = 1.; // Stores the counter value of the currentLoopMode while the user changes the counter value. 

    
    public:
    // The current looping mode, can be based on tempo or seconds.
    envelopeLoopMode currentLoopMode = envelopeFrequencyTempo;

    FrequencyPanel(uint32_t inXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight, envelopeLoopMode initMode = envelopeFrequencyTempo, double initValue = 0.);

    // Can either:
    //  - Initialize change of the counter value if (x, y) is on counter.
    //  - Open context menu to select loop mode if (x, y) is on mode button.
    void processLeftClick(uint32_t x, uint32_t y);

    // If last left click was on counter, change counter value based on y-position of cursor.
    void processMouseDrag(uint32_t x, uint32_t y);

    // Stops updating the counter value.
    void processMouseRelease(uint32_t x, uint32_t y);

    // The following two methods don't do anything:
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);

    // Renders only the counter displaying the current counter value.
    void renderCounter(uint32_t *canvas);

    // Renders only the button displaying the currentLoopMode.
    void renderButton(uint32_t *canvas);

    void renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    // Returns a value in [0, 1] that represents the phase of an oscillation with a frequency corresponding
    // to the current loop mode and counter value. It serves as the input for an Envelopes modForward method to
    // determine the modulation offset caused by that instance.
    double getEnvelopePhase(double beatPosition, double secondsPlayed);

    // After being called, the full FrequencyPanel GUI will be rerendered from scratch on the next renderGUI call.
    void setupForRerender();

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3]);
};

// Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove
// and update modulated parameters.
class Envelope : public ShapeEditor{
    public:
    std::vector<ModulatedParameter*> modulatedParameters; // Vector of all parameters modulated by this Envelope. 
    FrequencyPanel *frequencyPanel; // Pointer to the FrequencyPanel that controls loop mode and frequency of this Envelope.
    double tempo;

    Envelope(uint32_t size[4], uint32_t GUIWidth, uint32_t GUIHeight, FrequencyPanel *inPanel, const int index);

    // Adds a modulation link to the parameter of the given point corresponding to the given mode:
    //  1. Adds a pointer to the parameter to the vector modulatedParameters of this instance.
    //  2. Adds a reference to this Envelope as a modulator to the ModulatedParameter instance.
    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode, int envelopeIdx);

    // Removes the ModulatedParameter at the given index:
    //  1. Removes the reference to this instance from the ModulatedParameter.
    //  2. Erases the pointer to the ModulatedParameter from modulatedParameters.
    void removeModulatedParameter(int index);

    // Returns the number of ModulatedParameters linked to this Envelope.
    int getNumberModulatedParameters();

    // Returns the current amount of modulation the ModulatedParameter at the given index experiences from this
    // Envelope. 
    float getModAmount(int index);

    // Sets the modulation amount of the ModulatedParameter at the given index corresponding to this Envelope
    // instance to the given amount.
    void setModulatedParameterAmount(int index, float amount);

    // Gets the current phase from the FrequencyPanel and returns the function value at this position.
    float modForward(double beatPosition = 0, double secondsPlayed = 0);

    bool saveModulationState(const clap_ostream_t *stream);
    bool loadModulationState(const clap_istream_t *stream, int version[3], ShapeEditor *shapeEditors[2]);
};

// Class in which Envelopes can be edited and linked to Parameters of the ShapeEditors.
//
// Consists of four parts: An Envelope, a selection panel left to the Envelope and a frequency panel
// and tool panel below Envelope and selection panel. Only one Envelope is displayed and editable, the
// active Envelope can be switched on the selection panel. The tool panel shows knobs for all
// ModulatedParameters that are linked to the active Envelope. These knobs control the modulation
// amount of the corresponding parameter. The frequency panel can be used to change the modulation
// speed of the active Envelope.
class EnvelopeManager : public InteractiveGUIElement {
    private:
    uint32_t clickedX; // x-position of last mouseclick
    uint32_t clickedY; // y-position of last mouseclick

    // Vector of Envelopes. MAX_NUMBER_ENVELOPES spots is reserved and no more Envelopes can be added.
    std::vector<Envelope> envelopes;

    // Vector of FrequencyPanels. Stores one FrequencyPanel for each Envelope, the panel at activeEnvelopeIndex is displayed and edited.
    std::vector<FrequencyPanel> frequencyPanels;

    // Index of the knob which is currently edited (rightclicked or moved). Set to -1 if no knob is edited.
    int selectedKnob = -1;

    // Amount the clicked knob was set to. Is only updated on mouseClick and can be used to correctly update
    // the current value while dragging the knob.
    float selectedKnobAmount;

    // Set to true if an action requires the GUI elements to rerender. Will rerender selector and 3D frame
    // around Envelope.
    bool updateGUIElements = false;

    // Set to true if any of the tools has been updated and needs to be rerendered.
    bool toolsUpdated;

    // Index of the displayed and editable Envelope.
    int activeEnvelopeIndex;

    envelopeLoopMode loopMode = envelopeFrequencyTempo;
    double loopFrequency = 1;
    
    // Sets the Envelope at index as active and updates the GUI accordingly.
    void setActiveEnvelope(int index);

    // Adds a new Envelope to the back of envelopes and a FrequencyPanel to the back of frequencyPanels.
    void addEnvelope();

    // Draws the knobs for the ModulatedParameters of the active Envelope to the tool panel. Knobs are positioned
    // next to each other sorted by their index in the modulatedParameters vector of the parent Envelope.
    void drawKnobs(uint32_t *canvas);


    public:

    // Points to a ShapePoint that was linked to the active Envelope by the user, but is waiting for the user
    // to select the X or Y direction in the menu before it can actually be added as a ModulatedParameter.
    ShapePoint *attemptedToModulate = nullptr; 

    EnvelopeManagerLayout layout; // Stores the box coordinates of elements belonging to this EnvelopeManager instance.
    envelopeManagerDraggingMode currentDraggingMode = envNone;
    
    EnvelopeManager(uint32_t XYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);

    // Draw the initial 3D frames around the displayed Envelope. Only has to be called when creating the GUI
    // or the GUI contents have been changed by an user input.
    void setupFrames(uint32_t *canvas);
    
    // Can either:
    //  - forward left click to active Envelope if (x, y) is on Envelope.
    //  - switch active Envelope and attempt to connect it to a parameter if (x, y) is on selection panel.
    //  - start tracking mouse drag to move knob position if (x, y) on knob on tool panel.
    void processLeftClick(uint32_t x, uint32_t y);

    // Fowards mouse drag to active Envelope and change knob position if last left click was on knob.
    void processMouseDrag(uint32_t x, uint32_t y);

    // Stops dragging.
    void processMouseRelease(uint32_t x, uint32_t y);

    // Forwards double click to active Envelope.
    void processDoubleClick(uint32_t x, uint32_t y);

    // Forwards right click to active Envelope and opens link knob context menu if (x, y) is close to a link knob.
    void processRightClick(uint32_t x, uint32_t y);

    void renderGUI(uint32_t	*canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    // After being called, the full EnvelopeManager GUI will be rerendered from scratch on the next renderGUI call.
    void setupForRerender();

    // Draws a circle around the parameter corresponding to a link knob close to cursor position (x, y).
    // Is used to show which parameter a knob belongs to if the cursor hovers over it.
    void highlightHoveredParameter(uint32_t x, uint32_t y);

    // Adds the given point and mode as a ModulatedParameter to the active Envelope.
    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode);

    // Removes all links to the given ShapePoint. Must be called when a ShapePoint is deleted to avoid dangling pointers.
    // The pointer is already freed when this function is called, it is only used to erase entries in the
    // modulatedParameters vector that use it.
    void clearLinksToPoint(ShapePoint *point);

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3], ShapeEditor *shapeEditors[2]);
};
