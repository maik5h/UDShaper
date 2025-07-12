#include "UDShaperElements.h"
#include "logging.h"

// Claculates the power of the curve for the interpolation mode power from the y value at x=0.5.
// Can be negative, which does NOT mean the curve is actually f(x) = 1 / (x^|power|) but that the curve is flipped vertically and horizontally.
float getPowerFromPosY(float posY){
    // Calculates the power p of a curve f(x) = x^p that goes through yCenter at x=0.5:
    float power = (posY < 0.5) ? log(posY) / log(0.5) :  - log(1 - posY) / log(0.5);

    // Clamp to allowed maximum power
    power = (power < 0) ? ((power < -SHAPE_MAX_POWER) ? -SHAPE_MAX_POWER : power) : ((power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power);
        
    return power;
}

TopMenuBar::TopMenuBar(uint32_t inXYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight) {
    layout.setCoordinates(inXYXY, inGUIWidth, inGUIHeight);
}

void TopMenuBar::processLeftClick(uint32_t x, uint32_t y) {
    if (isInBox(x, y, layout.modeButtonXYXY)) {
        MenuRequest::sendRequest(this, menuDistortionMode);
    }
}

void TopMenuBar::processMouseDrag(uint32_t x, uint32_t y) {};
void TopMenuBar::processMouseRelease(uint32_t x, uint32_t y) {};
void TopMenuBar::processDoubleClick(uint32_t x, uint32_t y) {};
void TopMenuBar::processRightClick(uint32_t x, uint32_t y) {};

void TopMenuBar::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed) {
    // Redraw the logo during window resize.
    if (updateLogo) {
        TextBoxInfo textBoxInfo;
        textBoxInfo.GUIWidth = layout.GUIWidth;
        textBoxInfo.GUIHeight = layout.GUIHeight;
        textBoxInfo.text = "UDShaper";
        textBoxInfo.position[0] = layout.logoXYXY[0];
        textBoxInfo.position[1] = layout.logoXYXY[1];
        textBoxInfo.position[2] = layout.logoXYXY[2];
        textBoxInfo.position[3] = layout.logoXYXY[3];
        textBoxInfo.textHeight = 0.8;
        textBoxInfo.frameWidth = 0;

        drawTextBox(canvas, textBoxInfo);
        updateLogo = false;
    }
    // Redraw mode button when distortion mode has changed or during resize.
    if (updateModeButton) {
        std::string modeText; // The distortion mode as string.

        // Set string according to current distortion mode.
        switch (mode) {
            case upDown:
            modeText = "Up/Down";
            break;

            case leftRight:
            modeText = "Left/Right";
            break;

            case midSide:
            modeText = "Mid/Side";
            break;

            case positiveNegative:
            modeText = "+/-";
            break;
        }

        fillRectangle(canvas, layout.GUIWidth, layout.modeButtonXYXY);

        // Create text "Distortion mode".
        TextBoxInfo textBoxInfo;
        textBoxInfo.GUIWidth = layout.GUIWidth;
        textBoxInfo.GUIHeight = layout.GUIHeight;
        textBoxInfo.text = "Distortion mode";
        textBoxInfo.position[0] = layout.modeButtonXYXY[0];
        textBoxInfo.position[1] = layout.modeButtonXYXY[1];
        textBoxInfo.position[2] = layout.modeButtonXYXY[2];
        textBoxInfo.position[3] = static_cast<uint32_t>((uint32_t) (layout.modeButtonXYXY[3] - layout.modeButtonXYXY[1]) / 2);
        textBoxInfo.frameWidth = 0;

        drawTextBox(canvas, textBoxInfo);

        // Display current distortion mode inside a frame below previous text.
        textBoxInfo.text = modeText;
        textBoxInfo.position[1] = static_cast<uint32_t>(layout.modeButtonXYXY[3] - layout.modeButtonXYXY[1]) / 2;
        textBoxInfo.position[3] = layout.modeButtonXYXY[3];
        textBoxInfo.frameWidth = static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_NARROW * layout.GUIWidth);

        drawTextBox(canvas, textBoxInfo);

        updateModeButton = false;
    }
}

void TopMenuBar::rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight) {
    layout.setCoordinates(newXYXY, newWidth, newHeight);
    updateModeButton = true;
    updateLogo = true;
}

// Updates plugin distortion mode if different mode was selected from a menu.
// void TopMenuBar::processMenuSelection(int menuItem, distortionMode &pluginDistortionMode) {
void TopMenuBar::processMenuSelection(contextMenuType menuType, int menuItem) {
    if (menuType == menuDistortionMode) {
        // Set distortion mode of this TopMenuBar and the forwarded pluginDistortionMode to
        // the mode corresponding to the given menuItem.
        mode = static_cast<distortionMode>(menuItem);
        updateModeButton = true;
    }
}

// After calling, the TopMenuBar will be rerendered once in the next renderGUI() call.
void TopMenuBar::setupForRerender() {
    updateLogo = true;
    updateModeButton = true;
}

/*
---NOTES ON MODULATION---
Some parameters can be "modulated", which means they are changed by internal functions without explicit user input.
The modulation is controlled by an Envelope, which is a graph editor that can be linked to any ModulatedParameter by
the user.

ModulatedParameters are NOT reported to the host as modulatable or automatable parameters, they are only handled
inside the plugin, which makes implementation a lot more straightforward, while not impacting the user experience
too much. (TODO) Modulation managed by the host is possible by linking a macro to the phase of the Envelope curve.

Each ModulatedParameter has a base value, to which it is reset whenever the host is not playing. There is a
modulation offset, which is the accumulated offsets of all Envelopes that are linked to this parameter. The modulated
value is clamped to a parameter specific interval after modulation.

ModulatedParameters and Envelopes that are linked together reference each other. The adding and removing of links
is handled inside of the Envelope.
*/

// Holds a base value, min and max values and a list of *Envelope, that are linked to this parameter. Also has the pointer to the parent ShapePoint.
class ModulatedParameter{
    private:
    float base;     // Base value of this ModulatedParameter. This is the active value when no modulation offset is added. 
    float minValue; // Minimum value this ModulatedParameter can take.
    float maxValue; // Maximum value this ModulatedParameter can take.
    const ShapePoint *parent; // Pointer to the ShapePoint that this parameter is part of.

    std::map<Envelope*, int> envelopeIndices; // Indices of every Envelope in the envelopes vector in EnvelopeManager. Is equal to the order in which Envelopes were added and will never change. Used for serialization.
    std::map<Envelope*, float> modulationAmounts; // Maps the pointer to an Envelope to its scaling factor for modulation.

    public:
    const modulationMode mode; // The type of parameter represented by this instance.

    ModulatedParameter(ShapePoint *parentPoint, modulationMode inMode, float inBase, float inMinValue=-1, float inMaxValue=1) : parent(parentPoint), mode(inMode) {
        base = inBase;
        minValue = inMinValue;
        maxValue = inMaxValue;
    }

    // Registers the given envelope as a modulator to this parameter. This Envelope will now contribute to the modulation when calling .get() with the given amount.
    // The index of the Envelope in the vector of Envelopes in EnvelopeManager is saved in order to serialize the state.
    // Returns true on success and false if the Envelope was already a Modulator of this ModulatedParameter.
    bool addModulator(Envelope *envelope, float amount, int envelopeIdx){
        // Envelopes can only be added once.
        for (auto a : modulationAmounts){
            if (a.first == envelope){
                return false;
            }
        }

        modulationAmounts[envelope] = amount;
        envelopeIndices[envelope] = envelopeIdx;
        return true;
    }

    void removeModulator(Envelope *envelope){
        modulationAmounts.erase(envelope);
    }

    // Returns the modulation amount of the Envelope at the given pointer.
    float getAmount(Envelope *envelope){
        return modulationAmounts.at(envelope);
    }

    // Sets the modulation amount of the Envelope at the given pointer to the given amount.
    void setAmount(Envelope *envelope, float amount){
        amount = (amount < -1) ? -1 : (amount > 1) ? 1 : amount;
        modulationAmounts.at(envelope) = amount;
    }

    // Returns a pointer to the ShapePoint that uses this ModulatedParameter instance.
    const ShapePoint *getParentPoint(){
        return parent;
    }

    // Returns a pointer to the base value of this ModulatedParameter.
    float *getBaseAdress() {
        return &base;
    }

    // Moves the base value of this parameter to the input, should be used when the parameter is explicitly changed by the user.
    void move(float newValue){
        base = newValue < minValue ? minValue : newValue > maxValue ? maxValue : newValue;
    }

    // Returns the current value, i.e. the modulation offsets added to the base clamped to the min and max values.
    float get(double beatPosition = 0, double secondsPlayed = 0.){
        float currentValue = base;
        // Iterate over modulationAmounts, first is pointer to Envelope, second is amount.
        for (auto a : modulationAmounts){
            currentValue += a.second * a.first->modForward(beatPosition, secondsPlayed);
        }

        currentValue = (currentValue > maxValue) ? maxValue : (currentValue < minValue) ? minValue : currentValue;
        return currentValue;
    }
};


