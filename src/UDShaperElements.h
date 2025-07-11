// Subelements of the UDShaper are declared in this file.
// All subelements are InteractiveGUIElements and have a fixed position on the UDShaper GUI on which they can
// draw their GUI.
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
#include "../config.h"
#include "../color_palette.h"
#include "assets.h"
#include "string_presets.h"
#include "GUILayout.h"
#include "contextMenus.h"
#include "logging.h"
#include "enums.h"

// Renders the menu bar at the top of the plugin and handles all user inputs on this area.
// The menu bar will consist of: The plugin logo (TODO), a button to select the distortion mode and a panel to select presets (TODO).
class TopMenuBar: public InteractiveGUIElement {
    TopMenuBarLayout layout; // Stores the box coordinates of elements belonging to this TopMenuBar instance.
    bool updateModeButton = true; // Rerenders mode button in next renderGUI call if true.
    bool updateLogo = true; // Renders plugin logo in next renderGUI call if true.


    public:
    distortionMode mode = upDown; // UDShaper distortion mode.

    TopMenuBar(uint32_t inXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    void setupForRerender();
};

class ShapePoint;

// A shape editor is a region on the UI that can be used to design a curve by adding, moving and deleting points between
// which the curve is interpolated. The way the curve is interpolated in each region between two points ("curve segment")
// can be chosen from a dialog box by rightclicking the point to the right of the segment. There are the options power
// and sine, see implementation for more details. Some shapes can further be edited by dragging the center of the
// curve.
class ShapeEditor : public InteractiveGUIElement {
    protected:
    ShapePoint *currentlyDragging = nullptr; // Pointer to the ShapePoint that is currently edited by the user.
    ShapePoint *rightClicked = nullptr; // Pointer to the ShapePoint that has been rightclicked by the user.
    ShapePoint *deletedPoint = nullptr; // If a point was deleted, store a pointer to it here, so that the plugin can remove all Envelope links after the input is fully processed.
    bool GUIInitialized = false; // Indicates whether elements that do not have to be rendered every frame are drawn to the GUI.
    void drawConnection(uint32_t *canvas, ShapePoint *point, double beatPosition = 0., double secondsPlayed = 0, uint32_t color = 0xFF000000, float thickness = 5);

    public:
    ShapeEditorLayout layout; // Stores the box coordinates of elements belonging to this ShapeEditor instance.
    const int index; // Can be used to distinguish this instance of ShapeEditor to others. Is used in the serialization to associate ShapePoints with the correct instance.

    shapeEditorDraggingMode currentDraggingMode = none; // Indicates which action should be performed when processing the mouse drag. Is set based on the position where the dragging started.

    bool highlightModulatedParameters = false; // Set to true to draw circles around ShapePoints and curve center points.

    //Pointer to the first ShapePoint in this ShapeEditor. ShapePoints are stored in a doubly linked list.
    //
    //The first point must be excluded from all methods since it can not be edited and displayed. All loops over the ShapePoints must therefore start at shapePoints->next.
    //The last point must not be moved in x-direction or deleted.
    ShapePoint *shapePoints;

    ShapeEditor(uint32_t position[4], uint32_t GUIWidth, uint32_t GUIHeight, int shapeEditorIndex);
    ~ShapeEditor();

    ShapePoint *getClosestPoint(uint32_t x, uint32_t y, float minimumDistance = REQUIRED_SQUARED_DISTANCE);
    ShapePoint *getDeletedPoint();

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t *canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);

    float forward(float input, double beatPosition = 0, double secondsPlayed = 0);

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3]);
};

class ModulatedParameter;

// Envelopes modulate ShapeEditor parameters in a periodic and timedependent manner. The frequency
// can be changed by the user by adjusting a "counter" which displays a number. There are different modes
// determining how this number is interpreted:
//     -envelopeFrequencyTempo: the Envelope loops n times per beat, where n is a power of 2 or one over a power of 2.
//     -envelopeFrequencySeconds: the period of the oscillation in seconds can be set directly using the counter.
//
// The FrequencyPanels that control these properties are stored in EnvelopeManager and only referenced in Envelope.

// An InteractiveGUIElement that lets the user change the frequency with which the corresponding Envelope loops.
// There is one FrequencyPanel for each Envelope in EnvelopeManager, of which only the one corresponding to the active Envelope is displayed.
//
// Consists of two elements that are drawn to the GUI: A "counter", which displays a number that corresponds to the frequency and a dropdown menu, that lets the user select the loop mode.
class FrequencyPanel : public InteractiveGUIElement {
    private:
    FrequencyPanelLayout layout; // Stores the box coordinates of elements belonging to this FrequencyPanel instance.
    uint32_t clickedX = 0;
    uint32_t clickedY = 0;

