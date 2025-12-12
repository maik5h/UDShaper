#pragma once

// Tags used for sending messages between IControls. 
enum EControlMsg
{
  // Sent by an LFOController after the user starts dragging to connect it with a
  // ShapeEditor parameter.
  LFOConnectInit = 0,

  // Sent by an LFOController after releasing a dragging event meant to connect
  // it to a ShapeEditor parameter.
  LFOConnectAttempt,

  // Sent by a ShapeEditorControl as response to a successfull connection with an LFO.
  LFOConnectSuccess,

  // Sent by a LinkKnob that must be disconnected from its ModulatedParameter.
  LFODisconnect,

  // Sent by a ShapeEditor after a point has been deleted.
  editorPointDeleted,

  // Sent by a a LinkKnob on mouse over. ShapeEditors should highlight the point
  // connected to this knob as a response.
  linkKnobMouseOver,

  // Sent by a a LinkKnob on mouse out. ShapeEditors stop highlighting.
  linkKnobMouseOut,
};
