#include "GUILayout.h"

UDShaperLayout::UDShaperLayout(float width, float height)
{ setCoordinates(width, height); }

// Sets the coordinates of all sub-elements according to the given width and height values.
void UDShaperLayout::setCoordinates(float width, float height) {
  GUIWidth = width;
  GUIHeight = height;

  fullRect.L = 0;
  fullRect.T = 0;
  fullRect.R = GUIWidth;
  fullRect.B = GUIHeight;

  // Top menu starts at upper left corner and extends over the whole width of the GUI.
  // It takes up 12.5% of the GUI height.
  topMenuRect.L = 0;
  topMenuRect.T = 0;
  topMenuRect.R = GUIWidth;
  topMenuRect.B = GUIHeight * 0.125f;

  // Two ShapeEditors are positioned below the top menu on the left side of the remaining area.
  editor1Rect.L = 2 * FRAME_WIDTH;
  editor1Rect.T = topMenuRect.B + 2 * FRAME_WIDTH;
  editor1Rect.R = GUIWidth * 0.25f - FRAME_WIDTH * 0.5f;
  editor1Rect.B = GUIHeight - 2 * FRAME_WIDTH;

  editor2Rect.L = GUIWidth * 0.25f + FRAME_WIDTH * 0.5f;
  editor2Rect.T = topMenuRect.B + 2 * FRAME_WIDTH;
  editor2Rect.R = GUIWidth * 0.5f - 2 * FRAME_WIDTH;
  editor2Rect.B = GUIHeight - 2 * FRAME_WIDTH;

  // Editor frame is drawn around the two ShapeEditors.
  editorFrameRect.L = FRAME_WIDTH;
  editorFrameRect.T = topMenuRect.B + FRAME_WIDTH;
  editorFrameRect.R = GUIWidth * 0.5f - FRAME_WIDTH;
  editorFrameRect.B = GUIHeight - FRAME_WIDTH;

  // LFOController is located below the top menu bar and right to the ShapeEditors.
  LFORect.L = GUIWidth * 0.5f + FRAME_WIDTH;
  LFORect.T = topMenuRect.B + FRAME_WIDTH;
  LFORect.R = GUIWidth - FRAME_WIDTH;
  LFORect.B = GUIHeight - FRAME_WIDTH;
}

TopMenuBarLayout::TopMenuBarLayout(IRECT rect, float GUIWidth, float GUIHeight)
{ setCoordinates(rect, GUIWidth, GUIHeight); }

// Sets the coordinates of all sub-elements according to the given IRECT.
void TopMenuBarLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;

  // The plugin logo is displayed at the upper left corner.
  logoRect.L = fullRect.L + 2 * FRAME_WIDTH;
  logoRect.T = fullRect.T;
  logoRect.R = GUIWidth * 0.25f - FRAME_WIDTH * 0.5f;
  logoRect.B = fullRect.B;

  // The mode menu is placed next to the logo and extends over half the height of the menu bar.
  modeMenuRect.L = GUIWidth * 0.25f + FRAME_WIDTH * 0.5f;
  modeMenuRect.T = fullRect.T + fullRect.H() / 2;
  modeMenuRect.R = GUIWidth * 0.5f - 2 * FRAME_WIDTH;
  modeMenuRect.B = fullRect.B;

  // The menu title is above the mode menu.
  menuTitleRect.L = modeMenuRect.L;
  menuTitleRect.T = fullRect.T;
  menuTitleRect.R = modeMenuRect.R;
  menuTitleRect.B = modeMenuRect.T;
}

ShapeEditorLayout::ShapeEditorLayout(IRECT rect, float GUIWidth, float GUIHeight)
{ setCoordinates(rect, GUIWidth, GUIHeight); }

// Sets the coordinates of all sub-elements according to the given IRECT.
void ShapeEditorLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  // Get the current GUI size. This is required to render correctly.
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;
        
  // Inner area is one RELATIVE_FRAME_WIDTH apart from outer area.
  innerRect.L = fullRect.L + FRAME_WIDTH;
  innerRect.T = fullRect.T + FRAME_WIDTH;
  innerRect.R = fullRect.R - FRAME_WIDTH;
  innerRect.B = fullRect.B - FRAME_WIDTH;

  // Editor area is one RELATIVE_FRAME_WIDTH apart from inner area.
  editorRect.L = innerRect.L + FRAME_WIDTH;
  editorRect.T = innerRect.T + FRAME_WIDTH;
  editorRect.R = innerRect.R - FRAME_WIDTH;
  editorRect.B = innerRect.B - FRAME_WIDTH;
}

