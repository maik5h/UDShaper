#include "GUILayout.h"

// Sets the coordinates of all sub-elements according to the given width and height values.
void UDShaperLayout::setCoordinates(float width, float height) {
  GUIWidth = width;
  GUIHeight = height;

  // Free space around edges of GUI.
  float margin = RELATIVE_FRAME_WIDTH * GUIWidth;

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
  editor1Rect.L = 2 * margin;
  editor1Rect.T = topMenuRect.B + 2 * margin;
  editor1Rect.R = GUIWidth * 0.25f - margin * 0.5f;
  editor1Rect.B = GUIHeight - 2 * margin;

  editor2Rect.L = GUIWidth * 0.25f + margin * 0.5f;
  editor2Rect.T = topMenuRect.B + 2 * margin;
  editor2Rect.R = GUIWidth * 0.5f - 2 * margin;
  editor2Rect.B = GUIHeight - 2 * margin;

  // Editor frame is drawn around the two ShapeEditors.
  editorFrameRect.L = margin;
  editorFrameRect.T = topMenuRect.B + margin;
  editorFrameRect.R = GUIWidth * 0.5f - margin;
  editorFrameRect.B = GUIHeight - margin;

  // EnvelopeManager is located below the top menu bar and right to the ShapeEditors.
  envelopeRect.L = GUIWidth * 0.5f + margin;
  envelopeRect.T = topMenuRect.B + margin;
  envelopeRect.R = GUIWidth - margin;
  envelopeRect.B = GUIHeight - margin;
}

// Sets the coordinates of all sub-elements according to the given IRECT.
void TopMenuBarLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  // Free space around edges of GUI.
  float margin = RELATIVE_FRAME_WIDTH * GUIWidth;

  fullRect = rect;

  // The plugin logo is displayed at the upper left corner.
  logoRect.L = fullRect.L + 2 * margin;
  logoRect.T = fullRect.T;
  logoRect.R = GUIWidth * 0.25f - margin * 0.5f;
  logoRect.B = fullRect.B;

  // The mode menu is placed next to the logo and extends over half the height of the menu bar.
  modeMenuRect.L = GUIWidth * 0.25f + margin * 0.5f;
  modeMenuRect.T = fullRect.T + fullRect.H() / 2;
  modeMenuRect.R = GUIWidth * 0.5f - 2 * margin;
  modeMenuRect.B = fullRect.B;

  // The menu title is above the mode menu.
  menuTitleRect.L = modeMenuRect.L;
  menuTitleRect.T = fullRect.T;
  menuTitleRect.R = modeMenuRect.R;
  menuTitleRect.B = modeMenuRect.T;
}

// Sets the coordinates of all sub-elements according to the given IRECT.
void ShapeEditorLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  // Get the current GUI size. This is required to render correctly.
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;
        
  // Inner area is one RELATIVE_FRAME_WIDTH apart from outer area.
  innerRect.L = fullRect.L + RELATIVE_FRAME_WIDTH * GUIWidth;
  innerRect.T = fullRect.T + RELATIVE_FRAME_WIDTH * GUIWidth;
  innerRect.R = fullRect.R - RELATIVE_FRAME_WIDTH * GUIWidth;
  innerRect.B = fullRect.B - RELATIVE_FRAME_WIDTH * GUIWidth;

  // Editor area is one RELATIVE_FRAME_WIDTH apart from inner area.
  editorRect.L = innerRect.L + RELATIVE_FRAME_WIDTH * GUIWidth;
  editorRect.T = innerRect.T + RELATIVE_FRAME_WIDTH * GUIWidth;
  editorRect.R = innerRect.R - RELATIVE_FRAME_WIDTH * GUIWidth;
  editorRect.B = innerRect.B - RELATIVE_FRAME_WIDTH * GUIWidth;
}

// Sets the coordinates of all sub-elements according to the given IRECT.
void EnvelopeManagerLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;

  // Width and height of the EnvelopeManager. 
  float width = fullRect.R - fullRect.L;
  float height = fullRect.B - fullRect.T;

  // The Envelope is positioned at the upper right corner of the EnvelopeManager.
  editorRect.L = fullRect.L + 0.1f * width;
  editorRect.T = fullRect.T;
  editorRect.R = fullRect.R;
  editorRect.B = fullRect.B - 0.3f * height;

  editorInnerRect.L = editorRect.L + RELATIVE_FRAME_WIDTH * GUIWidth;
  editorInnerRect.T = editorRect.T + RELATIVE_FRAME_WIDTH * GUIWidth;
  editorInnerRect.R = editorRect.R - RELATIVE_FRAME_WIDTH * GUIWidth;
  editorInnerRect.B = editorRect.B - RELATIVE_FRAME_WIDTH * GUIWidth;

  // The selector panel is positioned directly to the left of Envelope, with the same y-extent as the Envelope.
  // editorXRect is the full size of the graph editor. selectorRect has to take the width of the frame into account.
  // It is expanded by one pixel in x-direction to connect to the editorRect.
  selectorRect.L = fullRect.L;
  selectorRect.T = fullRect.T;
  selectorRect.R = editorRect.L + 1;
  selectorRect.B = editorRect.B;

  // The knobs are positioned below the Envelope, with the same x-extent as the Envelope.
  // For y-position, take width of the 3D frame around the active Envelope into account.
  knobsRect.L = editorRect.L;
  knobsRect.T = fullRect.B - 0.3f * height + RELATIVE_FRAME_WIDTH * GUIWidth;
  knobsRect.R = fullRect.R;
  knobsRect.B = fullRect.B - 0.15f * height;

  knobsInnerRect.L = knobsRect.L + RELATIVE_FRAME_WIDTH * GUIWidth;
  knobsInnerRect.T = knobsRect.T + RELATIVE_FRAME_WIDTH * GUIWidth;
  knobsInnerRect.R = knobsRect.R - RELATIVE_FRAME_WIDTH * GUIWidth;
  knobsInnerRect.B = knobsRect.B - RELATIVE_FRAME_WIDTH * GUIWidth;

  // Tools are positioned below knobs.
  toolsRect.L = editorRect.L;
  toolsRect.T = fullRect.B - 0.15f * height + RELATIVE_FRAME_WIDTH * GUIWidth;
  toolsRect.R = fullRect.R;
  toolsRect.B = fullRect.B;
}

// Sets the coordinates of all sub-elements according to the given IRECT.
void FrequencyPanelLayout::setCoordinates(IRECT rect, float inGUIWidth, float inGUIHeight)
{
  GUIWidth = inGUIWidth;
  GUIHeight = inGUIHeight;

  fullRect = rect;

  float frame_width = RELATIVE_FRAME_WIDTH_NARROW * GUIWidth;

  // TODO for now counter and button are just split in half. There is room for visual improvement
  counterRect.L = fullRect.L + frame_width;
  counterRect.T = fullRect.T + frame_width;
  counterRect.R = (fullRect.R + fullRect.L) / 2 - frame_width;
  counterRect.B = fullRect.B - frame_width;

  modeButtonRect.L = (fullRect.R + fullRect.L) / 2 + frame_width;
  modeButtonRect.T = fullRect.T + frame_width;
  modeButtonRect.R = fullRect.R - frame_width;
  modeButtonRect.B = fullRect.B - frame_width;
}