/* Class to store all information about a curve Segment on a ShapeEditor. Curve segments are handled in a way that information about the function (e.g. type of interpolation and parameters) are stored in the point next to the right, which marks the beginning of the next segment.
The reason for that is that curve has to always satisfy f(0) = 0 or else the plugin would generate a DC offset, so the left hand side of the first curve segment must stay at [0, 0] and can not be moved.*/
class ShapePoint{
    public:

    // parameters of the parent ShapeEditor:

    int32_t XYXY[4]; // Position and size of parent shapeEditor. Points must stay inside these coordinates.
    const int shapeEditorIndex; // Index of the parent ShapeEditor.

    // Parameters of the point:

    ModulatedParameter posX; // Relative x-position on the graph, 0 <= posX <= 1.
    ModulatedParameter posY; // Relative y-position on the graph, 0 <= posX <= 1.

    ShapePoint *previous = nullptr; // Pointer to the previous ShapePont.
    ShapePoint *next = nullptr; // Pointer to the next ShapePoint.

    
    // Parameters of the curve segment:

    ModulatedParameter curveCenterPosY; // Relative y-value of the curve at the x-center point between this and the previous point.
    float sineOmegaPrevious; // Omega after last update.
    float sineOmega; // Omega for the shapeSine interpolation mode.
    Shapes mode = shapePower; // Interpolation mode between this and previous point.

    modulationMode highlightMode = modNone; // Highlights the point or curve center point on the GUI if not modNone.

    /*x, y are relative positions on the graph editor, e.g. they must be between 0 and 1
    editorSize is screen coordinates of parent shapeEditor in XYXY notation: {xMin, yMin, xMax, yMax}
    previousPoint is pointer to the next ShapePoint on the left hand side of the current point. If there is no point to the left, should be nullptr
    pow is parameter defining the shape of the curve segment, its role is dependent on the mode:
        shapePower: f(x) = x^parameter
    */
    ShapePoint(float x, float y, uint32_t editorSize[4], const int parentIdx, float pow = 1, float omega = 0.5, Shapes initMode = shapePower): shapeEditorIndex(parentIdx), posX(this, modPosX, x, 0), posY(this, modPosY, y, 0), curveCenterPosY(this, modCurveCenterY, 0.5, 0) {
        assert ((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));
        
        for (int i=0; i<4; i++){
            XYXY[i] = editorSize[i];
        }

        mode = initMode;
        sineOmega = omega;
        sineOmegaPrevious = omega;
    }

    // Sets relative positions posX and posY such that the absolute positions are equal to the input x and y. Updates the curve center.
    void updatePositionAbsolute(int32_t x, int32_t y){
        // Move the modulatable relative parameters to the corresponding positions.
        posX.move((float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]));
        posY.move((float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]));
    }

    /*---get-functions for parameters that are dependent on a ModulatedParameter and have to recalculated each time they are used---*/

    // Returns the x-position of this point. Is limited by the position of the next point, meaning modulating a point to the left will push all other points on its way to the left. Mod to the right will stop on the next point.
    float getPosX(double beatPosition = 0., double secondsPlayed = 0.){
        if (next == nullptr){
            return posX.get(beatPosition, secondsPlayed);
        }
        else {
            return (posX.get(beatPosition, secondsPlayed) > next->getPosX(beatPosition, secondsPlayed)) ? next->getPosX(beatPosition, secondsPlayed) : posX.get(beatPosition, secondsPlayed);
        }
    }

    // Retunrs the current y-position of the point.
    float getPosY(double beatPosition = 0., double secondsPlayed = 0.){
        return posY.get(beatPosition, secondsPlayed);
    }

    // Calculates the absolute x-position of the point on the GUI from the relative position posX.
    uint32_t getAbsPosX(double beatPosition = 0., double secondsPlayed = 0.){
        return XYXY[0] + (uint32_t)(getPosX(beatPosition, secondsPlayed) * (XYXY[2] - XYXY[0]));
    }

    // Calculates the absolute y-position of the point on the GUI from the relative position posY. By convention, this value is inversely related to posY.
    uint32_t getAbsPosY(double beatPosition = 0., double secondsPlayed = 0.){
        return XYXY[3] - (uint32_t)(getPosY(beatPosition, secondsPlayed) * (XYXY[3] - XYXY[1]));
    }

    // Calculates the absolute x-position of the curve center associated with this point. Is always the center between this point and the previous.
    uint32_t getCurveCenterAbsPosX(double beatPosition = 0., double secondsPlayed = 0.){
        return (uint32_t)(getAbsPosX(beatPosition, secondsPlayed) + previous->getAbsPosX(beatPosition, secondsPlayed)) / 2;
    }

    // Calculates the absolute y-position of the curve center associated with this point.
    uint32_t getCurveCenterAbsPosY(double beatPosition = 0., double secondsPlayed = 0.){
        // Cast the absPosY to signed int, since the difference will be negative.
        return previous->getAbsPosY(beatPosition, secondsPlayed) + (uint32_t)(curveCenterPosY.get(beatPosition, secondsPlayed) * ((int32_t)getAbsPosY(beatPosition, secondsPlayed) - (int32_t)previous->getAbsPosY(beatPosition, secondsPlayed)));
    }

    void updateParentXYXY(uint32_t newXYXY[4]) {
        for (int i=0; i<4; i++) {
            XYXY[i] = newXYXY[i];
        }
    }

    // updates the Curve center point when manually dragging it
    void updateCurveCenter(int x, int y){
        // nothing to update if line is horizontal
        if (previous->getAbsPosY() == getAbsPosY()){
            return;
        }

        switch (mode){
            case shapePower:
            {
                int32_t yL = previous->getAbsPosY(); // Absolute y-position of the previous point.

                int32_t yMin = (yL > getAbsPosY()) ? getAbsPosY() : yL; // Lower absolute y-position from this and the previous point.
                int32_t yMax = (yL < getAbsPosY()) ? getAbsPosY() : yL; // Higher absolute y-position from this and the previous point.

                // y must not be yL or posY, else there will be a log(0). set y to be at least one pixel off of these values
                y = (y >= yMax) ? yMax - 1 : (y <= yMin) ? yMin + 1 : y;

                // find mouse position normalized to y extent
                curveCenterPosY.move((yL - (float)y) / (yL - (int32_t)getAbsPosY()));
                break;
            }

            case shapeSine:
            {
                /*For shapeSine the center point stays at the same position, while the sineOmega value is still updated when moving the mouse in y-direction.
                Sine wave will always go through this point. When omega is smaller 0.5, the sine will smoothly connect the points, if larger 0.5,
                it will be increased in discrete steps, such that always n + 1/2 cycles lay in the interval between the points.*/

                // TODO fix this.
                // if (sineOmega <= 0.5){
                //     sineOmega = sineOmegaPrevious * pow(0.5, (float)(y - getCurveCenterAbsPosY()) / 50);
                //     sineOmega = (sineOmega < SHAPE_MIN_OMEGA) ? SHAPE_MIN_OMEGA : sineOmega;
                // } else {
                //     // TODO if shift or strg or smth is pressed, be continuous
                //     sineOmega = sineOmegaPrevious + ((getCurveCenterAbsPosY() - y) / 40);
                //     sineOmega -= (int32_t)sineOmega % 2;
                // }
                break;
            }
        }
    }

    void processLeftClick(){
        // Save the previous omega state.
        sineOmegaPrevious = sineOmega;
    }
};

// Deletes the given ShapePoint and connects this points previous and next point in the list.
void deleteShapePoint(ShapePoint *point){
    // The first and last point are special and must not be deleted, as a minimum of two points is necessary to
    // have a curve.
    if (point->previous == nullptr){
        throw std::invalid_argument("Tried to delete the first ShapePoint which can not be deleted.");
    }
    if (point->next == nullptr){
        throw std::invalid_argument("Tried to delete the last ShapePoint which can not be deleted.");
    }

    // Link the new neighboring points together.
    ShapePoint *previous = point->previous;
    ShapePoint *next = point->next;

    previous->next = next;
    next->previous = previous;

    delete point;
}

// Inserts the ShapePoint *point into the linked list before next.
void insertPointBefore(ShapePoint *next, ShapePoint *point){
    if (next->previous == nullptr){
        throw std::invalid_argument("Tried to insert shapePoint before first point, which is not allowed.");
    }

    ShapePoint *previous = next->previous;

    previous->next = point;
    point->previous = previous;

    point->next = next;
    next->previous = point;
}

