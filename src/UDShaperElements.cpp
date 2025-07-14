#include "UDShaperElements.h"

// Consider the function f(x) = x^power, x in [0, 1] -> [0, 1].
// getPowerFromPosY calculates the power that satisfies the following equations:
//  f(0.5) = posY           if posY <= 0.5
//  f(0.5) = 1 - posY       if posY > 0.5
//
// In the second case, -power is returned instead of power. It does NOT mean the curve
// is actually f(x) = 1 / (x^|power|) and only indicates the input was > 0.5.
float getPowerFromPosY(float posY){
    float power = (posY < 0.5) ? log(posY) / log(0.5) :  log(1 - posY) / log(0.5);

    // Clamp to allowed maximum power.
    power = (power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power;
        
    return (posY > 0.5) ? -power : power;
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

    if (updateModeButton) {
        std::string modeText;

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

void TopMenuBar::processMenuSelection(contextMenuType menuType, int menuItem) {
    if (menuType == menuDistortionMode) {
        mode = static_cast<distortionMode>(menuItem);
        updateModeButton = true;
    }
}

void TopMenuBar::setupForRerender() {
    updateLogo = true;
    updateModeButton = true;
}

class ModulatedParameter{
    private:
    float base;                 // Base value of this ModulatedParameter.
    float minValue;             // Minimum value this ModulatedParameter can take.
    float maxValue;             // Maximum value this ModulatedParameter can take.
    const ShapePoint *parent;   // Pointer to the ShapePoint that this parameter is part of.

    // The modulation amount of every Envelope associated with this ModulatedParameter.
    std::map<Envelope*, float> modulationAmounts; 

    public:
    const modulationMode mode; // The type of parameter represented by this instance.

    ModulatedParameter(ShapePoint *parentPoint, modulationMode inMode, float inBase, float inMinValue=-1, float inMaxValue=1) : parent(parentPoint), mode(inMode) {
        base = inBase;
        minValue = inMinValue;
        maxValue = inMaxValue;
    }

    // Registers the given envelope as a modulator to this parameter. This Envelope will now contribute
    // to the modulation when calling .get() with the given amount.
    // Returns true on success and false if the Envelope was already a Modulator of this ModulatedParameter.
    bool addModulator(Envelope *envelope, float amount){
        // Envelopes can only be added once.
        for (auto a : modulationAmounts){
            if (a.first == envelope){
                return false;
            }
        }

        modulationAmounts[envelope] = amount;
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
    // This is used solely for serialization in ShapeEditor::saveState.
    float *getBaseAddress() {
        return &base;
    }

    // Sets the base value of this parameter to the input. Should be used when the parameter is
    // explicitly changed by the user.
    void set(float newValue){
        base = (newValue < minValue) ? minValue : newValue;
        base = (newValue > maxValue) ? maxValue : newValue;
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

class ShapePoint{
    public:

    // parameters of the parent ShapeEditor:
    int32_t XYXY[4]; // Position and size of parent shapeEditor. Points must stay inside these coordinates.
    const int shapeEditorIndex; // Index of the parent ShapeEditor.


    // Parameters of the point:
    ModulatedParameter posX;        // Relative x-position on the graph, 0 <= posX <= 1.
    ModulatedParameter posY;        // Relative y-position on the graph, 0 <= posX <= 1.

    ShapePoint *previous = nullptr; // Pointer to the previous ShapePont.
    ShapePoint *next = nullptr;     // Pointer to the next ShapePoint.

    
    // Parameters of the curve segment:
    float sineOmegaPrevious;    // Omega after last update.
    float sineOmega;            // Omega for the shapeSine interpolation mode.
    Shapes mode = shapePower;   // Interpolation mode between this and previous point.

    // Relative y-value of the curve at the x-center point between this and the previous point.
    ModulatedParameter curveCenterPosY;

    // The parent ShapeEditor will highlight this point or curve center point if highlightMode is not modNone.
    // modPosX and modPosY highlight the point itself, modCurveCenterY highlight the curve center.
    modulationMode highlightMode = modNone;

    // x, y are relative positions on the graph editor, e.g. they must be between 0 and 1
    // editorSize is screen coordinates of parent shapeEditor in XYXY notation: {xMin, yMin, xMax, yMax}
    // previousPoint is pointer to the next ShapePoint on the left hand side of the current point. If there is no point to the left, should be nullptr
    // pow is parameter defining the shape of the curve segment, its role is dependent on the mode:
    //     shapePower: f(x) = x^parameter
    ShapePoint(float x, float y, uint32_t editorSize[4], const int parentIdx, float pow = 1, float omega = 0.5, Shapes initMode = shapePower): shapeEditorIndex(parentIdx), posX(this, modPosX, x, 0), posY(this, modPosY, y, 0), curveCenterPosY(this, modCurveCenterY, 0.5, 0) {
        assert ((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));
        
        for (int i=0; i<4; i++){
            XYXY[i] = editorSize[i];
        }

        mode = initMode;
        sineOmega = omega;
        sineOmegaPrevious = omega;
    }

    // Sets relative positions posX and posY such that the absolute positions are equal to the input x and y.
    // Updates the curve center.
    void updatePositionAbsolute(int32_t x, int32_t y){
        // Move the modulatable relative parameters to the corresponding positions.
        posX.set(static_cast<float>(x - XYXY[0]) / (XYXY[2] - XYXY[0]));
        posY.set(static_cast<float>(XYXY[3] - y) / (XYXY[3] - XYXY[1]));
    }

    // get-functions for parameters that are dependent on a ModulatedParameter and have to recalculated each time
    // they are used:

    // Returns the relative x-position of this point. It is limited by the position of the next point, meaning
    // modulating a point to the left will push all other points on its way to the left. Mod to the right will
    // stop on the next point.
    float getPosX(double beatPosition = 0., double secondsPlayed = 0.){
        // Rightmost point must always be at x = 1;
        if (next == nullptr){
            return 1;
        }
        else {
            float x = posX.get(beatPosition, secondsPlayed);
            float nextX = next->getPosX(beatPosition, secondsPlayed);
            return (x > nextX) ? nextX : x;
        }
    }

    // Returns the relative y-position of the point.
    float getPosY(double beatPosition = 0., double secondsPlayed = 0.){
        return posY.get(beatPosition, secondsPlayed);
    }

    // Calculates the absolute x-position of the point on the GUI from the relative position posX.
    uint32_t getAbsPosX(double beatPosition = 0., double secondsPlayed = 0.){
        return XYXY[0] + static_cast<uint32_t>(getPosX(beatPosition, secondsPlayed) * (XYXY[2] - XYXY[0]));
    }

    // Calculates the absolute y-position of the point on the GUI from the relative position posY.
    // By convention, this value is inversely related to posY.
    uint32_t getAbsPosY(double beatPosition = 0., double secondsPlayed = 0.){
        return XYXY[3] - static_cast<uint32_t>(getPosY(beatPosition, secondsPlayed) * (XYXY[3] - XYXY[1]));
    }

    // Calculates the absolute x-position of the curve center associated with this point.
    // It is always the center between this point and the previous.
    uint32_t getCurveCenterAbsPosX(double beatPosition = 0., double secondsPlayed = 0.){
        return static_cast<uint32_t>((getAbsPosX(beatPosition, secondsPlayed) + previous->getAbsPosX(beatPosition, secondsPlayed)) / 2);
    }

    // Calculates the absolute y-position of the curve center associated with this point.
    uint32_t getCurveCenterAbsPosY(double beatPosition = 0., double secondsPlayed = 0.){
        int32_t y           = static_cast<int32_t>(getAbsPosY(beatPosition, secondsPlayed));
        int32_t yL   = static_cast<int32_t>(previous->getAbsPosY(beatPosition, secondsPlayed));
        float curveCenterY  = curveCenterPosY.get(beatPosition, secondsPlayed);

        int32_t yExtent = y - yL;

        return yL + static_cast<uint32_t>(curveCenterY * yExtent);
    }

    // Sets the registered box coordinates of the parent ShapeEditor to newXYXY.
    // Must be called when the GUI is resized to properly determine the absolute position
    // of this ShapePoint.
    void updateParentXYXY(uint32_t newXYXY[4]) {
        for (int i=0; i<4; i++) {
            XYXY[i] = newXYXY[i];
        }
    }

    // Updates the Curve center point when manually dragging it
    void updateCurveCenter(uint32_t x, uint32_t y){
        // Nothing to update if curve segment is flat.
        if (previous->getAbsPosY() == getAbsPosY()) return;

        switch (mode){
            case shapePower:
            {
                uint32_t yL = previous->getAbsPosY();

                uint32_t yMin = (yL > getAbsPosY()) ? getAbsPosY() : yL;
                uint32_t yMax = (yL < getAbsPosY()) ? getAbsPosY() : yL;

                // y must not be yMax or yMin, else there will be a log(0). set y to be at least one pixel off of
                // these values.
                y = (y >= yMax) ? yMax - 1 : (y <= yMin) ? yMin + 1 : y;

                curveCenterPosY.set(static_cast<float>(yL - y) / static_cast<float>(yL - getAbsPosY()));
                break;
            }

            case shapeSine:
            {
                // For shapeSine the center point stays at the same position, while the sineOmega value is
                // still updated when moving the mouse in y-direction. Sine wave will always go through this
                // point. When omega is smaller 0.5, the sine will smoothly connect the points, if larger 0.5,
                // it will be increased in discrete steps, such that always n + 1/2 cycles lay in the interval
                // between the points.

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

// Inserts the ShapePoint *point into the linked list before *next.
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

ShapeEditor::ShapeEditor(uint32_t position[4], uint32_t inGUIWidth, uint32_t inGUIHeight, int shapeEditorIndex) : index(shapeEditorIndex) {

    layout.setCoordinates(position, inGUIWidth, inGUIHeight);

    // Since audio is forwarded to the function f(x) defined by the ShapeEditors, it is important to satisfy the condition
    // f(0) = 0, or else the plugin would create a DC offset, even when no input audio is playing. There is a
    // special ShapePoint at x=0, y=0, which can not be moved and is not displayed.
    // The last ShapePoint at x=1 may be moved, but only in y-direction. None of these points can be removed to
    // assure that the function is always well defined on the interval [0, 1].
    shapePoints = new ShapePoint(0., 0., layout.editorXYXY, index);
    shapePoints->next = new ShapePoint(1., 1., layout.editorXYXY, index);

    shapePoints->next->previous = shapePoints;
}

ShapeEditor::~ShapeEditor() {
    ShapePoint *current = shapePoints;
    ShapePoint *next;

    while (current != nullptr) {
        next = current->next;
        delete current;
        current = next;
    }
}

ShapePoint* ShapeEditor::getClosestPoint(uint32_t x, uint32_t y, float minimumDistance){
    // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.
    // This would require every point to check mouse position at any time

    // TODO i do not like the implementation using the internal currentDarggingMode to communicate if point
    // or curve center was closer at all. Maybe this can be replaced by an optional argument of a pointer to a
    // ShapeEditorDraggingMode object.

    float closestDistance = 10E6;   // The distance to the closest point, initialized as an arbitrary big number.
    ShapePoint *closestPoint;       // Pointer to the closest ShapePoint.

    // Check distance to all points and find closest:

    float distance;

    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        distance = (point->getAbsPosX() - x)*(point->getAbsPosX() - x) + (point->getAbsPosY() - y)*(point->getAbsPosY() - y);
        if (distance < closestDistance){
            closestDistance = distance;
            closestPoint = point;
            currentDraggingMode = position;
        }

        distance = (point->getCurveCenterAbsPosX() - x)*(point->getCurveCenterAbsPosX() - x) + (point->getCurveCenterAbsPosY() - y)*(point->getCurveCenterAbsPosY() - y);
        if (distance < closestDistance){
            closestDistance = distance;
            closestPoint = point;
            currentDraggingMode = curveCenter;
        }
    }

    if (closestDistance <= minimumDistance){
        return closestPoint;
    } else{
        currentDraggingMode = none;
        return nullptr;
    }
}

ShapePoint *ShapeEditor::getDeletedPoint() {
    ShapePoint *deleted = deletedPoint;
    deletedPoint = nullptr;
    return deleted;
}

void ShapeEditor::processLeftClick(uint32_t x, uint32_t y){
    if (!isInBox(x, y, layout.innerXYXY)) return;

    ShapePoint *closestPoint = getClosestPoint(x, y);

    // If in proximity to point mark point as currentlyDragging.
    if (closestPoint != nullptr){
        closestPoint->processLeftClick();
        currentlyDragging = closestPoint;
    }
}

void ShapeEditor::processDoubleClick(uint32_t x, uint32_t y){
    if (!isInBox(x, y, layout.innerXYXY)){
        deletedPoint = nullptr;
        return;
    }

    ShapePoint *closestPoint = getClosestPoint(x, y);

    if (closestPoint != nullptr){
        // Delete point if double click was performed close to point.
        if (currentDraggingMode == position && closestPoint->next != nullptr){
            deleteShapePoint(closestPoint);
            deletedPoint = closestPoint;
            return;
        }

        // Reset interpolation shape if double click was performed close to curve center.
        else if (currentDraggingMode == curveCenter){
            if (closestPoint->mode == shapePower){
                closestPoint->curveCenterPosY.set(0.5);
            } else if (closestPoint->mode == shapeSine){
                closestPoint->sineOmega = 0.5;
                closestPoint->sineOmegaPrevious = 0.5;
            }
            deletedPoint = nullptr;
            return;
        }
    }

    // If not in proximity to point, add one.
    if (closestPoint == nullptr){
        ShapePoint *insertBefore = shapePoints->next;
        while (insertBefore != nullptr){
            if (insertBefore->getAbsPosX() >= x){
                break;
            }
            insertBefore = insertBefore->next;
        }

        float newPointX = static_cast<float>((x - layout.editorXYXY[0])) / (layout.editorXYXY[2] - layout.editorXYXY[0]);
        float newPointY = static_cast<float>((layout.editorXYXY[3] - y)) / (layout.editorXYXY[3] - layout.editorXYXY[1]);

        insertPointBefore(insertBefore, new ShapePoint(newPointX, newPointY, layout.editorXYXY, index));
    }
    deletedPoint = nullptr;
}

void ShapeEditor::processRightClick(uint32_t x, uint32_t y){
    ShapePoint *closestPoint = getClosestPoint(x, y);

    if (closestPoint == nullptr) return;

    // If rightclicked on point, show shape menu to change function of curve segment.
    if (currentDraggingMode == position){
        rightClicked = closestPoint;
        MenuRequest::sendRequest(this, menuShapePoint);
        return;
    }
    // If rightclicked on curve center, reset interpolation shape.
    else if (currentDraggingMode == curveCenter){
        if (closestPoint->mode == shapePower){
            closestPoint->curveCenterPosY.set(0.5);
        } else if (closestPoint->mode == shapeSine){
            closestPoint->sineOmega = 0.5;
            closestPoint->sineOmegaPrevious = 0.5;
        }
    }
}

void ShapeEditor::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed){
    fillRectangle(canvas, layout.GUIWidth, layout.innerXYXY, colorEditorBackground);

    if (!GUIInitialized) {
        draw3DFrame(canvas, layout.GUIWidth, layout.innerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH * layout.GUIWidth);
        GUIInitialized = true;
    }

    drawGrid(canvas, layout.GUIWidth, layout.editorXYXY, 3, 2, 0xFFFFFFFF, alphaGrid);
    drawFrame(canvas, layout.GUIWidth, layout.editorXYXY, static_cast<uint32_t>(RELATIVE_FRAME_WIDTH_EDITOR * layout.GUIWidth), 0xFFFFFFFF);

    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawConnection(canvas, point, beatPosition, secondsPlayed, colorCurve);
    }
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        uint32_t pointX = point->getAbsPosX(beatPosition, secondsPlayed);
        uint32_t pointY = point->getAbsPosY(beatPosition, secondsPlayed);
        uint32_t centerX = point->getCurveCenterAbsPosX(beatPosition, secondsPlayed);
        uint32_t centerY = point->getCurveCenterAbsPosY(beatPosition, secondsPlayed);

        drawPoint(canvas, layout.GUIWidth, pointX, pointY, colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        drawPoint(canvas, layout.GUIWidth, centerX, centerY, colorCurve, RELATIVE_POINT_SIZE_SMALL*layout.GUIWidth);
    
        // Highlight some or all ModulatedParameters by drawing a circle around either the point or curve
        // cenetr point.
        // If highlightModulatedParameters is true, highlight all parameters.
        // If highlightMode of the corresponding point is not modNone, highlight this point even if
        // highlightModulatedParameters is false.
        if (highlightModulatedParameters || point->highlightMode == modPosX || point->highlightMode == modPosY) {
            drawCircle(canvas, layout.GUIWidth, pointX, pointY, colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        }
        if (highlightModulatedParameters || point->highlightMode == modCurveCenterY) {
            drawCircle(canvas, layout.GUIWidth, centerX, centerY, colorCurve, RELATIVE_POINT_SIZE*layout.GUIWidth);
        }
    }
}

void ShapeEditor::rescaleGUI(uint32_t newXYXY[4], uint32_t newWidth, uint32_t newHeight) {
    layout.setCoordinates(newXYXY, newWidth, newHeight);

    ShapePoint *point = shapePoints;
    while (point != nullptr) {
        point->updateParentXYXY(layout.editorXYXY);
        point = point->next;
    }

    GUIInitialized = false;
}

// Is called when an option on a ShapePoint context menu was selected. Changes the interpolation mode between
// points if this option was one of shapePower, shapeSine, etc. Resets rightClicked to nullptr.
void ShapeEditor::processMenuSelection(contextMenuType menuType, int menuItem){
    // Only process menu selection if the last opened menu was concerning a ShapePoint.
    if (menuType != menuShapePoint) return;

    switch (menuItem) {
        case shapePower:
            rightClicked->mode = shapePower;
            break;
        case shapeSine:
            // rightClicked->mode = shapeSine;
            break;
    }

    rightClicked = nullptr;
}

void ShapeEditor::processMouseRelease(uint32_t x, uint32_t y){
    currentlyDragging = nullptr;
    currentDraggingMode = none;

    // ModulatedParameters should only be indicated on the GUI while dragging, so reset
    // highlightModulatedParameters on release.
    highlightModulatedParameters = false;
}

void ShapeEditor::drawConnection(uint32_t *canvas, ShapePoint *point, double beatPosition, double secondsPlayed, uint32_t color, float thickness){

    uint32_t xMin = point->previous->getAbsPosX(beatPosition, secondsPlayed);
    uint32_t xMax = point->getAbsPosX(beatPosition, secondsPlayed);

    uint32_t yL = point->previous->getAbsPosY(beatPosition, secondsPlayed);
    uint32_t yR = point->getAbsPosY(beatPosition, secondsPlayed);

    int32_t yExtent = static_cast<int32_t>(yL - yR);

    switch (point->mode){
        case shapePower:
        {
            float power = getPowerFromPosY(point->curveCenterPosY.get(beatPosition, secondsPlayed));
            // The curve is f(x) = x^power, where x is in [0, 1] and x and y intervals are stretched such that
            // the curve connects this and previous point.
            for (int i = xMin; i < xMax; i++) {
                // TODO: implement antialiasing/ width of curve
                double relativeY;
                if (power > 0){
                    relativeY = pow(static_cast<double>(i - xMin) / (xMax - xMin), power);
                    canvas[i + static_cast<uint32_t>(yL - relativeY * yExtent) * layout.GUIWidth] = color;
                } else {
                    relativeY = pow(static_cast<double>(xMax - i) / (xMax - xMin), -power);
                    canvas[i + static_cast<uint32_t>(yR + relativeY * yExtent) * layout.GUIWidth] = color;
                }
            }
            break;
        }
        case shapeSine:
        {
            for (int i = xMin; i < xMax; i++) {
                double relativeX = static_cast<double>(i - xMin - (xMax - xMin)/2) / (xMax - xMin);
                double relativeY = sin(relativeX * 2 * M_PI * point->sineOmega);

                // Normalize curve to one if if frequency is so low that segment is smaller than half a wavelength.
                float ampCorrection = (point->sineOmega < 0.5) ? 1 / sin(point->sineOmega * M_PI) : 1;

                canvas[i + static_cast<int>(yL + ampCorrection * relativeY * (yR - yL) / 2 - (yL - yR)/2) * layout.GUIWidth] = color;
            }
            break;
        }
    }
}

void ShapeEditor::processMouseDrag(uint32_t x, uint32_t y){
    if (!currentlyDragging) return;

    if (currentDraggingMode == position){
        // The rightmost point must not move in x-direction.
        if (currentlyDragging->next == nullptr){
            x = layout.editorXYXY[2];
            currentlyDragging->updatePositionAbsolute(x, y);
            return;
        }

        int32_t xLowerLim = currentlyDragging->previous->getAbsPosX();
        int32_t xUpperLim = currentlyDragging->next->getAbsPosX();

        x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
        y = (y > layout.editorXYXY[3]) ? layout.editorXYXY[3] : (y < layout.editorXYXY[1]) ? layout.editorXYXY[1] : y;

        currentlyDragging->updatePositionAbsolute(x, y);
    }

    else if (currentDraggingMode == curveCenter){
        currentlyDragging->updateCurveCenter(x, y);
    }
}

float ShapeEditor::forward(float input, double beatPosition, double secondsPlayed){
    // Catching this case is important because the function might return non-zero values for steep curves
    // due to quantization errors, which would result in an DC offset even when no input audio is given.
    if (input == 0) return 0;

    float out;

    // Absolute value of input is processed, information about sign is saved to flip output after computing.
    bool flipOutput = input < 0;
    input = (input < 0) ? -input : input;

    input = (input > 1) ? 1 : input;

    ShapePoint *point = shapePoints->next;

    // Find the curve segment concerned by the input.
    while(point->getPosX(beatPosition, secondsPlayed) < input){
        point = point->next;
    }

    switch(point->mode){
        case shapePower:
        {
            float xL = point->previous->getPosX(beatPosition, secondsPlayed);
            float yL = point->previous->getPosY(beatPosition, secondsPlayed);
            float x = point->getPosX(beatPosition, secondsPlayed);

            // Compute relative x-position inside the curve segment, relative "height" of the curve segment and power
            // corresponding to the current curve cenetr position.
            float relX = (x == xL) ? xL : (input - xL) / (x - xL);
            float segmentYExtent = (point->getPosY(beatPosition, secondsPlayed) - yL);
            float power = getPowerFromPosY(point->curveCenterPosY.get(beatPosition, secondsPlayed));

            if (power > 0) {
                out = yL + pow(relX, power) * segmentYExtent;
            }
            else {
                out = point->getPosY(beatPosition, secondsPlayed) - pow(1-relX, -power) * segmentYExtent;
            }
        }
        case shapeSine:
        break;
    }
    return flipOutput ? -out : out;
}

// Saves the ShapeEditor state to the clap_ostream.
// First saves the number of ShapePoints as an int and then saves:
//  - (float)   point x-position
//  - (float)   point y-position
//  - (float)   point curve center y-position
//  - (Shapes)  point interpolation mode
//  - (float)   point omega value
//
// for every ShapePoint. These values describe the ShapeEditor state entirely.
bool ShapeEditor::saveState(const clap_ostream_t *stream) {
    ShapePoint *point = shapePoints->next;

    int numberPoints = 0;
    while (point != nullptr) {
        point = point->next;
        numberPoints ++;
    }

    if (stream->write(stream, &numberPoints, sizeof(int)) != sizeof(int)) return false;

    point = shapePoints->next;
    while (point != nullptr) {
        // Save ModulatedParameter base values.
        if (stream->write(stream, point->posX.getBaseAddress(), sizeof(float)) != sizeof(float)) return false;
        if (stream->write(stream, point->posY.getBaseAddress(), sizeof(float)) != sizeof(float)) return false;
        if (stream->write(stream, point->curveCenterPosY.getBaseAddress(), sizeof(float)) != sizeof(float)) return false;

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
//  - (Shapes) mode
//  - (float) sineOmega
//
// The first element in the stream must be the number of ShapePoints to be loaded. It is
// expected to be called in UDShaper::loadState() after the version number was loaded.
bool ShapeEditor::loadState(const clap_istream_t *stream, int version[3]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        int numberPoints;
        if (stream->read(stream, &numberPoints, sizeof(int)) != sizeof(int)) return false;

        ShapePoint *last = shapePoints->next;

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
                // The last point is created when ShapeEditor is initialized, so it does not have to be added.
                // Only set the attribute values.
                last->posY.set(posY);
                last->curveCenterPosY.set(curveCenterY);
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

void FrequencyPanel::processMouseDrag(uint32_t x, uint32_t y) {
    if (!currentlyDraggingCounter) return;

    switch (currentLoopMode) {
        case envelopeFrequencyTempo: {
            // In this case, the frequency is a (negative) integer power of 2 between 1/64 and 64.
            // The counter value represents the power and does stherefore take integer values between -6 and 6.
            int counterOffset = (static_cast<int>(clickedY) - static_cast<int>(y)) * COUNTER_SENSITIVITY;
            int newValue = static_cast<int>(previousValue) + counterOffset;

            counterValue[currentLoopMode] = (newValue < -6) ? -6 : (newValue > 6) ? 6 : newValue;

            break;
        }
        case envelopeFrequencySeconds: {
            double newValue = previousValue + (static_cast<int>(clickedY) - static_cast<int>(y)) * COUNTER_SENSITIVITY;
            counterValue[currentLoopMode] = (newValue < 0) ? 0 : newValue;
            break;
        }
    }
};

void FrequencyPanel::processMouseRelease(uint32_t x, uint32_t y) {
    previousValue = counterValue[currentLoopMode];
    currentlyDraggingCounter = false;
};

void FrequencyPanel::processDoubleClick(uint32_t x, uint32_t y) {};
void FrequencyPanel::processRightClick(uint32_t x, uint32_t y) {};

void FrequencyPanel::renderCounter(uint32_t *canvas) {
    std::string counterText;

    if (currentLoopMode == envelopeFrequencyTempo) {
        // Convert counter value to index for FREQUENCY_COUNTER_STRINGS, where 0 is the lowest
        // "64/1" and 12 the highest frequency "1/64".
        int stringIdx = static_cast<int>(counterValue[envelopeFrequencyTempo]) + 6;
        counterText = FREQUENCY_COUNTER_STRINGS[stringIdx];
    }
    else if (currentLoopMode == envelopeFrequencySeconds){
        std::stringstream counterStream;

        // If value is less than a second, display in milliseconds.
        if (counterValue[envelopeFrequencySeconds] >= 1){
            counterStream << std::fixed << std::setprecision(2) << counterValue[envelopeFrequencySeconds];
            counterText = counterStream.str();
            counterText += " s";
        }
        else {
            counterStream << std::fixed << std::setprecision(0) << 1000 * counterValue[envelopeFrequencySeconds];
            counterText = counterStream.str();
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

void FrequencyPanel::renderButton(uint32_t *canvas) {
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

void FrequencyPanel::processMenuSelection(contextMenuType menuType, int menuItem) {
    // FrequencyPanels are only concerned by Envelope loop mode menus.
    if (menuType != menuEnvelopeLoopMode) return;

    envelopeLoopMode previous = currentLoopMode;
    currentLoopMode = static_cast<envelopeLoopMode>(menuItem);

    if (currentLoopMode != previous) {
        updateButton = true;
        updateCounter = true;
        previousValue = counterValue[currentLoopMode];
    }
}

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

void Envelope::addModulatedParameter(ShapePoint *point, float amount, modulationMode mode, int envelopeIdx){
    switch (mode) {
        case modCurveCenterY:
        {
            if (point->curveCenterPosY.addModulator(this, amount)) {
                modulatedParameters.push_back(&point->curveCenterPosY);
            }
            break;
        }
        case modPosY:
        {
            if (point->posY.addModulator(this, amount)) {
                modulatedParameters.push_back(&point->posY);
            }
            break;
        }
        case modPosX:
        {
            if (point->posX.addModulator(this, amount)) {
                modulatedParameters.push_back(&point->posX);
            };
            break;
        }
    }
}

void Envelope::setModulatedParameterAmount(int index, float amount){
    assert (index < modulatedParameters.size());
    modulatedParameters.at(index)->setAmount(this, amount);
}

void Envelope::removeModulatedParameter(int index){
    // Remove highlighting from parent point.
    ShapePoint *point = const_cast<ShapePoint*>(modulatedParameters.at(index)->getParentPoint());
    point->highlightMode = modNone;

    modulatedParameters.at(index)->removeModulator(this);
    modulatedParameters.erase(modulatedParameters.begin() + index);
}

int Envelope::getNumberModulatedParameters(){
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

        // Position of point in the list of ShapePoints. Starts as -1 since the first point in the
        // list is not editable and does not count.
        int pointIdx = -1;
        const ShapePoint *point = parameter->getParentPoint();
        while (point->previous != nullptr) {
            point = point->previous;
            pointIdx ++;
        }

        if (stream->write(stream, &parameter->getParentPoint()->shapeEditorIndex, sizeof(int)) != sizeof(int)) return false;
        if (stream->write(stream, &pointIdx, sizeof(int)) != sizeof(int)) return false;
        if (stream->write(stream, &parameter->mode, sizeof(int)) != sizeof(int)) return false;
        float modAmount = parameter->getAmount(this);
        if (stream->write(stream, &modAmount, sizeof(float)) != sizeof(float)) return false;
    }
    return true;
}

// Loads modulation links.
// First, reads the number of links that have been saved.
//  - (int) number of links
//
// For each saved link loads:
//  - (int) ndex of the ShapeEditor corresponding to the point (0 or 1)
//  - (int) ShapePoint index
//  - (modulationMode) type of modulation
//  - (float) modulation amount
//
// This method requires knowledge of the addresses of the ShapeEditor instances of the plugin to find the ShapePoints that
// are modulated. The addresses are passed as an array of ShapeEditor* by the parent UDShaper instance.
bool Envelope::loadModulationState(const clap_istream_t *stream, int version[3], ShapeEditor* shapeEditors[2]) {
    if (version[0] == 1 && version[1] == 0 && version[2] == 0) {
        int numberModulationLinks;
        if (stream->read(stream, &numberModulationLinks, sizeof(int)) != sizeof(int)) return false;

        for (int i=0; i<numberModulationLinks; i++) {
            int shapeEditorIdx;
            int pointIdx;
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

// TODO modulation link knobs should be an own class with x, y coordinates. Determining the knob coordinates
// from the knob index and the available space results in some fairly verbose code segments.

EnvelopeManager::EnvelopeManager(uint32_t inXYXY[4], uint32_t inGUIWidth, uint32_t inGUIHeight){
    layout.setCoordinates(inXYXY, inGUIWidth, inGUIHeight);
    activeEnvelopeIndex = 0;

    envelopes.reserve(MAX_NUMBER_ENVELOPES);
    frequencyPanels.reserve(MAX_NUMBER_ENVELOPES);

    // Instantiate with three envelopes.
    addEnvelope();
    addEnvelope();
    addEnvelope();
}

void EnvelopeManager::addEnvelope(){
    if (envelopes.size() == MAX_NUMBER_ENVELOPES){
        return;
    }

    // Add new FrequncyPanel first so it can be referenced in the new Envelope.
    frequencyPanels.emplace_back(layout.toolsXYXY, layout.GUIWidth, layout.GUIHeight);
    envelopes.emplace_back(layout.editorXYXY, layout.GUIWidth, layout.GUIHeight, &frequencyPanels.back(), envelopes.size());
    updateGUIElements = true;
}

void EnvelopeManager::setActiveEnvelope(int index){
    activeEnvelopeIndex = index;

    updateGUIElements = true;
    toolsUpdated = true;
    frequencyPanels.at(activeEnvelopeIndex).setupForRerender();
}

void EnvelopeManager::setupFrames(uint32_t *canvas){
    fillRectangle(canvas, layout.GUIWidth, layout.selectorXYXY, colorBackground);
    draw3DFrame(canvas, layout.GUIWidth, layout.editorInnerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);

    // To draw the selector panel left the the Envelope, first reset the whole area to the background color.
    fillRectangle(canvas, layout.GUIWidth, layout.selectorXYXY, colorBackground);

    // Fill one box at the left of the Envelope in the same color as the Envelope.
    // It indicates which Envelope is currently selected.
    // boxYRange is the Envelope y-range divided by the number of Envelopes.
    float boxYRange = (layout.editorInnerXYXY[3] - layout.editorInnerXYXY[1]) / envelopes.size();
    uint32_t boxXRange = layout.selectorXYXY[2] - layout.selectorXYXY[0];

    uint32_t selectorBox[4] = {
        layout.selectorXYXY[0] + RELATIVE_FRAME_WIDTH*layout.GUIWidth,
        layout.selectorXYXY[1] + static_cast<uint32_t>(activeEnvelopeIndex*boxYRange) + RELATIVE_FRAME_WIDTH*layout.GUIWidth,
        layout.selectorXYXY[2],
        layout.selectorXYXY[1] + static_cast<uint32_t>((activeEnvelopeIndex + 1)*boxYRange) + RELATIVE_FRAME_WIDTH*layout.GUIWidth
    };

    // Rounding errors can lead to a mismatch between the box and the Envelope positions. This is only visible
    // when the last Envelope is active. If the last Envelope is selected, set the lower y-position of the box
    // to match the lower y-position of the Envelope.
    if (activeEnvelopeIndex == envelopes.size() - 1) {
        selectorBox[3] = layout.editorInnerXYXY[3];
    }

    // Draw partial frame around the box.
    drawPartial3DFrame(canvas, layout.GUIWidth, selectorBox, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);

    // Fill the box with the Envelope background color. The box is larger in x-direction to overlap with the
    // frame around the Envelope.
    selectorBox[2] += RELATIVE_FRAME_WIDTH*layout.GUIWidth;
    fillRectangle(canvas, layout.GUIWidth, selectorBox, colorEditorBackground);

    for (int i=0; i<envelopes.size(); i++) {
        std::string envelopeIdx = std::to_string(i);

        // Size and position of the textbox is calculated relative to the selectorBox and takes the width of
        // the frame into account. It is shifted half the frame width to the left by default.
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

    // Draw darker rectangle onto the EnvelopeManager knob panel and add 3D frame.
    fillRectangle(canvas, layout.GUIWidth, layout.knobsInnerXYXY, blendColor(colorBackground, 0xFF000000, 0.1));
    draw3DFrame(canvas, layout.GUIWidth, layout.knobsInnerXYXY, colorEditorBackground, RELATIVE_FRAME_WIDTH*layout.GUIWidth);
}

// Draws the GUI of this EnvelopeManager to canvas. Always calls renderGUI on the active Envelope but renders
// 3D frames, Envelope selection panel and tool panel only if they have been changed.
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
    layout.setCoordinates(newXYXY, newWidth, newHeight);

    for (Envelope &env : envelopes) {
        env.rescaleGUI(layout.editorXYXY, newWidth, newHeight);
    }

    for (FrequencyPanel &panel : frequencyPanels) {
        panel.rescaleGUI(layout.toolsXYXY, newWidth, newHeight);
    }

    updateGUIElements = true;
    toolsUpdated = true;
}

void EnvelopeManager::setupForRerender() {
    frequencyPanels.at(activeEnvelopeIndex).setupForRerender();
}

void EnvelopeManager::drawKnobs(uint32_t *canvas){
    fillRectangle(canvas, layout.GUIWidth, layout.knobsInnerXYXY, blendColor(colorBackground, 0xFF000000, 0.1));

    for (int i=0; i<envelopes[activeEnvelopeIndex].getNumberModulatedParameters(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);
        uint32_t knobXPosition = layout.knobsInnerXYXY[0] + spacing*i + spacing/2;
        uint32_t knobYPosition = static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2);
        float amount = envelopes[activeEnvelopeIndex].getModAmount(i);

        drawLinkKnob(canvas, layout.GUIWidth, knobXPosition, knobYPosition, RELATIVE_LINK_KNOB_SIZE * layout.GUIWidth, amount);
    }
}

void EnvelopeManager::processLeftClick(uint32_t x, uint32_t y){
    clickedX = x;
    clickedY = y;

    // Switch active Envelope when clicked onto the corresponding switch at the selection panel.
    if (isInBox(x, y, layout.selectorXYXY)){
        int newActive = (y - layout.selectorXYXY[1]) * envelopes.size() / (layout.selectorXYXY[3] - layout.selectorXYXY[1]);
        setActiveEnvelope(newActive);

        // After the switch was pressed, assume the following mouse movement to be an attempt to link a
        // parameter to this Envelope.
        currentDraggingMode = addLink;
    }

    // Check if it has been clicked on any of the LinkKnobs and set current dragging mode to moveKnob if true.
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getNumberModulatedParameters(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);
        uint32_t pointXPosition = layout.knobsInnerXYXY[0] + spacing*i + spacing/2;
        uint32_t pointYPosition = static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2);
        if (isInPoint(x, y, pointXPosition, pointYPosition, static_cast<int>(RELATIVE_LINK_KNOB_SIZE*layout.GUIWidth / 2))) {
            selectedKnob = i;
            selectedKnobAmount = envelopes.at(activeEnvelopeIndex).getModAmount(i);
            currentDraggingMode = moveKnob;
        }
    }

    envelopes.at(activeEnvelopeIndex).processLeftClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processLeftClick(x, y);
}

// Forwards double click to the active Envelope.
void EnvelopeManager::processDoubleClick(uint32_t x, uint32_t y){
    envelopes.at(activeEnvelopeIndex).processDoubleClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processDoubleClick(x, y);
}

void EnvelopeManager::processRightClick(uint32_t x, uint32_t y){
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getNumberModulatedParameters(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);
        uint32_t pointXPosition = layout.knobsInnerXYXY[0] + spacing*i + spacing/2;
        uint32_t pointYPosition = static_cast<uint32_t>((layout.knobsXYXY[3] + layout.knobsXYXY[1]) / 2);

        if (isInPoint(x, y, pointXPosition, pointYPosition, static_cast<int>(RELATIVE_LINK_KNOB_SIZE*layout.GUIWidth / 2))){
            selectedKnob = i;
            MenuRequest::sendRequest(this, menuLinkKnob);
            return;
        }
    }
    
    envelopes.at(activeEnvelopeIndex).processRightClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processRightClick(x, y);
}

void EnvelopeManager::highlightHoveredParameter(uint32_t x, uint32_t y) {
    uint32_t knobBox[4]; // Stores coordinates of the box corresponding to a link knob.
    ShapePoint *highlighted = nullptr; // Pointer to the ShapePoint corresponding to the link knob over which the cursor hovers.

    for (int i=0; i<envelopes[activeEnvelopeIndex].getNumberModulatedParameters(); i++){
        uint32_t spacing = static_cast<uint32_t>((layout.knobsInnerXYXY[2] - layout.knobsInnerXYXY[0]) / MAX_NUMBER_LINKS);
        
        knobBox[0] = layout.knobsInnerXYXY[0] + spacing*i;
        knobBox[1] = layout.knobsInnerXYXY[1];
        knobBox[2] = layout.knobsInnerXYXY[0] + spacing*(i+1);
        knobBox[3] = layout.knobsInnerXYXY[1] + spacing;

        ShapePoint *point = const_cast<ShapePoint*>(envelopes.at(activeEnvelopeIndex).modulatedParameters.at(i)->getParentPoint());
        modulationMode mode = envelopes.at(activeEnvelopeIndex).modulatedParameters.at(i)->mode;

        if (isInBox(x, y, knobBox)) {
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
    if (menuType == menuLinkKnob) {
        if (menuItem == removeLink && selectedKnob != -1){
            envelopes.at(activeEnvelopeIndex).removeModulatedParameter(selectedKnob);
            toolsUpdated = true;
        }
    }

    // If it was attempted to modulate a point position, add a ModulatedParameter based on the modulationMode.
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

void EnvelopeManager::processMouseRelease(uint32_t x, uint32_t y){
    currentDraggingMode = envNone;
    envelopes[activeEnvelopeIndex].processMouseRelease(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processMouseRelease(x, y);

    selectedKnob = -1;
}

void EnvelopeManager::processMouseDrag(uint32_t x, uint32_t y){
    envelopes[activeEnvelopeIndex].processMouseDrag(x, y);

    // If a knob is moved, change the amount of modulation of this link.
    if (currentDraggingMode == moveKnob){
        // Add the amount added by moving the mouse to the previous amount the knob was set to.
        float amount = selectedKnobAmount + static_cast<int>(clickedY - y) * KNOB_SENSITIVITY;

        envelopes.at(activeEnvelopeIndex).setModulatedParameterAmount(selectedKnob, amount);

        toolsUpdated = true;

        // TODO a visualization here would be nice, draw less opaque curve that indicates the shape at maximum modulation
    }

    frequencyPanels.at(activeEnvelopeIndex).processMouseDrag(x, y);
}

void EnvelopeManager::addModulatedParameter(ShapePoint *point, float amount, modulationMode mode){
    // Dont modulate point in x-direction, if it is last point.
    // TODO it would be better to not even show the option X in the context menu on attempted modulation, but it is not straightforward with the current implementation. This is easier even though slightly more confusing to the user.
    if ((mode == modPosX) && (point->next == nullptr)) return;

    if (envelopes.at(activeEnvelopeIndex).modulatedParameters.size() == MAX_NUMBER_LINKS) return;

    envelopes.at(activeEnvelopeIndex).addModulatedParameter(point, amount, mode, activeEnvelopeIndex);
    toolsUpdated = true;
}

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
// This method requires knowledge of the addresses of the ShapeEditor instances of the plugin in order to recover the
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
