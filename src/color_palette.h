// The following values define the color scheme of the UDShaper GUI.
// Base colors are given as 32-bit unsigned integers in 0xAARRGGBB notation.

#pragma once

#include <cstdint>

constexpr uint32_t colorBackground = 0xFF5F5F5F;          // Background color of the plugin.
constexpr uint32_t colorEditorBackground = 0xFFFEB55A;    // Background color of the ShapeEditors.
constexpr uint32_t colorCurve = 0xFFFFFFFF;               // Color of the curves and frame on ShapeEditors.

constexpr float alphaGrid = 0.4;        // Alpha value of the grid on the ShapeEditor background. Grid has color colorCurve.
constexpr float alphaShadow = 0.45;     // Percantage of black (0xFF000000) being mixed to the base color to create a darker variation.
