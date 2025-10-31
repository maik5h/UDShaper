#include "TopMenuBar.h"


void TopMenuBar::setCoordinates(IRECT rect, float GUIWidth, float GUIHeight) {
  layout.setCoordinates(rect, GUIWidth, GUIHeight);
}

void TopMenuBar::attachUI(IGraphics* pGraphics) {
  // UI must be created after a rect has been assigned to this instance.
  assert(!layout.fullRect.Empty());

  pGraphics->AttachControl(new ITextControl(layout.logoRect, "UDShaper", IText(50)));
  pGraphics->AttachControl(new ITextControl(layout.menuTitleRect, "Distortion mode", IText(25)));
  pGraphics->AttachControl(new ICaptionControl(layout.modeMenuRect, distMode, IText(24.f), DEFAULT_FGCOLOR, false), kNoTag, "misccontrols");
}