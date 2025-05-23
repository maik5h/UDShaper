// Structs to manage the sizes and positions of GUI elements are defined in the following.
// The struct UDShaperElementCoordinates stores box coordinates of all UDShaper GUI elements such as ShapeEditors
// and the EnvelopeManager. The setCoordinates method calculates the coordinates of all elements corresonding
// to a given GUI size. ShapeEditorCoordinates, FrequencyPanelCoordinates and TopMenuBarCoordinates work similar.
//
// Rendering of the GUI is handled in the individual GUI elements of the plugin in shapeEditor.cpp.

#pragma once

#include "../config.h"

// Stores box coordinates of the main elements of UDShaper.
// Box coordinates corresponding to a specific window width and height are calculated by setCoordinates().
struct UDShaperLayout {
    uint32_t GUIWidth;              // Width of the full UDShaper GUI.
    uint32_t GUIHeight;             // Height of the full UDShaper GUI.
    uint32_t fullXYXY[4];           // Box coordinates of the full GUI {0, 0, GUIWidth, GUIHeight}.
    uint32_t topMenuXYXY[4];        // Box coordinates of the top menu bar of the plugin.
    uint32_t editorFrameXYXY[4];    // Box coordinates of the Frame around both ShapeEditors.
    uint32_t editor1XYXY[4];        // Box coordinates of ShapeEditor 1.
    uint32_t editor2XYXY[4];        // Box coordinates of ShapeEditor 2.
    uint32_t envelopeXYXY[4];       // Box coordinates of the EnvelopeManager.

    void setCoordinates(uint32_t width, uint32_t height);
};

// Stores box coordinates of the main elements of TopMenuBar:
// - The plugin logo on the very left (logoXYXY).
// - A button to select the plugin distortion mode right of the logo (modeButtonXYXY).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct TopMenuBarLayout {
    uint32_t GUIWidth;          // Width of the full UDShaper GUI.
    uint32_t GUIHeight;         // Height of the full UDShaper GUI.
    uint32_t fullXYXY[4];       // Box coordinates of the full TopMenuBar.
    uint32_t logoXYXY[4];       // Box coordinates of the plugin logo (upper left corner).
    uint32_t modeButtonXYXY[4]; // Box coordinates of the button to select the distortion mode.

    void setCoordinates(uint32_t inXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);
};

// Stores box coordinates of elements of a ShapeEditor instance:
// - The full ShapeEditor including outer frame (fullXYXY).
// - The graph editor excluding the outer frame, but including a margin around the actual editable area (innerXYXY).
// - The inner graph editor (editorXYXY).
//
// The parts are concentric with different sizes.
// The setCoordinates method calculates coordinates of the three elements such that they fit into a given rectangle.
struct ShapeEditorLayout {
    uint32_t GUIWidth;      // Width of the full UDShaper GUI.
    uint32_t GUIHeight;     // Height of the full UDShaper GUI.
    uint32_t fullXYXY[4];   // Box coordinates of the ShapeEditor instance including the 3D frame.
    uint32_t innerXYXY[4];  // Box coordinates of the ShapeEditor instance inside the 3D frame.
    uint32_t editorXYXY[4]; // Box coordinates of the curve editor.

    void setCoordinates(uint32_t inFullXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);
};

// Stores box coordinates of elements of an EnvelopeManager instance:
// - A selector panel in the upper left corner (selectorXYXY).
// - A graph editor right of the selector panel (editorXYXY).
// - A knob bar below both previous elements (knobsXYXY).
// - A tool bar below the knob bar (toolsXYXY).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct EnvelopeManagerLayout {
    uint32_t GUIWidth;              // Width of the full UDShaper GUI.
    uint32_t GUIHeight;             // Height of the full UDShaper GUI.
    uint32_t fullXYXY[4];           // Box coordinates of the full EnvelopeManager.
    uint32_t editorXYXY[4];         // Box coordinates of the full graph editor.
    uint32_t editorInnerXYXY[4];    // Box coordinates of the graph editor without the outer 3D frame.
    uint32_t selectorXYXY[4];       // Box coordinates of the selector panel left to the editor.
    uint32_t knobsXYXY[4];          // Box coordinates of the knob panel below the editor and selector panel.
    uint32_t toolsXYXY[4];          // Box coordinates of the tool panel below the knob panel.

    void setCoordinates(uint32_t inFullXYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);
};

// Stores box coordinates of elements of a FrequencyPanel instance:
// - A counter to the left (counterXYXY).
// - A button to select the Envelope loop mode to the right (modeButtonXYXY).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct FrequencyPanelLayout {
    uint32_t GUIWidth;          // Width of the full UDShaper GUI.
    uint32_t GUIHeight;         // Height of the full UDShaper GUI.
    uint32_t fullXYXY[4];       // Box coordinates of the full FrequencyPanel instance.
    uint32_t counterXYXY[4];    // Box coordinates of the counter on the left side of this FrequencyPanel.
    uint32_t modeButtonXYXY[4]; // Box coordinates of the mode button on the right side of this FrequencyPanel.

    void setCoordinates(uint32_t XYXY[4], uint32_t GUIWidth, uint32_t GUIHeight);
};
