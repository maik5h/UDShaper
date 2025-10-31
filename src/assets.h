// Helpful functions to draw different elements to the UI.


#pragma once

#include <cstdint>
#include <string>
#include <cmath>
#include "config.h"
#include "enums.h"
#include "../IControl.h"
#include "../IGraphics.h"
using namespace iplug;
using namespace igraphics;


// Fills the given rectangle with color.
class FillRectangle : public IControl
{
  const IColor col;
  public:
    FillRectangle(const IRECT& rect, const IColor color, float alpha = 1.)
    : IControl(rect), col(color.A, color.R, color.G, color.B) {}

    void Draw(IGraphics& g) override
    {
      g.FillRect(col, mRECT);
    }
};

// Draws a simple frame in rect. The outer edge of the frame aligns with rect.
class DrawFrame : public IControl
{
  const IColor col;
  float width;
  IBlend blend;

  public:
    DrawFrame(const IRECT& rect, const IColor color, float lineWidth = 1., float alpha = 1.)
    : IControl(rect), col(color.A, color.R, color.G, color.B)
    {
      // For some reason the rectangles drawn by iPlug2 appear to have only half the given width.
      width = lineWidth;
      blend = IBlend(EBlend::Default, alpha);
    }

    void Draw(IGraphics& g) override
    {
      g.DrawRect(col, mRECT, &blend, width);
    }
};

// TODO:
//// Draw a frame that has each side shaded in a different color to create the impression of depth.
//void draw3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerRectangle[4], uint32_t baseColor, int thickness = 15);

//// Draws a 3D frame similar to the function draw3DFrame, but the segment on the right is missing and connects to
//// another 3D frame positioned to the right.
//void drawPartial3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerRectangle[4], uint32_t baseColor, uint32_t thickness = 15);

//void drawArrow(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], bool up, float sizeFactor);

//// Changes the cursor icon to indicate dragging.
//void setCursorDragging();

//class ShapeEditorUI : public IControl;
