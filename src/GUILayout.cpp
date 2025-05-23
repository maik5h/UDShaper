#include "GUILayout.h"

// Sets the coordinates of all sub-elements according to the given width and height values.
void UDShaperLayout::setCoordinates(uint32_t width, uint32_t height) {
        GUIWidth = width;
        GUIHeight = height;
        
        uint32_t margin = RELATIVE_FRAME_WIDTH * GUIWidth; // Free space around edges of GUI.
        
        fullXYXY[0] = 0;
        fullXYXY[1] = 0;
        fullXYXY[2] = width;
        fullXYXY[3] = height;

        // Top menu starts at upper left corner and extends over the whole width of the GUI.
        topMenuXYXY[0] = 0;
        topMenuXYXY[1] = 0;
        topMenuXYXY[2] = GUIWidth;
        topMenuXYXY[3] = GUIHeight * 0.125; // Top menu bar takes up 12.5% of the GUI height.

        // Two ShapeEditors are positioned below the top menu on the left side of the remaining area.
        editor1XYXY[0] = 2 * margin;
        editor1XYXY[1] = topMenuXYXY[3] + 2 * margin;
        editor1XYXY[2] = GUIWidth * 0.25 - margin * 0.5;
        editor1XYXY[3] = GUIHeight - 2 * margin;

        editor2XYXY[0] = GUIWidth * 0.25 + margin * 0.5;
        editor2XYXY[1] = topMenuXYXY[3] + 2 * margin;
        editor2XYXY[2] = GUIWidth * 0.5 - 2 * margin;
        editor2XYXY[3] = GUIHeight - 2 * margin;

        // Editor frame is drawn around the two ShapeEditors.
        editorFrameXYXY[0] = margin;
        editorFrameXYXY[1] = topMenuXYXY[3] + margin;
        editorFrameXYXY[2] = GUIWidth * 0.5 - margin;
        editorFrameXYXY[3] = GUIHeight - margin;

        // EnvelopeManager is located below the top menu bar and right to the ShapeEditors.
        envelopeXYXY[0] = GUIWidth * 0.5 + margin;
        envelopeXYXY[1] = topMenuXYXY[3] + margin;
        envelopeXYXY[2] = GUIWidth - margin;
        envelopeXYXY[3] = GUIHeight - margin;
    }

// Sets the coordinates of all sub-elements according to the area given in XYXY.
void TopMenuBarLayout::setCoordinates(uint32_t XYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight) {
    GUIWidth = inGUIWidth;
    GUIHeight = inGUIHeight;

    uint32_t margin = RELATIVE_FRAME_WIDTH * GUIWidth; // Free space around edges of GUI.

    for (int i=0; i<4; i++) {
        fullXYXY[i] = XYXY[i];
    }

    // The plugin logo is displayed at the upper left corner.
    logoXYXY[0] = fullXYXY[0] + 2 * margin;
    logoXYXY[1] = fullXYXY[1];
    logoXYXY[2] = (uint32_t) (0.15*(fullXYXY[2] - fullXYXY[0] - 4 * margin));
    logoXYXY[3] = fullXYXY[3];

    // The mode button is placed next to the logo.
    modeButtonXYXY[0] = (uint32_t) (0.256*(fullXYXY[2] - fullXYXY[0]));
    modeButtonXYXY[1] = fullXYXY[1];
    modeButtonXYXY[2] = (uint32_t) (0.456*(fullXYXY[2] - fullXYXY[0]));
    modeButtonXYXY[3] = (uint32_t) fullXYXY[3];
}

// Sets the coordinates of all sub-elements according to the area given in XYXY.
void ShapeEditorLayout::setCoordinates(uint32_t XYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight) {
        // Get the current GUI size. This is required to render correctly.
        GUIWidth = inGUIWidth;
        GUIHeight = inGUIHeight;

        for (int i=0; i<4; i++) {
            fullXYXY[i] = XYXY[i];
        }
        
        // Inner area is one RELATIVE_FRAME_WIDTH apart from outer area.
        innerXYXY[0] = fullXYXY[0] + RELATIVE_FRAME_WIDTH * GUIWidth;
        innerXYXY[1] = fullXYXY[1] + RELATIVE_FRAME_WIDTH * GUIWidth;
        innerXYXY[2] = fullXYXY[2] - RELATIVE_FRAME_WIDTH * GUIWidth;
        innerXYXY[3] = fullXYXY[3] - RELATIVE_FRAME_WIDTH * GUIWidth;

        // Editor area is one RELATIVE_FRAME_WIDTH apart from inner area.
        editorXYXY[0] = innerXYXY[0] + RELATIVE_FRAME_WIDTH * GUIWidth;
        editorXYXY[1] = innerXYXY[1] + RELATIVE_FRAME_WIDTH * GUIWidth;
        editorXYXY[2] = innerXYXY[2] - RELATIVE_FRAME_WIDTH * GUIWidth;
        editorXYXY[3] = innerXYXY[3] - RELATIVE_FRAME_WIDTH * GUIWidth;
    }