/*
---SHAPE EDITORS---
ShapeEditors are grapheditors that can be used to design functions on the user interface (effect plugins similar
to this one are called waveshaper, hence the name ShapeEditor). They have two applications, firstly to shape
the input sound and secondly to have a function that controls parameters of the graphs that are used for the
audio processing. The function inside is a function from [0, 1] to some subset of [0, 1].

They are built on ShapePoints, which are points that exist on the interface of the editor. The graph is interpolated
between neighbouring points with one of a few possible interpolation functions. The default interpolation is linear.
Points can be added, removed and moved freely between the neighbouring points.

Since audio is forwarded to the function f(x) defined by the ShapeEditors, it is important to satisfy the condition
f(0) = 0, or else the plugin would create a DC offset, even when no input audio is playing. It was decided to add a
special ShapePoint at x=0, y=0, which can not be moved and is not displayed. The last ShapePoint at x=1
may be moved, but only in y-direction. None of these points can be removed to assure that the function is always
well defined on the interval [0, 1].
*/

ShapeEditor::ShapeEditor(uint32_t position[4], uint32_t inGUIWidth, uint32_t inGUIHeight, int shapeEditorIndex) : index(shapeEditorIndex) {

    layout.setCoordinates(position, inGUIWidth, inGUIHeight);

    // Set up first and last point and link them. These points will always stay first and last point.
    shapePoints = new ShapePoint(0., 0., layout.editorXYXY, index);
    shapePoints->next = new ShapePoint(1., 1., layout.editorXYXY, index);
    shapePoints->next->previous = shapePoints;
}

ShapeEditor::~ShapeEditor() {
    // Loop through all ShapePoints and delete them.
    ShapePoint *current = shapePoints;
    ShapePoint *next;

    while (current != nullptr) {
        next = current->next;
        delete current;
        current = next;
    }
}

// Finds the closest ShapePoint or curve center point. Returns pointer to the corresponding point if it is closer than the squareroot of minimumDistance, else nullptr. The field currentDraggingMode of this ShapeEditor is set to either position or curveCenter.
ShapePoint* ShapeEditor::getClosestPoint(uint32_t x, uint32_t y, float minimumDistance){
    // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.
    //   This would require every point to check mouse position at any time

    float closestDistance = 10E6; // The distance to the closest point, initialized as an arbitrary big number.
    ShapePoint *closestPoint; // Pointer to the closest ShapePoint.

    // Check mouse distance to all points and find closest:

    float distance;
    // start with second point in linked list because first point must always be skipped, see comments at shapePoints declaration
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        // check for distance to point and set mode to position
        distance = (point->getAbsPosX() - x)*(point->getAbsPosX() - x) + (point->getAbsPosY() - y)*(point->getAbsPosY() - y);
        if (distance < closestDistance){
            closestDistance = distance;
            closestPoint = point;
            currentDraggingMode = position;
        }
        // check for distance to curve center and set mode to curveCenter
        distance = (point->getCurveCenterAbsPosX() - x)*(point->getCurveCenterAbsPosX() - x) + (point->getCurveCenterAbsPosY() - y)*(point->getCurveCenterAbsPosY() - y);
        if (distance < closestDistance){
            closestDistance = distance;
            closestPoint = point;
            currentDraggingMode = curveCenter;
        }
    }
    // if curveCenter is close enough, return point, else set dragging mode to none and return nullptr
    if (closestDistance <= minimumDistance){
        return closestPoint;
    } else{
        currentDraggingMode = none;
        return nullptr;
    }
}

void ShapeEditor::processLeftClick(uint32_t x, uint32_t y){
    // check if mouse hovers over the graphical interface and return if not
    // check for slightly higher area to be able to grab point from outside the interface
    float offset = pow(REQUIRED_SQUARED_DISTANCE, 0.5);
    if ((x < layout.editorXYXY[0] - offset) | (x >= layout.editorXYXY[2] + offset) | (y < layout.editorXYXY[1] - offset) | (y >= layout.editorXYXY[3] + offset)){
        return;
    }

    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    // if in proximity to point mark point as currentlyDragging
    if (closestPoint != nullptr){
        closestPoint->processLeftClick();
        currentlyDragging = closestPoint;
    }
}

// Processes double click. If a ShapePoint was deleted by the double click, a pointer to this ShapePoint is returned. If no point has been deleted, nullptr is returned.
void ShapeEditor::processDoubleClick(uint32_t x, uint32_t y){
    if ((x < layout.editorXYXY[0]) | (x >= layout.editorXYXY[2]) | (y < layout.editorXYXY[1]) | (y >= layout.editorXYXY[3])){
        deletedPoint = nullptr;
        return;
    }

    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    if (closestPoint != nullptr){
        // if close to point and point is not last, which cannot be removed, remmove point
        if (currentDraggingMode == position && closestPoint->next != nullptr){
            ShapePoint *nextPoint = closestPoint->next;
            deleteShapePoint(closestPoint);
            deletedPoint = closestPoint;
            return;
        }

        // if double click on curve center, reset power
        else if (currentDraggingMode == curveCenter){
            if (closestPoint->mode == shapePower){
                closestPoint->curveCenterPosY.move(0.5);
            } else if (closestPoint->mode == shapeSine){
                closestPoint->sineOmega = 0.5;
                closestPoint->sineOmegaPrevious = 0.5;
            }
            deletedPoint = nullptr;
            return;
        }
    }

    // if not in proximity to point, add one
    if (closestPoint == nullptr){
        // points in shapePoints should be sorted by x-position, find correct index to insert
        uint32_t insertIdx = 0;
        ShapePoint *insertBefore = shapePoints->next;
        while (insertBefore != nullptr){
            if (insertBefore->getAbsPosX() >= x){
                break;
            }
            insertBefore = insertBefore->next;
        }

        // TODO can i make this line shorter?
        insertPointBefore(insertBefore, new ShapePoint((float)(x - layout.editorXYXY[0]) / (layout.editorXYXY[2] - layout.editorXYXY[0]), (float)(layout.editorXYXY[3] - y) / (layout.editorXYXY[3] - layout.editorXYXY[1]), layout.editorXYXY, index));
    }
    deletedPoint = nullptr;
}

// Process right click and return a contextMenuType if clicked onto a ShapePoint.
void ShapeEditor::processRightClick(uint32_t x, uint32_t y){
    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    // if rightclick on point, show shape menu to change function of curve segment
    if (closestPoint != nullptr && currentDraggingMode == position){
        rightClicked = closestPoint;
        MenuRequest::sendRequest(this, menuShapePoint);
        return;
    }
    // if right click on curve center, reset power
    else if (closestPoint != nullptr && currentDraggingMode == curveCenter){
        if (closestPoint->mode == shapePower){
            closestPoint->curveCenterPosY.move(0.5);
        } else if (closestPoint->mode == shapeSine){
            closestPoint->sineOmega = 0.5;
            closestPoint->sineOmegaPrevious = 0.5;
        }
    }
    // menuRequest = menuNone;
}

void ShapeEditor::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed){
    // set whole area to background color
    fillRectangle(canvas, layout.GUIWidth, layout.innerXYXY, colorEditorBackground);

    if (!GUIInitialized) {
        draw3DFrame(canvas, layout.GUIWidth, layout.innerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH * layout.GUIWidth);
        GUIInitialized = true;
    }

    drawGrid(canvas, layout.GUIWidth, layout.editorXYXY, 3, 2, 0xFFFFFFFF, alphaGrid);
    drawFrame(canvas, layout.GUIWidth, layout.editorXYXY, static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_EDITOR * layout.GUIWidth), 0xFFFFFFFF);

    // first draw all connections between points, then points on top
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawConnection(canvas, point, beatPosition, secondsPlayed, colorCurve);
    }
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawPoint(canvas, layout.GUIWidth, point->getAbsPosX(beatPosition, secondsPlayed), point->getAbsPosY(beatPosition, secondsPlayed), colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        drawPoint(canvas, layout.GUIWidth, point->getCurveCenterAbsPosX(beatPosition, secondsPlayed), point->getCurveCenterAbsPosY(beatPosition, secondsPlayed), colorCurve, RELATIVE_POINT_SIZE_SMALL*layout.GUIWidth);
    
        // Highlight some or all ModulatedParameters. Highlighting is done by drawing a circle around
        // either the point or curve center point of this ShapePoint instance.
        // If highlightModulatedParameters is true, highlight all parameters.
        // If highlightMode of the corresponding point is not modNone, highlight this point even if
        // highlightModulatedParameters is false.
        if (highlightModulatedParameters || point->highlightMode == modCurveCenterY) {
            drawCircle(canvas, layout.GUIWidth, point->getCurveCenterAbsPosX(beatPosition, secondsPlayed), point->getCurveCenterAbsPosY(beatPosition, secondsPlayed), colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        }
        if (highlightModulatedParameters || point->highlightMode == modPosX || point->highlightMode == modPosY) {
            drawCircle(canvas, layout.GUIWidth, point->getAbsPosX(beatPosition, secondsPlayed), point->getAbsPosY(beatPosition, secondsPlayed), colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        }
    }
}

void ShapeEditor::rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight) {
    layout.setCoordinates(newXYXY, newWidth, newHeight);

    // ShapePoints are aware of the size of the editor they belong to. They must be informed of the changes.
    ShapePoint *point = shapePoints;
    while (point != nullptr) {
        point->updateParentXYXY(layout.editorXYXY);
        point = point->next;
    }

    GUIInitialized = false;
}

