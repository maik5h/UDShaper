#include "UDShaper.h"

// Returns the current time since Unix epoch in milliseconds.
long getCurrentTime(){
	auto time = std::chrono::system_clock::now();
	auto since_epoch = time.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

/*
Audio and GUI rendering:

There are ModulatedParameters that can be modulated by any of the Envelopes. The Envelopes change the value of ModulatedParameters depending
on the song position. There is a difference between audio and visual rendering, since visual rendering must happen on the main thread, while
audio rendering runs parallel on audio threads.
The exact song position in beats and seconds is available only on the audio threads (through clap_event_transport_t), so the time the host
started playing and the inital position in beats are reported to the main thread using a mutex (see reportPlaybackStatusToMain() in main.cpp).
UDShaper recalculates the correct song position from these values every time renderGUI() is called. This song position in beats and seconds
is forwarded to the renderGUI() methods of all other InteractiveGUIElements.
*/

// Initializes the two ShapeEditors and the EnvelopeManager at positions and with sizes according to the input window size.
UDShaper::UDShaper() {
    // Calculate Coordinates of UDShaper elements based on the given window parameters.
    layout.setCoordinates(GUI_WIDTH_INIT, GUI_HEIGHT_INIT);

    topMenuBar = new TopMenuBar(layout.topMenuXYXY, layout.GUIWidth, layout.GUIHeight);
    shapeEditor1 = new ShapeEditor(layout.editor1XYXY, layout.GUIWidth, layout.GUIHeight, 0);
    shapeEditor2 = new ShapeEditor(layout.editor2XYXY, layout.GUIWidth, layout.GUIHeight, 1);
    envelopes = new EnvelopeManager(layout.envelopeXYXY, layout.GUIWidth, layout.GUIHeight);
}

UDShaper::~UDShaper() {
    delete topMenuBar;
    delete shapeEditor1;
    delete shapeEditor2;
    delete envelopes;
}

// Forwards left click to all InteractiveGUIElement members and starts to track the mouse drag.
void UDShaper::processLeftClick(uint32_t x, uint32_t y) {
    topMenuBar->processLeftClick(x, y);
    shapeEditor1->processLeftClick(x, y);
    shapeEditor2->processLeftClick(x, y);
    envelopes->processLeftClick(x, y);

    mouseDragging = true;
}

// Forwards mouse drag to all InteractiveGUIElement members.
void UDShaper::processMouseDrag(uint32_t x, uint32_t y) {
    if (mouseDragging) {
        shapeEditor1->processMouseDrag(x, y);
        shapeEditor2->processMouseDrag(x, y);

        envelopes->processMouseDrag(x, y);

        // If the user is trying to add a modulation link, highlight the positions on the GUI where links
        // can be connected.
        // Setting highlightModulatedParameters to true will highlight all possible parameters.
        // Additionally change the cursor to indicate that the Envelope can be linked to the parameters by dragging.
        if (envelopes->currentDraggingMode == addLink) {
            shapeEditor1->highlightModulatedParameters = true;
            shapeEditor2-> highlightModulatedParameters = true;
            setCursorDragging();
        }
    }
    else {
        // If the cursor hovers over a link knob, highlight the point corresponding to this knob.
        // The highlight state is not updated while dragging in order to
        // a) not highlight while dragging another element, which might be confusing
        // b) more importantly, keep highlighted when turning a knob and moving outside the knob range.
        envelopes->highlightHoveredParameter(x, y);
    }
}

// Forwards mouse release to all InteractiveGUIElement members and keeps track if any of them requests a context menu to open. Stops tracking the mouse drag.
void UDShaper::processMouseRelease(uint32_t x, uint32_t y) {
    if (mouseDragging) {
        // If it was attempted to link the active Envelope to a modulatable parameter, check if a parameter
        // was selected successfully and add a ModulatedParameter instance to the corresponding Envelope.
        if (envelopes->currentDraggingMode == addLink){
            // Get the closest points for both ShapeEditors to the position the mouse was dragged to
            ShapePoint *closestPoint1, *closestPoint2;

            closestPoint1 = shapeEditor1->getClosestPoint(x, y);
            closestPoint2 = shapeEditor2->getClosestPoint(x, y);

            // check if a close point was found
            if ((closestPoint1 != nullptr) | (closestPoint2 != nullptr)){

                ShapeEditor *targetEditor;
                ShapePoint *closest;

                if (!(closestPoint1 == nullptr)) {
                    closest = closestPoint1;
                    targetEditor = shapeEditor1;
                }
                else {
                    closest = closestPoint2;
                    targetEditor = shapeEditor2;
                }

                // Find the correct parameter to modulate. There are the options curveCenter, posX and posY.
                if (targetEditor->currentDraggingMode == curveCenter) {
                    envelopes->addModulatedParameter(closest, 1, modCurveCenterY);
                }
                // If the point position should be modulated, show context menu to select correct direction.
                else {
                    MenuRequest::sendRequest(envelopes, menuPointPosMod);
                }

                // Add a ModulatedParameter to the active Envelope.
                envelopes->attemptedToModulate = closest;
            }
        }

        envelopes->processMouseRelease(x, y);

        shapeEditor1->processMouseRelease(x, y);
        shapeEditor2->processMouseRelease(x, y);

        mouseDragging = false;
    }
}

// Forwards double click to all InteractiveGUImembers. If a ShapePoint was deleted by the double click, all Envelope links to this point are cleared.
void UDShaper::processDoubleClick(uint32_t x, uint32_t y) {
    // If a ShapePoint has been deleted by the double click, retrieve the pointer to this ShapePoint.
    shapeEditor1->processDoubleClick(x, y);
    shapeEditor2->processDoubleClick(x, y);
    ShapePoint *deletedPoint1 = shapeEditor1->getDeletedPoint();
    ShapePoint *deletedPoint2 = shapeEditor2->getDeletedPoint();

    // Clear all modulation links to the deleted point to avoid dangling pointers.
    envelopes->clearLinksToPoint(deletedPoint1);
    envelopes->clearLinksToPoint(deletedPoint2);

    envelopes->processDoubleClick(x, y);
}

// Forwards right click to all InteractiveGUIElement members and keeps track if any of them requests a context menu to open.
void UDShaper::processRightClick(uint32_t x, uint32_t y) {
    shapeEditor1->processRightClick(x, y);
    shapeEditor2->processRightClick(x, y);
    envelopes->processRightClick(x, y);
}

// Renders the GUI of all InteractiveGUIElement members. beatPosition is used to sync animated graphics to the host playback position.
void UDShaper::renderGUI(uint32_t *canvas) {
    // clap_plugin_gui_t->create() is not automatically called when the plugin is created, it might be postponed to
    // when the plugin window is first opened. This guard takes care that the GUI is only rendered after it has
    // been created.
    if (gui == nullptr) return;

    if (!GUIInitialized) {
        // Draw GUI elements that do not change over time. They will not be rerendered every frame.
        // Start with filling the background.
        fillRectangle(canvas, layout.GUIWidth, layout.fullXYXY, colorBackground);

        // Set up EnvelopManager frame.
        envelopes->setupFrames(canvas);
        // Draw darker rectangle onto the EnvelopeManager knob panel.
        fillRectangle(canvas, layout.GUIWidth, envelopes->layout.knobsInnerXYXY, blendColor(colorBackground, 0xFF000000, 0.1));

        // Draw frame around ShapeEditors.
        drawFrame(canvas, layout.GUIWidth, layout.editorFrameXYXY, RELATIVE_FRAME_WIDTH_NARROW * layout.GUIWidth, 0xFF000000, alphaShadow);

        GUIInitialized = true;
    }

    // The beatposition is, unlike on the audio threads, not taken from a clap_transport, but calculated for the point in time when this function is called.
    long now = getCurrentTime();
    MutexAcquire(synchProcessStartTime);

    // If the host is playing, add the beats that have past since it started playing to the initial beatPosition.
    // The initBeatPosition and startedPlaying are set from the audio threads, where clap_transport is available.
    double beatPosition = hostPlaying ? (initBeatPosition + (now - startedPlaying) / 1000 / 60 * currentTempo) : initBeatPosition;
    // TODO init secondsPlayed has to be calculated from initBeatPosition
    double secondsPlayed = hostPlaying ? (now - startedPlaying) / 1000. : 0;
    MutexRelease(synchProcessStartTime);

    topMenuBar->renderGUI(canvas);
    shapeEditor1->renderGUI(canvas, beatPosition, secondsPlayed);
    shapeEditor2->renderGUI(canvas, beatPosition, secondsPlayed);
    envelopes->renderGUI(canvas);
}

void UDShaper::rescaleGUI(uint32_t width, uint32_t height) {
    // Update the UDShaper layout.
    layout.setCoordinates(width, height);

    // Firstly reset the whole GUI so no remainders of stretched elements are visible.
    GUIInitialized = false;

    // Secondly, update layouts of all elements accoring to new UDShaper layout and rerender their contents.
    topMenuBar->rescaleGUI(layout.topMenuXYXY, width, height);
    shapeEditor1->rescaleGUI(layout.editor1XYXY, width, height);
    shapeEditor2->rescaleGUI(layout.editor2XYXY, width, height);
    envelopes->rescaleGUI(layout.envelopeXYXY, width, height);
}

// Takes the input audio from the input process and writes the output audio into process->audio_outputs.
//
// The output is mainly dependent on two things: The state of both ShapeEditors (which depends on the host playback position if linked to an Envellope) and the distortion mode:
//  - upDown:             shapeEditor1 is used on all samples that are higher than the previous sample, shapeEditor 2 on samples that are lower than the previous one.
//  - leftRight:          shapeEditor1 is used on the left channel, shapeEditor2 on the right channel.
//  - midSide:            shapeEditor1 is used on the mid channel, shapeEditor2 on the side channel.
//  - positiveNegative:   shapeEditor1 is used on samples > 0, shapeEditor2 on samples < 0. samples = 0 stay 0.
void UDShaper::renderAudio(const clap_process_t *process) {
    assert(process->audio_outputs_count == 1);
    assert(process->audio_inputs_count == 1);

    const uint32_t start = 0;
    const uint32_t end = process->frames_count; // what is frame count, is it highest sample with event?
    const uint32_t inputEventCount = process->in_events->size(process->in_events);

    const clap_event_transport_t *transport = process->transport;
    clap_transport_flags isNotPlaying = CLAP_TRANSPORT_IS_PLAYING;
    double beatPosition = (double) transport->song_pos_beats / CLAP_BEATTIME_FACTOR;
    double secondsPlayed = (double) transport->song_pos_seconds / CLAP_SECTIME_FACTOR;

    // tempo is needed to calculate the exact beat position for every sample of the buffer. However, if the host is not playing, a tempo of zero is passed so all parameters stay constant.
    double tempo = transport->flags[&isNotPlaying] ? transport->tempo : 0;

    float *inputL = process->audio_inputs[0].data32[0];
    float *inputR = process->audio_inputs[0].data32[1];
    float *outputL = process->audio_outputs[0].data32[0];
    float *outputR = process->audio_outputs[0].data32[1];

    switch (topMenuBar->mode) {
        /* TODO Buffer is parallelized into pieces of ~200 samples. I dont know how to access all of them in
        one place so i can not properly decide which shape to choose.
        */
        case upDown:
        {
            // since data from last buffer is not available, take first samples as a reference, works most of the time but is not optimal.
            bool processShape1L = (inputL[1] >= inputL[0]);
            bool processShape1R = (inputR[1] >= inputR[0]);

            float previousLevelL;
            float previousLevelR;

            for (uint32_t index = start; index < end; index++) {
                // for modulation faster than buffer rate, modulate parameters with updated beatPosition for every sample
                // tempo is beats per minute, so tempo / 60 is beats per second. samplerate is samples per second, so tempo/60 * samplerate is beats per sample.
                beatPosition += tempo / samplerate / 60;
                secondsPlayed += 1. / samplerate;

                // update active shape only if value has changed so for plateaus the current shape stays active
                if (index > 0) {
                    // if (inputL[index] != previousLevelL) {
                        processShape1L = (inputL[index] >= previousLevelL);
                    // }
                    // if (inputR[index] != previousLevelR) {
                        processShape1R = (inputR[index] >= previousLevelR);
                    // }
                }

                outputL[index] = processShape1L ? shapeEditor1->forward(inputL[index], beatPosition, secondsPlayed) : shapeEditor2->forward(inputL[index], beatPosition, secondsPlayed);
                outputR[index] = processShape1R ? shapeEditor1->forward(inputR[index], beatPosition, secondsPlayed) : shapeEditor2->forward(inputR[index], beatPosition, secondsPlayed);

                previousLevelL = inputL[index];
                previousLevelR = inputR[index];
            }		
            break;
        }
        case midSide:
        {
            float mid;
            float side;

            for (uint32_t index = start; index < end; index++) {
                mid = shapeEditor1->forward((inputL[index] + inputR[index])/2, beatPosition);
                side = shapeEditor2->forward((inputL[index] - inputR[index])/2, beatPosition);

                outputL[index] = mid + side;
                outputR[index] = mid - side;
            }
            break;
        }
        case leftRight:
        {
            for (uint32_t index = start; index < end; index++) {
                outputL[index] = shapeEditor1->forward(inputL[index], beatPosition);
                outputR[index] = shapeEditor2->forward(inputR[index], beatPosition);
            }
            break;
        }
        case positiveNegative:
        {
            for (uint32_t index = start; index < end; index++) {
                outputL[index] = (inputL[index] > 0) ? shapeEditor1->forward(inputL[index], beatPosition) : shapeEditor2->forward(inputL[index], beatPosition);
                outputR[index] = (inputR[index] > 0) ? shapeEditor1->forward(inputR[index], beatPosition) : shapeEditor2->forward(inputR[index], beatPosition);
            }
            break;
        }
    }
}

// Saves the UDShaper state.
//
// Every object that is not natively serializable has a saveState(clap_ostream_t) method.
// In the following it is described how the UDShaper state is built.
// "..." indicates that an arbitrary number of elements follows.
//
// UDShaper state:
//  version
//  distortion mode
//
//  shapeEditor1 state (variable size):
//      number of ShapePoints
//      shapePoint 1 state
//      shapePoint 2 state
//      ...
//
//  shapeEditor2 state (variable size):
//      number of ShapePoints
//      shapePoint 1 state
//      shapePoint 2 state
//      ...
//
//  EnvelopeManager state (variable size):
//      Envelope 1 state (variable size):
//          number of ShapePoints
//          shapePoint 1 state
//      Modulation links state
//      ...
//
//      frequencyPanel 1 state
//      ...
//
bool UDShaper::saveState(const clap_ostream_t *stream) {
    // Write UDShaper version:
    if (stream->write(stream, &UDSHAPER_VERSION, 3*sizeof(int)) != 3*sizeof(int)) return false;

    // Write current distortion mode:
    if (stream->write(stream, &topMenuBar->mode, sizeof(topMenuBar->mode)) != sizeof(topMenuBar->mode)) return false;

    // Data regarding the ShapeEditors and EnvelopeManager are written by methods of the corresponding instances.
    if (!shapeEditor1->saveState(stream)) return false;
    if (!shapeEditor2->saveState(stream)) return false;
    if (!envelopes->saveState(stream)) return false;

    return true;
}

// Loads the UDShaper state from stream. Firstly reads the UDShaper version from which the state was saved and interprets
// the following data accordingly.
bool UDShaper::loadState(const clap_istream_t *stream){
    // First item is always the version represented as three integer numbers.
    int version[3];
    if (stream->read(stream, version, 3*sizeof(int)) != 3*sizeof(int)) return false;

    // Second item is the current distortion mode.
    stream->read(stream, &topMenuBar->mode, sizeof(distortionMode));
    // Synch topMenuBar mode and rerender it to display the changes made.
    topMenuBar->mode = topMenuBar->mode;
    topMenuBar->setupForRerender();

    // Following data corresponds to the states of ShapeEditors and EnvelopeManager.
    if (!shapeEditor1->loadState(stream, version)) return false;
    if (!shapeEditor2->loadState(stream, version)) return false;
    // To find the locations of all ShapePoints, EnvelopeManager needs pointer to both ShapeEditors.
    ShapeEditor *shapeEditors[2] = {shapeEditor1, shapeEditor2};
    if (!envelopes->loadState(stream, version, shapeEditors)) return false;

    return true;
}
