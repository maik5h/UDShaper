#include "TopMenuBar.h"


TopMenuBar::TopMenuBar(IRECT rect, float GUIWidth, float GUIHeight)
: layout(rect, GUIWidth, GUIHeight) {}

void TopMenuBar::attachUI(IGraphics* pGraphics) {
  // UI must be created after a rect has been assigned to this instance.
  assert(!layout.fullRect.Empty());

  pGraphics->AttachControl(new ITextControl(layout.logoRect, "UDShaper", IText(50)));
  pGraphics->AttachControl(new ITextControl(layout.menuTitleRect, "Distortion mode", IText(UDS_TEXT_SIZE)));
  pGraphics->AttachControl(new ICaptionControl(layout.modeMenuRect, distMode, IText(UDS_TEXT_SIZE), DEFAULT_FGCOLOR, false), kNoTag, "misccontrols");
}