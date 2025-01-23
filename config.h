#pragma once

constexpr int GUI_WIDTH = 1800;
constexpr int GUI_HEIGHT = 700;

constexpr uint32_t colorBackground = 0xFFCF7F;
constexpr uint32_t colorThinLines = 0xC0C0C0;
constexpr uint32_t colorCurve = 0xFFFFFF;

constexpr float SHAPE_MAX_POWER = 30;
constexpr float SHAPE_MIN_OMEGA = 10E-3;

// Time in ms until GUI is rerendered
constexpr int GUI_REFRESH_INTERVAL = 15;
