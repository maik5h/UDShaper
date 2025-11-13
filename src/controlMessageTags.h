#pragma once

// Tags used for sending messages between IControls. 
enum EControlMsg
{
  // Sent by an LFOController that attempts to connect an LFO to a parameter.
  LFOAttemptConnect = 0,

  // Sent by a ShapeEditorControl as response to a successfull connection with an LFO.
  LFOConnectSuccess,

  // Sent by a LinkKnob that must be disconnected from its ModulatedParameter.
  LFODisconnect,
};
