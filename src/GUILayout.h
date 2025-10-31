// Structs to manage the sizes and positions of GUI elements are defined in the following.
// The struct UDShaperLayout stores rectangles corresponding to all UDShaper GUI elements such as ShapeEditors
// and the EnvelopeManager. The setCoordinates method calculates the coordinates of all elements corresonding
// to a given GUI size. ShapeEditorLayout, FrequencyPanelLayout and TopMenuBarLayout work similar.

#pragma once

#include "config.h"
#include "../IGraphicsStructs.h"
using namespace iplug;
using namespace igraphics;

// Stores box coordinates of the main elements of UDShaper.
// Box coordinates corresponding to a specific window width and height are calculated by setCoordinates().
struct UDShaperLayout {
  float GUIWidth = 0;               // Width of the full UDShaper GUI.
  float GUIHeight = 0;              // Height of the full UDShaper GUI.
  IRECT fullRect = IRECT();         // Box coordinates of the full GUI {0, 0, GUIWidth, GUIHeight}.
  IRECT topMenuRect = IRECT();      // Box coordinates of the top menu bar of the plugin.
  IRECT editorFrameRect = IRECT();  // Box coordinates of the Frame around both ShapeEditors.
  IRECT editor1Rect = IRECT();      // Box coordinates of ShapeEditor 1.
  IRECT editor2Rect = IRECT();      // Box coordinates of ShapeEditor 2.
  IRECT envelopeRect = IRECT();     // Box coordinates of the EnvelopeManager.

  void setCoordinates(float width, float height);
};

// Stores box coordinates of the main elements of TopMenuBar:
// - The plugin logo on the very left (logoRect).
// - A button to select the plugin distortion mode right of the logo (modeButtonRect).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct TopMenuBarLayout {
  float GUIWidth;                 // Width of the full UDShaper GUI.
  float GUIHeight;                // Height of the full UDShaper GUI.
  IRECT fullRect = IRECT();       // Box coordinates of the full TopMenuBar.
  IRECT logoRect = IRECT();       // Box coordinates of the plugin logo (upper left corner).
  IRECT modeMenuRect = IRECT();   // Box coordinates of the menu to select the distortion mode.
  IRECT menuTitleRect = IRECT();  // Box coordinates of the menu title text.

  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);
};

// Stores box coordinates of elements of a ShapeEditor instance:
// - The full ShapeEditor including outer frame (fullRect).
// - The graph editor excluding the outer frame, but including a margin around the actual editable area (innerRect).
// - The inner graph editor (editorRect).
//
// The parts are concentric with different sizes.
// The setCoordinates method calculates coordinates of the three elements such that they fit into a given rectangle.
struct ShapeEditorLayout {
  float GUIWidth;             // Width of the full UDShaper GUI.
  float GUIHeight;            // Height of the full UDShaper GUI.
  IRECT fullRect = IRECT();   // Box coordinates of the ShapeEditor instance including the 3D frame.
  IRECT innerRect = IRECT();  // Box coordinates of the ShapeEditor instance inside the 3D frame.
  IRECT editorRect = IRECT(); // Box coordinates of the curve editor.

  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);
};

// Stores box coordinates of elements of an EnvelopeManager instance:
// - A selector panel in the upper left corner (selectorRect).
// - A graph editor right of the selector panel (editorRect).
// - A knob bar below both previous elements (knobsRect).
// - A tool bar below the knob bar (toolsRect).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct EnvelopeManagerLayout {
  float GUIWidth;                   // Width of the full UDShaper GUI.
  float GUIHeight;                  // Height of the full UDShaper GUI.
  IRECT fullRect = IRECT();         // Box coordinates of the full EnvelopeManager.
  IRECT editorRect = IRECT();       // Box coordinates of the full graph editor.
  IRECT editorInnerRect = IRECT();  // Box coordinates of the graph editor without the outer 3D frame.
  IRECT selectorRect = IRECT();     // Box coordinates of the selector panel left to the editor.
  IRECT knobsRect = IRECT();        // Box coordinates of the full knob panel below the editor and selector panel.
  IRECT knobsInnerRect = IRECT();   // Box coordinates of the knob panel without the outer 3D frame.
  IRECT toolsRect = IRECT();        // Box coordinates of the tool panel below the knob panel.

  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);
};

// Stores box coordinates of elements of a FrequencyPanel instance:
// - A counter to the left (counterRect).
// - A button to select the Envelope loop mode to the right (modeButtonRect).
//
// The setCoordinates method calculates coordinates of the elements such that they fit into a given rectangle.
struct FrequencyPanelLayout {
  float GUIWidth;                 // Width of the full UDShaper GUI.
  float GUIHeight;                // Height of the full UDShaper GUI.
  IRECT fullRect = IRECT();       // Box coordinates of the full FrequencyPanel instance.
  IRECT counterRect = IRECT();    // Box coordinates of the counter on the left side of this FrequencyPanel.
  IRECT modeButtonRect = IRECT(); // Box coordinates of the mode button on the right side of this FrequencyPanel.

  void setCoordinates(IRECT rect, float GUIWidth, float GUIHeight);
};
