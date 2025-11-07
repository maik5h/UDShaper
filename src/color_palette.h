// The UDShaper color palette.

#pragma once

#include "../IGraphicsStructs.h"
using namespace iplug;
using namespace igraphics;

const IColor UDS_GREY(255, 95, 95, 95);
const IColor UDS_ORANGE(255, 254, 181, 90);
const IColor UDS_WHITE(255, 255, 255, 255);
const IColor UDS_BLACK(255, 0, 0, 0);

// Alpha value of the grid on the ShapeEditor background.
const float ALPHA_GRID = 0.4f;

// Alpha value of shadows.
const float ALPHA_SHADOW = 0.45f;

// Alpha values of shaded segments in 3D frames.
// Light defines how many UDS_WHITE is blended to the base color,
// dark analoguous for UDS_BLACK.
const float ALPHA_3DFRAME_LIGHT1 = 0.4;
const float ALPHA_3DFRAME_LIGHT2 = 0.52;
const float ALPHA_3DFRAME_DARK1 = 0.2;
const float ALPHA_3DFRAME_DARK2 = 0.3;