// Returns the pointer to the ShapePoint that was most recently deleted and resets the deletedPoint attribute to nullptr. Returns nullptr, when no point was deleted.
ShapePoint *ShapeEditor::getDeletedPoint() {
    ShapePoint *deleted = deletedPoint;
    deletedPoint = nullptr;
    return deleted;
}

// Is called when an option on a ShapePoint context menu was selected. Changes the interpolation mode between points if this option was one of shapePower, shapeSine, etc.
// Resets rightClicked to nullptr.
void ShapeEditor::processMenuSelection(contextMenuType menuType, int menuItem){
    // If no point was rightclicked, the menu selection does not concern this ShapeEditor instance.
    if (rightClicked == nullptr){
        return;
    }

    // Only process menu selection if the last opened menu was corresponding to a ShapePoint.
    if (menuType != menuShapePoint) return;

    switch (menuItem) {
        case shapePower:
            rightClicked->mode = shapePower;
            break;
        case shapeSine:
            // rightClicked->mode = shapeSine;
            break;
    }

    // Reset right clicked point and last requested menu after they have been processed.
    rightClicked = nullptr;
}

// Reset currentlyDragging to nullptr and currentDraggingMode to none.
void ShapeEditor::processMouseRelease(uint32_t x, uint32_t y){
    currentlyDragging = nullptr;
    currentDraggingMode = none;

    // ModulatedParameters should only be indicated on the GUI while dragging, so reset
    // highlightModulatedParameters on release.
    highlightModulatedParameters = false;
}

// Draws the connection corresponding to ShapePoint *point (from the previous SHapePoint up to this one).
void ShapeEditor::drawConnection(uint32_t *canvas, ShapePoint *point, double beatPosition, double secondsPlayed, uint32_t color, float thickness){

    uint32_t xMin = point->previous->getAbsPosX(beatPosition, secondsPlayed);
    uint32_t xMax = point->getAbsPosX(beatPosition, secondsPlayed);

    uint32_t yL = point->previous->getAbsPosY(beatPosition, secondsPlayed);
    uint32_t yR = point->getAbsPosY(beatPosition, secondsPlayed);

    switch (point->mode){
        case shapePower:
        {
            float power = getPowerFromPosY(point->curveCenterPosY.get(beatPosition, secondsPlayed));
            // The curve is f(x) = x^power, where x is in [0, 1] and x and y intervals are stretched such that the curve connects this and previous point
            for (int i = xMin; i < xMax; i++) {
                // TODO: antialiasing/ width of curve
                if (power > 0){
                    canvas[i + (uint32_t)((yL - pow((float)(i - xMin) / (xMax - xMin), power) * (int32_t)(yL - yR))) * layout.GUIWidth] = color;
                } else {
                    canvas[i + (uint32_t)((yR + pow((float)(xMax - i) / (xMax - xMin), -power) * (int32_t)(yL - yR))) * layout.GUIWidth] = color;
                }
            }
            break;
        }
        case shapeSine:
        {
            for (int i = xMin; i < xMax; i++) {
                // TODO: antialiasing/ width of curve
                float pi = 3.141592654;
                // normalize curve to one if if frequency is so low that segment is smaller than half a wavelength
                float ampCorrection = (point->sineOmega < 0.5) ? 1 / sin(point->sineOmega * pi) : 1;
                // this works trust me
                canvas[i + (int)(ampCorrection * sin((float)(i - xMin - (xMax - xMin)/2) / (xMax - xMin) * point->sineOmega * 2 * pi) * (yR - yL) / 2 - (yL - yR)/2 + yL) * layout.GUIWidth] = color;
            }
            break;
        }
    }
}

void ShapeEditor::processMouseDrag(uint32_t x, uint32_t y){
    if (!currentlyDragging) return;

    if (currentDraggingMode == position){
        // point is not allowed to go beyond neighbouring points in x-direction or below 0 or above 1 if it first or last point.
        int32_t xLowerLim;
        int32_t xUpperLim;

        // The rightmost point must not move in x-direction. If point is last point set both limits to only allowed value, else choose positions of neighbouring points.
        xLowerLim = currentlyDragging->next == nullptr ? layout.editorXYXY[2] : currentlyDragging->previous->getAbsPosX();
        xUpperLim = currentlyDragging->next == nullptr ? layout.editorXYXY[2] : currentlyDragging->next->getAbsPosX();

        x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
        y = (y > (int32_t)layout.editorXYXY[3]) ? layout.editorXYXY[3] : (y < layout.editorXYXY[1]) ? layout.editorXYXY[1] : y;

        // the rightmost point must not move in x-direction
        if (currentlyDragging->next == nullptr){
            x = layout.editorXYXY[2];
        }

        currentlyDragging->updatePositionAbsolute(x, y);
    }

    else if (currentDraggingMode == curveCenter){
        currentlyDragging->updateCurveCenter(x, y);
    }
}

// Returns the value of the curve defined by this shapeEditor at the x position input. Input is clamped to [0, 1].
float ShapeEditor::forward(float input, double beatPosition, double secondsPlayed){
    // Catching this case is important because due to quantization error, the function might return non-zero values for steep curves, which would result in an DC offset even when no input audio is given.
    if (input == 0) return 0;

    float out;

    // absolute value of input is processed, store information to flip sign after computing if necessary
    bool negativeInput = input < 0;
    input = (input < 0) ? -input : input;

    // limit input to one
    input = (input > 1) ? 1 : input;

    ShapePoint *point = shapePoints->next;

    while(point->getPosX(beatPosition, secondsPlayed) < input){
        point = point->next;
    }

    switch(point->mode){
        case shapePower:
        {
            float xL = point->previous->getPosX(beatPosition, secondsPlayed);
            float yL = point->previous->getPosY(beatPosition, secondsPlayed);

            float relX = (point->getPosX(beatPosition, secondsPlayed) == xL) ? xL : (input - xL) / (point->getPosX(beatPosition, secondsPlayed) - xL);
            float factor = (point->getPosY(beatPosition, secondsPlayed) - yL);

            float power = getPowerFromPosY(point->curveCenterPosY.get(beatPosition, secondsPlayed));

            out = (power > 0) ? yL + pow(relX, power) * factor : point->posY.get(beatPosition, secondsPlayed) - pow(1-relX, -power) * factor;
        }
        case shapeSine:
        break;
    }
    return negativeInput ? -out : out;
}

// Saves the ShapeEditor state to the clap_ostream.
// First saves the number of ShapePoints as an int and then saves:
//  - (float)   point x-position
//  - (float)   point y-position
//  - (Shapes)  point interpolation mode
//  - (float)   point curve center y-position
//  - (float)   point omega value
//
// for every ShapePoint. These values describe the ShapeEditor state entirely.
bool ShapeEditor::saveState(const clap_ostream_t *stream) {
    ShapePoint *point = shapePoints->next;

    // Save number of ShapePoints first.
    int numberPoints = 0;
    while (point != nullptr) {
        point = point->next;
        numberPoints ++;
    }
    if (stream->write(stream, &numberPoints, sizeof(int)) != sizeof(int)) return false;

    point = shapePoints->next;
    while (point != nullptr) {
        // Save ModulatedParameter base values.
        if (stream->write(stream, point->posX.getBaseAdress(), sizeof(float)) != sizeof(float)) return false;
        if (stream->write(stream, point->posY.getBaseAdress(), sizeof(float)) != sizeof(float)) return false;
        if (stream->write(stream, point->curveCenterPosY.getBaseAdress(), sizeof(float)) != sizeof(float)) return false;

        // Save interpolation mode and interpolation parameters.
        if (stream->write(stream, &point->mode, sizeof(point->mode)) != sizeof(point->mode)) return false;
        if (stream->write(stream, &point->sineOmega, sizeof(point->sineOmega)) != sizeof(point->sineOmega)) return false;

        point = point->next;
    }

    return true;
}