    bool currentlyDraggingCounter = false; // True if user is dragging on counter and value has to be updated.
    bool updateCounter = true; // Indicates whether the counter has to be redrawn on the GUI. True if counter value has changed after dragging or switching currentLoopMode. Resets to false once renderGUI() is called.
    bool updateButton = true; // Indicates whether the button should be rerendered in case of a switch between modes. Resets to false once renderGUI() is called.

    std::map<envelopeLoopMode, double> counterValue =  {{envelopeFrequencyTempo, 0}, // Mapping between the loop mode and the corresponding counter value.
                                                        {envelopeFrequencySeconds, 1.}};
    double previousValue = 1.; // Is used to store the counter value of the currentLoopMode when the user changes the counter value. 

    
    public:
    envelopeLoopMode currentLoopMode = envelopeFrequencyTempo; // The current looping mode, can be based on tempo, tempo triplets or seconds.

    FrequencyPanel(uint32_t inXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight, envelopeLoopMode initMode = envelopeFrequencyTempo, double initValue = 0.);

    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderCounter(uint32_t *canvas);
    void renderButton(uint32_t *canvas);
    void renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);
    double getEnvelopePhase(double beatPosition, double secondsPlayed);
    void setupForRerender();

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3]);
};

// Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove and update controlled parameters.
class Envelope : public ShapeEditor{
    public:
    std::vector<ModulatedParameter*> modulatedParameters; // Vector of all parameters modulated by this Envelope. 
    FrequencyPanel *frequencyPanel; // Pointer to the InteractiveGUIElement FrequencyPanel, to set loop mode and frequency of this Envelope.
    double tempo; // The song tempo. Is set by UDShaper during renderAudio() and used to retreive seconds passed from beatPosition in forward.

    Envelope(uint32_t size[4], uint32_t GUIWidth, uint32_t GUIHeight, FrequencyPanel *inPanel, const int index);
    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode, int envelopeIdx);
    void setModulatedParameterAmount(int index, float amount);
    void removeModulatedParameter(int index);
    int getModulatedParameterNumber();
    float getModAmount(int index);
    float modForward(double beatPosition = 0, double secondsPlayed = 0);

    bool saveModulationState(const clap_ostream_t *stream);
    bool loadModulationState(const clap_istream_t *stream, int version[3], ShapeEditor *shapeEditors[2]);
};

// Class in which Envelopes can be edited and linked to Parameters of the ShapeEditors.
//
// Consists of four parts: An Envelope, a selection panel left to the Envelope and a frequency panel and tool panel below Envelope and selection panel.
// Only one Envelope is displayed and editable, the active Envelope can be switched on the selection panel. The tool panel shows knobs for all ModulatedParameters that are linked to the active Envelope. These knobs control the modulation amount of the corresponding parameter.
// The frequency panel can be used to change the modulation speed of the active Envelope.
class EnvelopeManager : public InteractiveGUIElement {
    private:
    uint32_t clickedX; // x-position of last mouseclick
    uint32_t clickedY; // y-position of last mouseclick

    std::vector<Envelope> envelopes; // Vector of Envelopes. To prevent reallocation and dangling pointers in ModulatedParameters, MAX_NUMBER_ENVELOPES spots is reserved and no more Envelopes can be added.
    std::vector<FrequencyPanel> frequencyPanels; // Vector of FrequencyPanels. Stores one FrequencyPanel for each Envelope, the panel at activeEnvelopeIndex is displayed and edited.
    envelopeLoopMode loopMode = envelopeFrequencyTempo;
    double loopFrequency = 1;
    
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
    EnvelopeManagerLayout layout; // Stores the box coordinates of elements belonging to this EnvelopeManager instance.

    EnvelopeManager(uint32_t XYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);
    void setupFrames(uint32_t *canvas);
    
    void processLeftClick(uint32_t x, uint32_t y);
    void processMouseDrag(uint32_t x, uint32_t y);
    void processMouseRelease(uint32_t x, uint32_t y);
    void processDoubleClick(uint32_t x, uint32_t y);
    void processRightClick(uint32_t x, uint32_t y);
    void renderGUI(uint32_t	*canvas, double beatPosition = 0, double secondsPlayed = 0);
    void rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight);
    void processMenuSelection(contextMenuType menuType, int menuItem);
    void setupForRerender();

    void highlightHoveredParameter(uint32_t x, uint32_t y);

    void addModulatedParameter(ShapePoint *point, float amount, modulationMode mode);
    void clearLinksToPoint(ShapePoint *point);

    bool saveState(const clap_ostream_t *stream);
    bool loadState(const clap_istream_t *stream, int version[3], ShapeEditor *shapeEditors[2]);
};
