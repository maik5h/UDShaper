// Collection of enums used across most project files.

#pragma once

// Types of context menus that can be displayed on the GUI.
enum contextMenuType{
    menuNone,               // Dont show a menu.
    menuShapePoint,         // Show menu corresponding to a shapePoint, in which the point can be deleted and the interpolation mode can be changed. Opens on rightclick on point.
    menuLinkKnob,           // Show menu corresponding to a linkKnob, in which the link can be removed. Opens on rightclick on a link knob.
    menuPointPosMod,        // Show menu to select either x or y-direction for point position modeulation. Opens on left buttom release after modulating a point by an Envelope.
    menuEnvelopeLoopMode,   // Show menu to select a loop mode for the current active Envelope. Opens on left click on the loop mode button of EnvelopeManager.
    menuDistortionMode,     // Show menu to select the distortion mode of the plugin. Opens on left click on the mode button in TopMenuBar.
};

// Distortion modes of the UDShaper plugin.
enum distortionMode {
	upDown,             // Splits input audio into samples larger than the previous samples and samples lower than previous.
	leftRight,          // Splits input audio into left and right channel.
  midSide,            // Splits input audio into mid and side channel.
	positiveNegative    // Splits input audio into samples >= 0 and samples < 0.
};

// Decide the actions performed when MouseDrag is processed by a Envelope based on these values.
enum envelopeManagerDraggingMode{
    envNone, // Dont do anything.
    addLink, // Trying to find a parameter the activeEnvelope can be linked to.
    moveKnob // Moving a knob and updating the modulation amount of the corresponding ModulatedParameter.
};

// Types of ShapePoint parameters that can be modulated by an Envelope.
enum modulationMode{
    modNone,            // No modulation.
    modCurveCenterY,    // Modulation affects whatever value is associated with the curveCenterPosY in the current interpolation mode.
    modPosX,            // Modulation changes the x-position of the modulated ShapePoint.
    modPosY             // Modulation changes the y-position of the modulated ShapePoint.
};

// Actions that can be performed on a link knob.
// TODO add something like set, copy and paste value.
enum menuLinkKnobOptions{
    removeLink // Remove the rightclicked link.
};

// Modes which define the base to express the Envelope frequency.
// TODO add Hz and maybe a mode where its an arbitrary integer multiple of the beats, not a power of 2.
enum envelopeLoopMode{
    envelopeFrequencyTempo,     // Envelope frequency is the multiple of a beat.
    envelopeFrequencySeconds    // Envelope frequency is set in seconds.
};