// Loads a ShapeEditor state from a clap_istream_t. The state is defined by the state of every
// ShapePoint, which is given by:
//  - (float) posX
//  - (float) posY
//  - (float) curceCenterY
//  - (float) sineOmega
//  - (Shapes) mode
//
// The first element in the stream must be the number of ShapePoints to be loaded. It is
// expected to be called in UDShaper::loadState() after the version number was loaded.
bool ShapeEditor::loadState(const clap_istream_t *stream, int version[3]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        // Read number of ShapePoints in this shapeEditor.
        int numberPoints;
        if (stream->read(stream, &numberPoints, sizeof(int)) != sizeof(int)) return false;

        // Pointer to the rightmost point
        ShapePoint *last = shapePoints->next;

        // Read state of all ShapePoints.
        for (int i=0; i<numberPoints; i++) {
            float posX, posY, curveCenterY, sineOmega;
            Shapes mode;
            int numberLinks;

            if (stream->read(stream, &posX, sizeof(posX)) != sizeof(posX)) return false;
            if (stream->read(stream, &posY, sizeof(posY)) != sizeof(posY)) return false;
            if (stream->read(stream, &curveCenterY, sizeof(curveCenterY)) != sizeof(curveCenterY)) return false;
            if (stream->read(stream, &mode, sizeof(mode)) != sizeof(mode)) return false;
            if (stream->read(stream, &sineOmega, sizeof(sineOmega)) != sizeof(sineOmega)) return false;

            float power = getPowerFromPosY(curveCenterY);

            if (i != numberPoints - 1) {
                ShapePoint *newPoint = new ShapePoint(posX, posY, layout.editorXYXY, power, sineOmega, mode);
                insertPointBefore(last, newPoint);
            }
            else {
                // ShapeEditor is initialized with the last point, so it does not have to be added. Only set the attribute values.
                last->posY.move(posY);
                last->curveCenterPosY.move(curveCenterY);
                last->mode = mode;
                last->sineOmega = sineOmega;
            }
        }
        return true;
    }
    else {
        // TODO if version is not known one of these things should happen:
        //  - If a preset was loaded, it means probably that the preset was saved from a more
        //    recent version of UDShaper and can therefore not be loaded correctly.
        //    There should be a message to warn the user.
        //  - If it happens from copying/ moving by the host, throw an exception probably?
        return false;
    }
    return true;
}

FrequencyPanel::FrequencyPanel(uint32_t inXYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight, envelopeLoopMode initMode, double initValue) {
    currentLoopMode = initMode;
    counterValue[initMode] = initValue;
    layout.setCoordinates(inXYXY, inGUIWidth, inGUIHeight);
}

// Can either
//  - request context menu to select loop mode if clicked on the mode button
//  - start tracking mouse for changing counter value, if clicked on counter
void FrequencyPanel::processLeftClick(uint32_t x, uint32_t y) {
    clickedX = x;
    clickedY = y;

    if (isInBox(x, y, layout.modeButtonXYXY)) {
        MenuRequest::sendRequest(this, menuEnvelopeLoopMode);
    }

    if (isInBox(x, y, layout.counterXYXY)) {
        currentlyDraggingCounter = true;
    }
};

// If last left click was on counter, updates counter value based on y-position of mouse.
void FrequencyPanel::processMouseDrag(uint32_t x, uint32_t y) {
    if (!currentlyDraggingCounter) return;

    switch (currentLoopMode) {
        case envelopeFrequencyTempo: {
            // In this case, the frequency is a power of two between 1/64 and 64.
            // The counter value represents the power and doe stherefore take integer values between -6 and 6.
            int newValue = (int) (previousValue + ((int) clickedY - (int) y) * COUNTER_SENSITIVITY);
            counterValue[currentLoopMode] = (newValue < -6) ? -6 : (newValue > 6) ? 6 : newValue;
            break;
        }
        case envelopeFrequencySeconds: {
            double newValue = previousValue + ((int) clickedY - (int) y) * COUNTER_SENSITIVITY;
            counterValue[currentLoopMode] = (newValue < 0) ? 0 : newValue;
            break;
        }
    }
};

// Saves the current counter value as previousValue and stops mouse dragging.
void FrequencyPanel::processMouseRelease(uint32_t x, uint32_t y) {
    previousValue = counterValue[currentLoopMode];
    currentlyDraggingCounter = false;
};

void FrequencyPanel::processDoubleClick(uint32_t x, uint32_t y) {};
void FrequencyPanel::processRightClick(uint32_t x, uint32_t y) {};

// Renders the counter based on the current counter value
void FrequencyPanel::renderCounter(uint32_t *canvas) {
    std::string counterText; // Counter value as string.

    if (currentLoopMode == envelopeFrequencyTempo) {
        // Convert counter value to index for FREQUENCY_COUNTER_STRINGS, where 0 is the lowest "64/1" and 12 the highest frequency "1/64".
        int stringIdx = (int) counterValue[envelopeFrequencyTempo] + 6;
        counterText = FREQUENCY_COUNTER_STRINGS[stringIdx];
    }
    else if (currentLoopMode == envelopeFrequencySeconds){
        std::stringstream stream; // Counter value as string with two decimals.

        // If value is less than a second, display in milliseconds.
        if (counterValue[envelopeFrequencySeconds] >= 1){
            stream << std::fixed << std::setprecision(2) << counterValue[envelopeFrequencySeconds];
            counterText = stream.str();
            counterText += " s";
        }
        else {
            stream << std::fixed << std::setprecision(0) << 1000*counterValue[envelopeFrequencySeconds];
            counterText = stream.str();
            counterText += " ms";
        }
    }

    // Draw the counter with the current value.
    fillRectangle(canvas, layout.GUIWidth, layout.counterXYXY);

    TextBoxInfo textBoxInfo;
    textBoxInfo.GUIWidth = layout.GUIWidth;
    textBoxInfo.GUIHeight = layout.GUIHeight;
    textBoxInfo.text = counterText;
    textBoxInfo.position[0] = layout.counterXYXY[0];
    textBoxInfo.position[1] = layout.counterXYXY[1];
    textBoxInfo.position[2] = layout.counterXYXY[2];
    textBoxInfo.position[3] = layout.counterXYXY[3];
    textBoxInfo.frameWidth = static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_NARROW * layout.GUIWidth);

    drawTextBox(canvas, textBoxInfo);
}

// Renders the button with the currentLoopMode.
void FrequencyPanel::renderButton(uint32_t *canvas) {
    // The appropriate text to display on the loop mode button.
    std::string buttonText;
    switch (currentLoopMode) {
        case envelopeFrequencyTempo:
            buttonText = "Tempo";
            break;
        case envelopeFrequencySeconds:
            buttonText = "Seconds";
            break;
    }
    fillRectangle(canvas, layout.GUIWidth, layout.modeButtonXYXY);

    TextBoxInfo textBoxInfo;
    textBoxInfo.GUIWidth = layout.GUIWidth;
    textBoxInfo.GUIHeight = layout.GUIHeight;
    textBoxInfo.text = buttonText;
    textBoxInfo.position[0] = layout.modeButtonXYXY[0];
    textBoxInfo.position[1] = layout.modeButtonXYXY[1];
    textBoxInfo.position[2] = layout.modeButtonXYXY[2];
    textBoxInfo.position[3] = layout.modeButtonXYXY[3];
    textBoxInfo.frameWidth = static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_NARROW * layout.GUIWidth);

    drawTextBox(canvas, textBoxInfo);
}

// Renders two boxes:
// Left: a counter, which displays a number, right: a textbox displaying the currentLoopMode.
void FrequencyPanel::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed) {
    // Rerender elements only if their content has changed.
    if (currentlyDraggingCounter || updateCounter) {
        renderCounter(canvas);
        updateCounter = false;
    }
    if (updateButton) {
        renderButton(canvas);
        updateButton = false;
    }
}

void FrequencyPanel::rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight) {
    layout.setCoordinates(newXYXY, newWidth, newHeight);
    updateCounter = true;
    updateButton = true;
}

// Processes a context menu selection if the last opened menu was a Envelope loop mode menu.
void FrequencyPanel::processMenuSelection(contextMenuType menuType, int menuItem) {
    // Only process if last menu was an Envelope loop mode menu.
    if (menuType != menuEnvelopeLoopMode) return;

    envelopeLoopMode previous = currentLoopMode;
    if (menuItem == envelopeFrequencyTempo) {
        currentLoopMode = envelopeFrequencyTempo;
    }
    else if (menuItem == envelopeFrequencySeconds) {
        currentLoopMode = envelopeFrequencySeconds;
    }
    // If the loopMode has changed, rerender both Button and counter on the next renderGUI() call.
    if (currentLoopMode != previous) {
        updateButton = true;
        updateCounter = true;
        previousValue = counterValue[currentLoopMode];
    }
}

// Returns the Envelope phase corresponding to the current beat position and loop mode.
double FrequencyPanel::getEnvelopePhase(double beatPosition, double secondsPlayed) {
    // Convert from beats to bars.
    beatPosition /= 4;
    if (currentLoopMode == envelopeFrequencyTempo) {
        beatPosition = fmod(beatPosition, 1./pow(2, counterValue[envelopeFrequencyTempo]));
        return beatPosition * std::pow(2, counterValue[envelopeFrequencyTempo]);
    }
    else if (currentLoopMode == envelopeFrequencySeconds) {
        return fmod(secondsPlayed, counterValue[envelopeFrequencySeconds]) / counterValue[envelopeFrequencySeconds];
    }
    return 0.;
}

// After calling, the FrequencyPanel will be rerendered once in the next renderGUI() call.
void FrequencyPanel::setupForRerender() {
    updateButton = true;
    updateCounter = true;
}

