#pragma once

#include <cstdint>

constexpr int UDSHAPER_VERSION[3] = {1, 0, 0}; // The version of this UDShaper instance.
#define UDSHAPER_VERSION_STRING "1.0.0" // Version as an array of chars. Must be equivalent to UDSHAPER_VERSION.

// TODO the default values are good for my screen. On other systems it is probably too large.
constexpr uint32_t GUI_WIDTH_INIT = 1600;   // Default GUI window width in pixel.
constexpr uint32_t GUI_HEIGHT_INIT = 800;   // Sefault GUI window height in pixel.
constexpr uint32_t GUI_WIDTH_MIN = 320;     // Minimum GUI width in pixel.
constexpr uint32_t GUI_HEIGHT_MIN = 160;    // Minimum GUI height in pixel.
constexpr uint32_t GUI_WIDTH_MAX = 2400;    // Maximum GUI width in pixel.
constexpr uint32_t GUI_HEIGHT_MAX = 1200;   // Maximum GUI height in pixel.

constexpr float RELATIVE_FRAME_WIDTH = 16. / 1600;      // Width of 3D frames around ShapeEditors relative to the GUI width.
constexpr float RELATIVE_POINT_SIZE_SMALL = 12. / 1600; // Radius of the curve center points relative to the GUI width.
constexpr float RELATIVE_POINT_SIZE = 16. / 1600;       // Radius of ShapePoints relative to the GUI width.


constexpr uint32_t colorBackground = 0x5F5F5F;
constexpr uint32_t colorEditorBackground = 0xFEB55A;
constexpr float alphaGrid = 0.4;
constexpr uint32_t colorCurve = 0xFFFFFF;

constexpr float SHAPE_MAX_POWER = 30;       // Maximum of the power parameter of ShapePoints.
constexpr float SHAPE_MIN_OMEGA = 10E-3;    // Minimum omega of SHapePoints.

constexpr uint32_t MAX_NUMBER_LINKS = 12;               // Maximum number of links to a ModulatedParameter for a single Envelope.
constexpr float RELATIVE_LINK_KNOB_SIZE = 40. / 1600;       // Diameter of the link knobs below the active Envelope of the EnvelopeManager relative to the GUI width..
constexpr float KNOB_SENSITIVITY = 1./150;                  // How much the value of a knob changes when the mouse moves one pixel on the screen.
constexpr float COUNTER_SENSITIVITY = 1./40;                // How much the value of the counter changer when the mouse moves one pixel on the screen.

// Square of the minimum distance between the mouse and an object in pixels, in order to let them interact.
constexpr float REQUIRED_SQUARED_DISTANCE = 200;

// Time in ms until GUI is rerendered.
constexpr uint32_t GUI_REFRESH_INTERVAL = 15;

constexpr int MAX_NUMBER_ENVELOPES = 10; // The maxmium number of Envelopes. Can not be dynamic or else ModulatedParameter.modulatingEnvelopes pointers would dangle after reallocation.
