#pragma once

#include <cstdint>

constexpr int UDSHAPER_VERSION[3] = {1, 0, 0};  // The version of this UDShaper instance.
#define UDSHAPER_VERSION_STRING "1.0.0"         // Version as an array of chars. Must be equivalent to UDSHAPER_VERSION.

constexpr float FRAME_WIDTH = 10.f;           // Width of 3D frames around ShapeEditors relative to the GUI width.
constexpr float FRAME_WIDTH_NARROW = 3.f;     // Width of narrow frames relative to the GUI width.
constexpr float FRAME_WIDTH_EDITOR = 2.f;     // Width of the frames around ShapeEditor interfaces relative to the GUI width.
constexpr float POINT_SIZE = 4.f;             // Radius of the curve center points relative to the GUI width.
constexpr float POINT_SIZE_SMALL = 3.f;       // Radius of ShapePoints relative to the GUI width.

constexpr float SHAPE_MAX_POWER = 30;         // Maximum of the power parameter of ShapePoints.
constexpr float SHAPE_MIN_OMEGA = 10E-3;      // Minimum omega of SHapePoints.

constexpr uint32_t MAX_NUMBER_LINKS = 12;     // Maximum number of links to a ModulatedParameter for a single Envelope.
constexpr float LINK_KNOB_SIZE = 25.f;        // Diameter of the link knobs below the active Envelope of the EnvelopeManager relative to the GUI width.
constexpr float KNOB_SENSITIVITY = 1./150;    // How much the value of a knob changes when the mouse moves one pixel on the screen.
constexpr float COUNTER_SENSITIVITY = 1./40;  // How much the value of the counter changer when the mouse moves one pixel on the screen.

// Square of the minimum distance between the mouse and an object in pixels in order to let them interact.
constexpr float REQUIRED_SQUARED_DISTANCE = 200;

// Time in ms until GUI is rerendered.
constexpr uint32_t GUI_REFRESH_INTERVAL = 15;

 // The maxmium number of LFOs.
constexpr int MAX_NUMBER_LFOS = 10;

// The maximum number of parameters an LFO can modulate.
constexpr int MAX_MODULATION_LINKS = 10;