// Saves the FrequencyPanel state:
//  - (envelopeLoopMode) currentLoopMode
//  counterValue state:
//      - (envelopeLoopMode) mode
//      - (double) value
//      for every possible mode
bool FrequencyPanel::saveState(const clap_ostream_t *stream) {
    if (stream->write(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode)) return false;
    for (auto& value : counterValue) {
        if (stream->write(stream, &value.first, sizeof(envelopeLoopMode)) != sizeof(envelopeLoopMode)) return false;
        if (stream->write(stream, &value.second, sizeof(double)) != sizeof(double)) return false;
    }
    return true;
}

// Reads the FrequencyPanel state depending on the version the state was saved in.
bool FrequencyPanel::loadState(const clap_istream_t *stream, int version[3]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        if (stream->read(stream, &currentLoopMode, sizeof(currentLoopMode)) != sizeof(currentLoopMode)) return false;
        // In version 1.0.0 two envelopeLoopModes are availbale.
        for (int i=0; i<2; i++) {
            envelopeLoopMode mode;
            double value;

            if (stream->read(stream, &mode, sizeof(envelopeLoopMode)) != sizeof(envelopeLoopMode)) return false;
            if (stream->read(stream, &value, sizeof(double)) != sizeof(double)) return false;

            counterValue[mode] = value;
        }
    }
    return true;
}


Envelope::Envelope(uint32_t size[4], uint32_t inGUIWidth, uint32_t inGUIHeight, FrequencyPanel *inPanel, const int index) : ShapeEditor(size, inGUIWidth, inGUIHeight, index) {
    frequencyPanel = inPanel;
};

// Adds a modulation link to the parameter of point coresponding to mode. That means
//  1. adds the parameter to the vector of modulatedParameters of this instance
//  2. adds this Envelope as a modulator to the ModulatedParameter.
void Envelope::addModulatedParameter(ShapePoint *point, float amount, modulationMode mode, int envelopeIdx){
    switch (mode) {
        case modCurveCenterY:
        {
            // Add this Envelope to the list of modulating Envelopes of the parameter.
            // The pointer to this instance is used to get the modulation offset from inside the ModulatedParameter.
            // The Envelope index is used for serialization. It does not change in the lifetime of the Envelopes.
            if (point->curveCenterPosY.addModulator(this, amount, envelopeIdx)) {
                // If successfull, add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
                modulatedParameters.push_back(&point->curveCenterPosY);
            }
            break;
        }
        case modPosY:
        {
            // Add this Envelope to the list of modulating Envelopes of the parameter.
            if (point->posY.addModulator(this, amount, envelopeIdx)) {
                // If successfull, add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
                modulatedParameters.push_back(&point->posY);
            }
            break;
        }
        case modPosX:
        {
            // Add this Envelope to the list of modulating Envelopes of the parameter.
            if (point->posX.addModulator(this, amount, envelopeIdx)) {
                // If successfull, add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
                modulatedParameters.push_back(&point->posX);
            };
            break;
        }
    }
}

// Sets the amount of the ModulatedParameter at index to the given amount
void Envelope::setModulatedParameterAmount(int index, float amount){
    assert (index < modulatedParameters.size());
    modulatedParameters.at(index)->setAmount(this, amount);
}

void Envelope::removeModulatedParameter(int index){
    // Remove highlighting from parent point.
    ShapePoint *point = const_cast<ShapePoint*>(modulatedParameters.at(index)->getParentPoint());
    point->highlightMode = modNone;

    // First remove this Envelope from the list of modulators of the parameter.
    modulatedParameters.at(index)->removeModulator(this);

    // Secondly remove the parameter from the list of modulated parameters of this Envelope.
    modulatedParameters.erase(modulatedParameters.begin() + index);
}

int Envelope::getModulatedParameterNumber(){
    return modulatedParameters.size();
}

float Envelope::getModAmount(int index){
    return modulatedParameters.at(index)->getAmount(this);
}

// Works same as forward, but interprets the input depending on the value and mode of the corresponding FrequencyPanel.
float Envelope::modForward(double beatPosition, double secondsPlayed){
    // Envelope phase is calculated by the corresponding FrequencyPanel.
    beatPosition = frequencyPanel->getEnvelopePhase(beatPosition, secondsPlayed);
    return forward(beatPosition);
}

// Saves the state of all modulation Links of this Envelope:
//  - (int) number of links
//
// and for every link:
//  - (int) index of the ShapeEditor corresponding to the point (0 or 1)
//  - (int) ShapePoint index
//  - (modulationMode) type of modulation
//  - (float) modulation amount
bool Envelope::saveModulationState(const clap_ostream_t *stream) {
    int numberParameters = modulatedParameters.size();
    if (stream->write(stream, &numberParameters, sizeof(int)) != sizeof(int)) return false;

    for (auto& parameter : modulatedParameters) {
        // Find the position of the ShapePoint concerned by this parameter in the list of ShapePoints.
        int pointIdx = -1; // Position of point in the list of ShapePoints. Starts as -1 since the first point in the list is not editable and does not count.
        const ShapePoint *point = parameter->getParentPoint();
        while (point->previous != nullptr) {
            point = point->previous;
            pointIdx ++;
        }

        if (stream->write(stream, &parameter->getParentPoint()->shapeEditorIndex, sizeof(int)) != sizeof(int)) return false;
        if (stream->write(stream, &pointIdx, sizeof(int)) != sizeof(int)) return false;
        if (stream->write(stream, &parameter->mode, sizeof(int)) != sizeof(int)) return false;
        float modAmount = parameter->getAmount(this); // Modulation amount the parameter receives from this Envelope.
        if (stream->write(stream, &modAmount, sizeof(float)) != sizeof(float)) return false;
    }
    return true;
}

// Loads modulation links.
// First, reads the number of links that have been saved. For each saved link loads:
//  - (int) ndex of the ShapeEditor corresponding to the point (0 or 1)
//  - (int) ShapePoint index
//  - (modulationMode) type of modulation
//  - (float) modulation amount
//
// This method requires knowledge of the adresses of the ShapeEditor instances of the plugin to find the ShapePoints that
// are modulated. The adresses are passed as an array of ShapeEditor*.
bool Envelope::loadModulationState(const clap_istream_t *stream, int version[3], ShapeEditor* shapeEditors[2]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        int numberModulationLinks; // Number of modulated parameters using this Envelope.
        if (stream->read(stream, &numberModulationLinks, sizeof(int)) != sizeof(int)) return false;

        for (int i=0; i<numberModulationLinks; i++) {
            int shapeEditorIdx; // ShapeEditor containing ShapePoint using this parameter.
            int pointIdx; // Position of the ShapePoint concerning this parameter in list of ShapePoints.
            modulationMode mode;
            float amount;

            if (stream->read(stream, &shapeEditorIdx, sizeof(int)) != sizeof(int)) return false;
            if (stream->read(stream, &pointIdx, sizeof(int)) != sizeof(int)) return false;
            if (stream->read(stream, &mode, sizeof(modulationMode)) != sizeof(modulationMode)) return false;
            if (stream->read(stream, &amount, sizeof(float)) != sizeof(float)) return false;

            ShapePoint *point = shapeEditors[shapeEditorIdx]->shapePoints->next;
            for (int j=0; j<pointIdx; j++) {
                point = point->next;
            }
            addModulatedParameter(point, amount, mode, index);
        }
    }
    return true;
}

EnvelopeManager::EnvelopeManager(uint32_t inXYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight){
    layout.setCoordinates(inXYXY, inGUIWidth, inGUIHeight);
    activeEnvelopeIndex = 0;

    envelopes.reserve(MAX_NUMBER_ENVELOPES);
    frequencyPanels.reserve(MAX_NUMBER_ENVELOPES);

    // initiate with three envelopes
    addEnvelope();
    addEnvelope();
    addEnvelope();
}

// Adds a new Envelope to the back of envelopes and a FrequencyPanel to the back of frequencyPanels.
void EnvelopeManager::addEnvelope(){
    if (envelopes.size() == MAX_NUMBER_ENVELOPES){
        return;
    }
    // First add new FrequncyPanel so it can be referenced in the new Envelope.
    frequencyPanels.emplace_back(layout.toolsXYXY, layout.GUIWidth, layout.GUIHeight);
    envelopes.emplace_back(layout.editorXYXY, layout.GUIWidth, layout.GUIHeight, &frequencyPanels.back(), envelopes.size());
    updateGUIElements = true;
}

// Sets the Envelope at index as active and updates the GUI accordingly.
void EnvelopeManager::setActiveEnvelope(int index){
    activeEnvelopeIndex = index;

    // Set update bools to true for all elements that have to be rerendered.
    updateGUIElements = true;
    toolsUpdated = true;
    frequencyPanels.at(activeEnvelopeIndex).setupForRerender();
}