LFOControlLayout::LFOControlLayout(IRECT rect, float GUIWidth, float GUIHeight)
{ setCoordinates(rect, GUIWidth, GUIHeight); }

// Sets the coordinates of all sub-elements according to the given IRECT.
void LFOControlLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;

  // Width and height of the LFOControl.
  float width = fullRect.R - fullRect.L;
  float height = fullRect.B - fullRect.T;

  // The LFO editors are positioned at the upper right corner of the LFOControl.
  // Similar to the implementation in ShapeEditorLayout, there are three concentric rects.
  // one frame width apart.
  editorFullRect.L = fullRect.L + 0.1f * width;
  editorFullRect.T = fullRect.T;
  editorFullRect.R = fullRect.R;
  editorFullRect.B = fullRect.B - 0.3f * height;

  editorRect.L = editorFullRect.L + FRAME_WIDTH;
  editorRect.T = editorFullRect.T + FRAME_WIDTH;
  editorRect.R = editorFullRect.R - FRAME_WIDTH;
  editorRect.B = editorFullRect.B - FRAME_WIDTH;

  editorInnerRect.L = editorRect.L + FRAME_WIDTH;
  editorInnerRect.T = editorRect.T + FRAME_WIDTH;
  editorInnerRect.R = editorRect.R - FRAME_WIDTH;
  editorInnerRect.B = editorRect.B - FRAME_WIDTH;

  // The selector panel is positioned directly to the left of the LFO editor, with the
  // same y-extent. editorRect is the full size of the graph editor. selectorRect has
  // to take the width of the frame into account. It is expanded by one pixel in
  // x-direction to connect to the editorRect.
  selectorRect.L = fullRect.L;
  selectorRect.T = fullRect.T;
  selectorRect.R = editorRect.L + 1;
  selectorRect.B = editorFullRect.B;

  // The knobs are positioned below the LFO editor, with the same x-extent.
  // For y-position, take width of the 3D frame around the active LFO into account.
  knobsRect.L = editorRect.L;
  knobsRect.T = fullRect.B - 0.3f * height + FRAME_WIDTH;
  knobsRect.R = fullRect.R;
  knobsRect.B = fullRect.B - 0.15f * height;

  knobsInnerRect.L = knobsRect.L + FRAME_WIDTH;
  knobsInnerRect.T = knobsRect.T + FRAME_WIDTH;
  knobsInnerRect.R = knobsRect.R - FRAME_WIDTH;
  knobsInnerRect.B = knobsRect.B - FRAME_WIDTH;

  // Tools are positioned below knobs.
  toolsRect.L = editorRect.L;
  toolsRect.T = fullRect.B - 0.15f * height + FRAME_WIDTH;
  toolsRect.R = fullRect.R;
  toolsRect.B = fullRect.B;
}

FrequencyPanelLayout::FrequencyPanelLayout(IRECT rect, float GUIWidth, float GUIHeight)
{ setCoordinates(rect, GUIWidth, GUIHeight); }

// Sets the coordinates of all sub-elements according to the given IRECT.
void FrequencyPanelLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;

  // TODO for now counter and button are just split in half. There is room for visual improvement
  counterRect.L = fullRect.L + FRAME_WIDTH_NARROW;
  counterRect.T = fullRect.T + FRAME_WIDTH_NARROW;
  counterRect.R = (fullRect.R + fullRect.L) / 2 - FRAME_WIDTH_NARROW;
  counterRect.B = fullRect.B - FRAME_WIDTH_NARROW;

  modeButtonRect.L = (fullRect.R + fullRect.L) / 2 + FRAME_WIDTH_NARROW;
  modeButtonRect.T = fullRect.T + FRAME_WIDTH_NARROW;
  modeButtonRect.R = fullRect.R - FRAME_WIDTH_NARROW;
  modeButtonRect.B = fullRect.B - FRAME_WIDTH_NARROW;
}
