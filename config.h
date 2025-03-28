#pragma once

constexpr int GUI_WIDTH = 1950; // Default GUI window width in pixel.
constexpr int GUI_HEIGHT = 700; // Sefault GUI window height in pixel.

constexpr uint32_t FRAME_WIDTH = 16; // Width of the 3D frames around ShapeEditors in pixel.

constexpr uint32_t colorBackground = 0x5F5F5F;
constexpr uint32_t colorEditorBackground = 0xFEB55A;
constexpr float alphaGrid = 0.4;
constexpr uint32_t colorCurve = 0xFFFFFF;

constexpr float SHAPE_MAX_POWER = 30; // Maximum of the power parameter of ShapePoints.
constexpr float SHAPE_MIN_OMEGA = 10E-3;

constexpr int LINK_KNOB_SPACING = 100; // Distance between link knobs below the active Envelope of the EnvelopeManager.
constexpr int LINK_KNOB_SIZE = 40; // Size of the link knobs below the active Envelope of the EnvelopeManager.
constexpr float KNOB_SENSITIVITY = 1./150; // How much the value of a knob changes when the mouse moves one pixel on the screen.
constexpr float COUNTER_SENSITIVITY = 1./40; // How much the value of the counter changer when the mouse moves one pixel on the screen.

// Square of the minimum distance between the mouse and an object in pixels, in order to let them interact.
constexpr float REQUIRED_SQUARED_DISTANCE = 200;

// Time in ms until GUI is rerendered.
constexpr uint32_t GUI_REFRESH_INTERVAL = 15;

constexpr int MAX_NUMBER_ENVELOPES = 10; // The maxmium number of Envelopes. Can not be dynamic or else ModulatedParameter.modulatingEnvelopes pointers would dangle after reallocation.