// Draw the initial 3D frames around the displayed Envelope. Only has to be called when creating the GUI or the GUI contents have been changed by an user input.
void EnvelopeManager::setupFrames(uint32_t *canvas){
    fillRectangle(canvas, layout.GUIWidth, layout.selectorXYXY, colorBackground);

    // Draw frame around the Envelope.
    draw3DFrame(canvas, layout.GUIWidth, layout.editorInnerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);

    // To draw the selector panel left the the Envelope, first reset the whole area to the background color.
    fillRectangle(canvas, layout.GUIWidth, layout.selectorXYXY, colorBackground);

    // Fill one rectangle at the left of the Envelope in the same color as the Envelope.
    // It indicates which Envelope is currently selected.
    // boxYRange is the Envelope y-range divided by the number of Envelopes.
    float boxYRange = (layout.editorInnerXYXY[3] - layout.editorInnerXYXY[1]) / envelopes.size();
    uint32_t boxXRange = layout.selectorXYXY[2] - layout.selectorXYXY[0];

    // The box to be filled in:
    uint32_t selectorBox[4] = {
        layout.selectorXYXY[0] + RELATIVE_FRAME_WIDTH*layout.GUIWidth,
        layout.selectorXYXY[1] + static_cast<uint32_t>(activeEnvelopeIndex*boxYRange) + RELATIVE_FRAME_WIDTH*layout.GUIWidth,
        layout.selectorXYXY[2],
        layout.selectorXYXY[1] + static_cast<uint32_t>((activeEnvelopeIndex + 1)*boxYRange) + RELATIVE_FRAME_WIDTH*layout.GUIWidth
    };

    // Rounding errors can lead to a mismatch between the box and the Envelope positions. This is only visible when the last Envelope is active.
    // If the last Envelope is selected, set the lower y-position of the box to match the lower y-position of the Envelope.
    if (activeEnvelopeIndex == envelopes.size() - 1) {
        selectorBox[3] = layout.editorInnerXYXY[3];
    }

    // Draw partial frame around the box.
    drawPartial3DFrame(canvas, layout.GUIWidth, selectorBox, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);

    // Fill the box with the Envelope background color. The box is larger in x-direction to overlap with the frame around the Envelope.
    selectorBox[2] += RELATIVE_FRAME_WIDTH*layout.GUIWidth;
    fillRectangle(canvas, layout.GUIWidth, selectorBox, colorEditorBackground);

    Logger::logToFile(std::to_string(boxXRange));

    for (int i=0; i<envelopes.size(); i++) {
        std::string envelopeIdx = std::to_string(i);

        // Size and position of the textbox is calculated relative to the selectorBox and takes the width of the frame into account.
        // It is shifted half the frame width to the left by default.
        uint32_t textBoxXYXY[4] = {
            layout.selectorXYXY[0] + RELATIVE_FRAME_WIDTH * layout.GUIWidth / 2 + 0.23 * boxXRange,
            layout.selectorXYXY[1] + static_cast<uint32_t>(i * boxYRange) + RELATIVE_FRAME_WIDTH * layout.GUIWidth + 0.38 * boxYRange,
            layout.selectorXYXY[2] - 0.23 * boxXRange - RELATIVE_FRAME_WIDTH * layout.GUIWidth / 2,
            layout.selectorXYXY[1] + static_cast<uint32_t>((i + 1) * boxYRange) + RELATIVE_FRAME_WIDTH * layout.GUIWidth - 0.38 * boxYRange
        };

        uint32_t textColorDark = blendColor(colorBackground, 0xFF000000, alphaShadow);

        TextBoxInfo textBoxInfo;
        textBoxInfo.GUIWidth = layout.GUIWidth;
        textBoxInfo.GUIHeight = layout.GUIHeight;
        textBoxInfo.text = envelopeIdx;
        textBoxInfo.position[0] = textBoxXYXY[0];
        textBoxInfo.position[1] = textBoxXYXY[1];
        textBoxInfo.position[2] = textBoxXYXY[2];
        textBoxInfo.position[3] = textBoxXYXY[3];
        textBoxInfo.frameWidth = static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_EDITOR * layout.GUIWidth);
        textBoxInfo.colorFrame = textColorDark;
        textBoxInfo.colorText = textColorDark;

        // For the active Envelope, set the text and frame color to white and shift the box to the right.
        if (i == activeEnvelopeIndex) {
            textBoxInfo.colorFrame = 0xFFFFFFFF;
            textBoxInfo.colorText = 0xFFFFFFFF;
            textBoxInfo.position[0] += RELATIVE_FRAME_WIDTH * layout.GUIWidth;
            textBoxInfo.position[2] += RELATIVE_FRAME_WIDTH * layout.GUIWidth;

            drawTextBox(canvas, textBoxInfo);

            // Restore previous values
            textBoxInfo.colorFrame = textColorDark;
            textBoxInfo.colorText = textColorDark;
            textBoxInfo.position[0] -= RELATIVE_FRAME_WIDTH * layout.GUIWidth;
            textBoxInfo.position[2] -= RELATIVE_FRAME_WIDTH * layout.GUIWidth;
        }
        else {
            drawTextBox(canvas, textBoxInfo);
            }
    }

    // Draw a 3D frame around the link knobs below the Envelope.
    draw3DFrame(canvas, layout.GUIWidth, layout.knobsInnerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);
}

// Draws the GUI of this EnvelopeManager to canvas. Always calls renderGUI on the active Envelope but renders 3D frames, Envelope selection panel and tool panel only if they have been changed.
void EnvelopeManager::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed){
    envelopes[activeEnvelopeIndex].renderGUI(canvas);

    if (updateGUIElements){
        setupFrames(canvas);
        updateGUIElements = false;
    }

    if (toolsUpdated){
        drawKnobs(canvas);
        toolsUpdated = false;
    }

    frequencyPanels.at(activeEnvelopeIndex).renderGUI(canvas, beatPosition, secondsPlayed);
}

void EnvelopeManager::rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight) {
    // Update EnvelopeManager layout.
    layout.setCoordinates(newXYXY, newWidth, newHeight);

    // Update layouts of every Envelope.
    for (Envelope &env : envelopes) {
        env.rescaleGUI(layout.editorXYXY, newWidth, newHeight);
    }

    for (FrequencyPanel &panel : frequencyPanels) {
        panel.rescaleGUI(layout.toolsXYXY, newWidth, newHeight);
    }

    // Rerender all elements.
    updateGUIElements = true;
    toolsUpdated = true;
}

// Set ups the EnvelopeManager to fully rerender the next time renderGUI() is called. Use when plugin window is reopened after being closed.
void EnvelopeManager::setupForRerender() {
    frequencyPanels.at(activeEnvelopeIndex).setupForRerender();
}

// Draws the knobs for the ModulatedParameters of the active Envelope to the tool panel. Knobs are positioned next to each other sorted by their index in the modulatedParameters vector of the parent Envelope.
void EnvelopeManager::drawKnobs(uint32_t *canvas){
    // first reset whole area
    fillRectangle(canvas, layout.GUIWidth, layout.knobsInnerXYXY, blendColor(colorBackground, 0xFF000000, 0.1));

    for (int i=0; i<envelopes[activeEnvelopeIndex].getModulatedParameterNumber(); i++){
        float amount = envelopes[activeEnvelopeIndex].getModAmount(i); // modulation amount of the ith ModulatedParameter
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);
        drawLinkKnob(canvas, layout.GUIWidth, layout.knobsInnerXYXY[0] + spacing*i + spacing/2, static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2), RELATIVE_LINK_KNOB_SIZE * layout.GUIWidth, amount);
    }
}

// Depending on the mouse position, this method does different things:
//  mouse on Envelope: forwards left click to active Envelope.
//  mouse on selection panel: switches active Envelope and attempts to connect it to a parameter, if the mouse is later released close to a point or curveCenter.
//  mouse on tool panel: If over knob, start tracking mouse drag to move knob position.
void EnvelopeManager::processLeftClick(uint32_t x, uint32_t y){
    clickedX = x;
    clickedY = y;

    // switch active Envelope when clicked onto the corresponding switch at the selection panel
    if (isInBox(x, y, layout.selectorXYXY)){
        int newActive = (y - layout.selectorXYXY[1]) * envelopes.size() / (layout.selectorXYXY[3] - layout.selectorXYXY[1]);
        setActiveEnvelope(newActive);

        // After the switch was pressed, assume the following mouse movement to be an attempt to link a parameter to this Envelope:
        currentDraggingMode = addLink;
    }

    // check if it has been clicked on any of the LinkKnobs and set currentdragging mode to moveKnob if true
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getModulatedParameterNumber(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);;
        if (isInPoint(x, y, layout.knobsInnerXYXY[0] + spacing*i + spacing/2, static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2), static_cast<int>(RELATIVE_LINK_KNOB_SIZE*layout.GUIWidth / 2))){
            selectedKnob = i;
            selectedKnobAmount = envelopes.at(activeEnvelopeIndex).getModAmount(i);
            currentDraggingMode = moveKnob;
        }
    }

    // always forward input to active Envelope
    envelopes.at(activeEnvelopeIndex).processLeftClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processLeftClick(x, y);
}

