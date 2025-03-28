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

// Initializes the two ShapeEditors and the EnvelopeManager at positions at with sizes according to the input window size.
UDShaper::UDShaper(uint32_t windowWidth, uint32_t windowHieght) {
    // TODO select the sizes based on the window size.
    uint32_t editorSize1[4] = {50, 50, 450, 650};
    uint32_t editorSize2[4] = {495, 50, 895, 650};
    uint32_t envelopeSize[4] = {950, 50, 1900, 500};

    shapeEditor1 = new ShapeEditor(editorSize1);
    shapeEditor2 = new ShapeEditor(editorSize2);
    envelopes = new EnvelopeManager(envelopeSize);
}

// Forwards left click to all InteractiveGUIElement members and starts to track the mouse drag.
void UDShaper::processLeftClick(uint32_t x, uint32_t y) {
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
    }
}

// Forwards mouse release to all InteractiveGUIElement members and keeps track if any of them requests a context menu to open. Stops tracking the mouse drag.
void UDShaper::processMouseRelease(uint32_t x, uint32_t y) {
    if (mouseDragging) {
        // If it was attempted to link the active Envelope to a modulatable parameter, check if a parameter was selected successfully and add a ModulatedParameter instance to the corresponding Envelope.
        if (envelopes->currentDraggingMode == addLink){
            // Get the closest points for both ShapeEditors to the position the mouse was dragged to
            ShapePoint *closestPoint1, *closestPoint2;

            closestPoint1 = shapeEditor1->getClosestPoint(x, y);
            closestPoint2 = shapeEditor2->getClosestPoint(x, y);

            // check if a close point was found
            if ((closestPoint1 != nullptr) | (closestPoint2 != nullptr)){
                ShapePoint *closest = (closestPoint1 == nullptr) ? closestPoint2 : closestPoint1;
                // Since getClosestPoint() was called on the ShapeEditor instances, their currentDraggingMode value has been set to either position or curveCenter and can be used to find the correct parameter to modulate.
                shapeEditorDraggingMode mode = (closestPoint1 == nullptr) ? shapeEditor2->currentDraggingMode : shapeEditor1->currentDraggingMode;
                
                // Find the correct parameter to modulate. There are the options curveCenter, posX and posY.
                if (mode == curveCenter) {
                    envelopes->addModulatedParameter(closest, 1, modCurveCenterY);
                }
                // If the point position should be modulated, show context menu to select correct direction.
                // Exception: last point is selected, which can only be modulated in y-direction. Do not show the menu in this case and directly add the ModulatedParameter.
                else {
                    MenuRequest::requestedMenu = menuPointPosMod;
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
    // The beatposition is, unlike on the audio threads, not taken from a clap_transport, but calculated for the point in time when this function is called.
    long now = getCurrentTime();
    WaitForSingleObject(synchProcessStartTime, INFINITE);

    // If the host is playing, add the beats that have past since it started playing to the initial beatPosition.
    // The initBeatPosition and startedPlaying are set from the audio threads, where clap_transport is available.
    double beatPosition = hostPlaying ? (initBeatPosition + (now - startedPlaying) / 1000 / 60 * currentTempo) : initBeatPosition;
    // TODO init secondsPlayed has to be calculated from initBeatPosition
    double secondsPlayed = hostPlaying ? (now - startedPlaying) / 1000. : 0;
    ReleaseMutex(synchProcessStartTime);

    shapeEditor1->renderGUI(canvas, beatPosition, secondsPlayed);
    shapeEditor2->renderGUI(canvas, beatPosition, secondsPlayed);
    envelopes->renderGUI(canvas);
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

    switch (distortionMode) {
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

            // TODO get current samplerate
            int samplerate = 44100;

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