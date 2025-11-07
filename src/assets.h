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

// Draw a frame that has each side shaded in a different color to create the impression of depth.
class Frame3D : public IControl
{
  // Prepare colors, later blend them with background color.
  IColor light1;
  IColor light2;
  IColor dark1;
  IColor dark2;

  // Frame is divided into four trapezoids, two of each share x- or y-coordinates.
  float vertY[4];
  float leftX[4];
  float rightX[4];

  float horX[4];
  float topY[4];
  float bottomY[4];

public:
  Frame3D(IRECT rect, int thickness, IColor backgroundColor)
    : IControl(rect)
  {
    vertY[0] = rect.T;
    vertY[1] = rect.T + thickness;
    vertY[2] = rect.B - thickness;
    vertY[3] = rect.B;

    leftX[0] = rect.L;
    leftX[1] = rect.L + thickness;
    leftX[2] = rect.L + thickness;
    leftX[3] = rect.L;

    rightX[0] = rect.R;
    rightX[1] = rect.R - thickness;
    rightX[2] = rect.R - thickness;
    rightX[3] = rect.R;

    horX[0] = rect.L;
    horX[1] = rect.L + thickness;
    horX[2] = rect.R - thickness;
    horX[3] = rect.R;

    topY[0] = rect.T;
    topY[1] = rect.T + thickness;
    topY[2] = rect.T + thickness;
    topY[3] = rect.T;

    bottomY[0] = rect.B;
    bottomY[1] = rect.B - thickness;
    bottomY[2] = rect.B - thickness;
    bottomY[3] = rect.B;

    light1 = IColor::LinearInterpolateBetween(backgroundColor, UDS_WHITE, ALPHA_3DFRAME_LIGHT1);
    light2 = IColor::LinearInterpolateBetween(backgroundColor, UDS_WHITE, ALPHA_3DFRAME_LIGHT2);
    dark1 = IColor::LinearInterpolateBetween(backgroundColor, UDS_BLACK, ALPHA_3DFRAME_DARK1);
    dark2 = IColor::LinearInterpolateBetween(backgroundColor, UDS_BLACK, ALPHA_3DFRAME_DARK2);
  }
  void Draw(IGraphics& g) override
  {
    g.FillConvexPolygon(light1, leftX, vertY, 4);
    g.FillConvexPolygon(light2, horX, bottomY, 4);
    g.FillConvexPolygon(dark1, rightX, vertY, 4);
    g.FillConvexPolygon(dark2, horX, topY, 4);
  }
};

// TODO:
//void drawArrow(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], bool up, float sizeFactor);

//// Changes the cursor icon to indicate dragging.
//void setCursorDragging();

//class ShapeEditorUI : public IControl;
