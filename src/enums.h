// Collection of enums used across project files.

#pragma once

// Distortion modes of the UDShaper plugin.
enum distortionMode
{
  // Splits input audio into samples larger than the previous samples and samples lower than previous.
  upDown,

  // Splits input audio into left and right channel.
  leftRight,

  // Splits input audio into mid and side channel.
  midSide,

  // Splits input audio into samples >= 0 and samples < 0.
  positiveNegative
};

 // Types of ShapePoint parameters that can be modulated by an Envelope.
enum modulationMode{
  // No modulation.
    modNone,

    // Modulation affects whatever value is associated with the curveCenterPosY in the current interpolation mode.
    modCurveCenterY,

    // Modulation changes the x-position of the modulated ShapePoint.
    modPosX,

    // Modulation changes the y-position of the modulated ShapePoint.
    modPosY
};
