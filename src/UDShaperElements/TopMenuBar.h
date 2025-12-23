#pragma once

#include <assert.h>
#include <IControls.h>
#include "../IControl.h"
#include "../GUILayout.h"
#include "../enums.h"
#include "../UDShaperParameters.h"
#include "../controlTags.h"

// Renders the menu bar at the top of the plugin and handles all user inputs on this area.
// The menu bar will consist of: The plugin logo (TODO), a button to select the distortion
// mode and a panel to select presets (TODO).
class TopMenuBar
{
  // Stores the coordinates of elements belonging to this TopMenuBar instance.
  TopMenuBarLayout layout;

  public:
  // Create a TopMenuBar instance.
  // * @param rect The rectangle on the UI this instance will render in
  // * @param GUIWidth The width of the full UI in pixels
  // * @param GUIHeight The height of the full UI in pixels
  TopMenuBar(IRECT rect, float GUIWidth, float GUIHeight);

  // Attach the TopMenuBar UI to the given graphics context.
  //
  // This will create
  // - the UDShaper logo (TODO).
  // - the popup menu to select the distortion mode.
  void attachUI(IGraphics* pGraphics);
};