// Sets the coordinates of all sub-elements according to the area given in XYXY.
void EnvelopeManagerLayout::setCoordinates(uint32_t XYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight) {
        GUIWidth = inGUIWidth;
        GUIHeight = inGUIHeight;

        for (int i; i<4; i++) {
            fullXYXY[i] = XYXY[i];
        }

        // Width and height of the EnvelopeManager. 
        // 10% on left and 405 on the bottom are reserved for other GUI elements, rest is for Envelopes.
        uint32_t width = fullXYXY[2] - fullXYXY[0];
        uint32_t height = fullXYXY[3] - fullXYXY[1];

        // The Envelope is positioned at the upper right corner of the EnvelopeManager.
        editorXYXY[0] = (uint32_t)(fullXYXY[0] + 0.1*width);
        editorXYXY[1] = fullXYXY[1];
        editorXYXY[2] = fullXYXY[2];
        editorXYXY[3] = (uint32_t)(fullXYXY[3] - 0.4*height);

        editorInnerXYXY[0] = editorXYXY[0] + RELATIVE_FRAME_WIDTH * GUIWidth;
        editorInnerXYXY[1] = editorXYXY[1] + RELATIVE_FRAME_WIDTH * GUIWidth;
        editorInnerXYXY[2] = editorXYXY[2] - RELATIVE_FRAME_WIDTH * GUIWidth;
        editorInnerXYXY[3] = editorXYXY[3] - RELATIVE_FRAME_WIDTH * GUIWidth;

        // The selector panel is positioned directly to the left of Envelope, with the same y-extent as the Envelope.
        // editorXYXY is the full size of the graph editor. selectorXYXY has to take the width of the 3D into account.
        selectorXYXY[0] = fullXYXY[0];
        selectorXYXY[1] = fullXYXY[1] + RELATIVE_FRAME_WIDTH * GUIWidth;
        selectorXYXY[2] = editorInnerXYXY[0];
        selectorXYXY[3] = editorInnerXYXY[3];

        // The knobs are positioned below the Envelope, with the same x-extent as the Envelope.
        // For y-position, take width of the 3D frame around the active Envelope into account.
        knobsXYXY[0] = editorXYXY[0];
        knobsXYXY[1] = (uint32_t)(fullXYXY[3] - 0.4*height) + RELATIVE_FRAME_WIDTH * GUIWidth;
        knobsXYXY[2] = fullXYXY[2];
        knobsXYXY[3] = (uint32_t)(fullXYXY[3] - 0.2*height);

        // Tools are positioned below knobs.
        toolsXYXY[0] = editorXYXY[0];
        toolsXYXY[1] = (uint32_t)(fullXYXY[3] - 0.2*height) + RELATIVE_FRAME_WIDTH * GUIWidth;
        toolsXYXY[2] = fullXYXY[2];
        toolsXYXY[3] = fullXYXY[3];
    }

// Sets the coordinates of all sub-elements according to the area given in XYXY.
void FrequencyPanelLayout::setCoordinates(uint32_t XYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight) {
    GUIWidth = inGUIWidth;
    GUIHeight = inGUIHeight;

    for (int i=0; i<4; i++) {
        fullXYXY[i] = XYXY[i];
    }

    // TODO for now counter and button are just split in half. There is room for visual improvement
    counterXYXY[0] = fullXYXY[0];
    counterXYXY[1] = fullXYXY[1];
    counterXYXY[2] = (uint32_t) (fullXYXY[2] + fullXYXY[0]) / 2;
    counterXYXY[3] = fullXYXY[3];

    modeButtonXYXY[0] = (uint32_t) (fullXYXY[2] + fullXYXY[0]) / 2;
    modeButtonXYXY[1] = fullXYXY[1];
    modeButtonXYXY[2] = fullXYXY[2];
    modeButtonXYXY[3] = fullXYXY[3];
}