// Forwards double click to the active Envelope.
void EnvelopeManager::processDoubleClick(uint32_t x, uint32_t y){
    envelopes.at(activeEnvelopeIndex).processDoubleClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processDoubleClick(x, y);
}

// Depending on the mouse position, this method does different things:
//  mouse on Envelope: forwards right click to active Envelope.
//  mouse on tool panel: If over knob, open menu to delete the link to the ModulatedParameter.
void EnvelopeManager::processRightClick(uint32_t x, uint32_t y){
    // check all knobs if they have been rightclicked
    for (int i=0; i<envelopes[activeEnvelopeIndex].getModulatedParameterNumber(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);;
        if (isInPoint(x, y, layout.knobsInnerXYXY[0] + spacing*i + spacing/2, static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2), static_cast<int>(RELATIVE_LINK_KNOB_SIZE*layout.GUIWidth / 2))){
            selectedKnob = i;
            MenuRequest::sendRequest(this, menuLinkKnob);
            return;
        }
    }
    
    envelopes.at(activeEnvelopeIndex).processRightClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processRightClick(x, y);
}

// Draws a circle around the parameter corresponding to a link knob close to cursor position (x, y).
// Is used to show which parameter a knob belongs to if the cursor hovers over it.
void EnvelopeManager::highlightHoveredParameter(uint32_t x, uint32_t y) {
    uint32_t box[4]; // Stores coordinates of the box corresponding to a link knob.
    ShapePoint *highlighted = nullptr; // Pointer to the ShapePoint corresponding to the link knob, over which the cursor hovers.

    for (int i=0; i<envelopes[activeEnvelopeIndex].getModulatedParameterNumber(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);;
        // Find the box coordinates of the knob.
        box[0] = layout.knobsInnerXYXY[0] + spacing*i;
        box[1] = layout.knobsInnerXYXY[1];
        box[2] = layout.knobsInnerXYXY[0] + spacing*(i+1);
        box[3] = layout.knobsInnerXYXY[1] + spacing;

        // Get the parent point and modulation mode of the i-th ModulatedParameter.
        ShapePoint *point = const_cast<ShapePoint*>(envelopes.at(activeEnvelopeIndex).modulatedParameters.at(i)->getParentPoint());
        modulationMode mode = envelopes.at(activeEnvelopeIndex).modulatedParameters.at(i)->mode;

        // If mouse is in these coordinates, mark the parameter as highlighted.
        if (isInBox(x, y, box)) {
            point->highlightMode = mode;
            highlighted = point;
        }
        // As there are multiple modulation modes per point, it is possible to have multiple links to the same point.
        // Remove the highlighting only if point has not been processed yet.
        else if (point != highlighted) {
            point->highlightMode = modNone;
        }
    }
}

void EnvelopeManager::processMenuSelection(contextMenuType menuType, int menuItem){
    // If the last menu was a LinkKnob menu and it was selected to remove the link, remove it.
    if (menuType == menuLinkKnob) {
        if (menuItem == removeLink && selectedKnob != -1){
            envelopes.at(activeEnvelopeIndex).removeModulatedParameter(selectedKnob);
            toolsUpdated = true;
        }
    }

    // If it was attempted to modulate a point position, add a ModulatedParameter based on the modulationMode.
    // Set attemptedToModulate to nullptr afterwards to make sure no link is created by accident.
    if (menuType == menuPointPosMod) {
        if (menuItem == modPosX){
            if (attemptedToModulate != nullptr) {
                addModulatedParameter(attemptedToModulate, 1, modPosX);
            }
            attemptedToModulate = nullptr;
        }
        else if (menuItem == modPosY) {
            if (attemptedToModulate != nullptr) {
                addModulatedParameter(attemptedToModulate, 1, modPosY);
            }
            attemptedToModulate = nullptr;
        }
    }

    envelopes.at(activeEnvelopeIndex).processMenuSelection(menuType, menuItem);
    frequencyPanels.at(activeEnvelopeIndex).processMenuSelection(menuType, menuItem);
}

// Resets currentDraggingMode and selectedKnob. Calls processMouseRelease() on the active Envelope.
void EnvelopeManager::processMouseRelease(uint32_t x, uint32_t y){
    currentDraggingMode = envNone;
    envelopes[activeEnvelopeIndex].processMouseRelease(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processMouseRelease(x, y);

    selectedKnob = -1;
}

// Depending on the mouse position, this method does different things:
//  mouse was clicked on Envelope: forwards mouse drag to active Envelope.
//  mouse was clicked on tool panel: If clicked on knob, start to move knob position.
void EnvelopeManager::processMouseDrag(uint32_t x, uint32_t y){
    envelopes[activeEnvelopeIndex].processMouseDrag(x, y);

    // If a knob is moved, change the amount of modulation of this link.
    if (currentDraggingMode == moveKnob){
        // Add the amount added by moving the mouse to the previous amount the knob was set to.
        float amount = selectedKnobAmount + (int)(clickedY - y) * KNOB_SENSITIVITY;

        envelopes.at(activeEnvelopeIndex).setModulatedParameterAmount(selectedKnob, amount);

        // Knob position has changed, so rerender them on the GUI
        toolsUpdated = true;

        // TODO a visualization here would be nice, draw less opaque curve that indicates the shape at maximum modulation
    }

    frequencyPanels.at(activeEnvelopeIndex).processMouseDrag(x, y);
}

// Adds the given point and mode as a ModulatedParameter to the active Envelope.
void EnvelopeManager::addModulatedParameter(ShapePoint *point, float amount, modulationMode mode){
    // Dont modulate point in x-direction, if it is last point.
    // TODO it would be better to not even show the option X in the context menu on attempted modulation, but it is not straightforward with the current implementation. This is easier even though slightly more confusing to the user.
    if ((mode == modPosX) && (point->next == nullptr)) return;

    // Dont add ModulatedParameter if the maximum amount is reached.
    if (envelopes.at(activeEnvelopeIndex).modulatedParameters.size() == MAX_NUMBER_LINKS) return;

    envelopes.at(activeEnvelopeIndex).addModulatedParameter(point, amount, mode, activeEnvelopeIndex);
    toolsUpdated = true;
}

// Removes all links to the given ShapePoint. Must be called when a ShapePoint is deleted to avoid dangling pointers.
// The pointer is already freed when this function is called, it is only used to erase entries in the modulatedParameters vector that use it.
void EnvelopeManager::clearLinksToPoint(ShapePoint *point){
    if (point == nullptr) return;

    for (int i=0; i<envelopes.size(); i++){
        for (int j=envelopes.at(i).modulatedParameters.size()-1; j>=0; j--){
            // Remove the parameters referencing point from modulatedParameters of envelope i.
            if (envelopes.at(i).modulatedParameters.at(j)->getParentPoint() == point){
                envelopes.at(i).modulatedParameters.erase(envelopes.at(i).modulatedParameters.begin() + j);
            }
        }
    }
    toolsUpdated = true;
}

// Saves the EnvelopeManager state. First, all Envelope states are saved (same method as for base ShapeEditor).
// Secondly, FrequencyPanel states are saved.
bool EnvelopeManager::saveState(const clap_ostream_t *stream) {
    int numberEnvelopes = envelopes.size();
    if (stream->write(stream, &numberEnvelopes, sizeof(numberEnvelopes)) != sizeof(numberEnvelopes)) return false;
    for (auto& envelope : envelopes) {
        if (!envelope.saveState(stream)) return false;
        if (!envelope.saveModulationState(stream)) return false;
    }
    for (auto& frequencyPanel : frequencyPanels) {
        if (!frequencyPanel.saveState(stream)) return false;
    }

    return true;
}

// Load the EnvelopeManager state from stream. Firstly, loads Envelope states and Envelope modulation links, then
// FrequencyPanel states.
// This method requires knowledge of the adresses of the ShapeEditor instances of the plugin in order to recover the
// modulation links between the Envelopes and the ModulatedParameters in the ShapeEditors.
bool EnvelopeManager::loadState(const clap_istream_t *stream, int version[3], ShapeEditor *shapeEditors[2]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        int numberEnvelopes;
        stream->read(stream, &numberEnvelopes, sizeof(numberEnvelopes));

        // EnvelopeManager is initialized with three Envelopes. Add Envelopes until the number saved in state is reached.
        // TODO doing it like this means that EnvelopeManager has to be reinitialized when loading a preset. This
        // appears to work in FL Studio but might not work on other hosts.
        for (int i=0; i<numberEnvelopes - 3; i++) {
            addEnvelope();
        }

        for (auto& envelope : envelopes) {
            if (!envelope.loadState(stream, version)) return false;
            if (!envelope.loadModulationState(stream, version, shapeEditors)) return false;
        }
        for (auto& frequencyPanel : frequencyPanels) {
            if (!frequencyPanel.loadState(stream, version)) return false;
        }
    }
}
