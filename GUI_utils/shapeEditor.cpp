#include "shapeEditor.h"
#include "assets.h"

/*Returns the power p of a curve f(x) = x^p that goes through yCenter at x=0.5. User is able to deform curves by dragging the curve center. This function is used to get the power of the curve from the mouse position.
If yCenter > 0.5, the power is negative, which does NOT mean f(x)=1/(x^power), but that the curve is flipped f(x)=1-(1-x)^power. This convention is always used for shapes with mode shapePower.*/
float getPowerFromYCenter(float yCenter){
    float power = (yCenter < 0.5) ? log(yCenter) / log(0.5) :  - log(1 - yCenter) / log(0.5);
    power = (power < 0) ? ((power < -SHAPE_MAX_POWER) ? -SHAPE_MAX_POWER : power) : ((power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power);
    return power;
}


/* Class to store all information about a curve Segment. Curve segments are handled in a way that information about the function (e.g. type of interpolation and parameters) are stored together with the point next to the right, which marks the beginning of the next segment.
The reason for that is that curve has to always satisfy f(0) = 0 or else the plugin would generate a DC offset, so the left hand side of the first curve segment must stay at [0, 0] and can not be moved.*/
class ShapePoint{
    public:
    int32_t XYXY[4]; // Position and size of parent shapeEditor. Point must stay inside these coordinates.

    // Parameters of the point:

    float posX; // Relative x-position on the graph, 0 <= posX <= 1.
    float posY; // Relative y-position on the graph, 0 <= posY <= 1.
    float posXMod; // Relative x-position on the graph, modulated value.
    float posYMod; // Relative y-position on the graph, modulated value.
    int32_t absPosX; // Absolute x-position on the window.
    int32_t absPosY; // Absolute y-position on the window. By convention it is inversely related to the relative position.
    int32_t	absPosXMod; // Absolute x-position on the window, modulated value.
    int32_t absPosYMod; // Absolute y-position on the window, modulated value.

    ShapePoint *previous = nullptr; // Pointer to the previous ShapePont.
    ShapePoint *next = nullptr; // Pointer to the next ShapePoint.

    // Parameters of the curve left to the point:

    float curveCenterPosY; // Relative y-value of the curve at the x-center point between this and the previous point.
    float curveCenterPosYMod; // Relative y-value of the curve at the x-center point between this and the previous point, modulated value.
    uint32_t curveCenterAbsPosX; // Absolute x-center between this and the previous point.
    uint32_t curveCenterAbsPosY; // Absolute y-position of the curve at the x-center point between this and the previous point.
    uint32_t curveCenterAbsPosYMod; // Absolute y-position of the curve at the x-center point between this and the previous point, modulated value.
    float power; // Exponent for the shapePower interpolation mode. Can be negative, which does NOT mean the curve is actually f(x) = 1 / (x^|power|) but that the curve is flipped vertically and horizontally
    float sineOmegaPrevious; // Omega after last update.
    float sineOmega; // Omega for the shapeSine interpolation mode.
    Shapes mode = shapePower; // Interpolation mode between this and previous point.

    /*x, y are relative positions on the graph editor, e.g. they must be between 0 and 1
    editorSize is screen coordinates of parent shapeEditor in XYXY notation: {xMin, yMin, xMax, yMax}
    previousPoint is pointer to the next ShapePoint on the left hand side of the current point. If there is no point to the left, should be nullptr
    pow is parameter defining the shape of the curve segment, its role is dependent on the mode:
        shapePower: f(x) = x^parameter
    */
    ShapePoint(float x, float y, uint32_t editorSize[4], float pow = 1, float omega = 0.5, Shapes initMode = shapePower){
        assert ((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));

        // x and y are relative coordinates on the Graph 0 <= x <= 1, 0 <= y <= 1
        posX = x;
        posY = y;

        posXMod = posX;
        posYMod = posY;

        // in absolute coordinates y is mirrored
        absPosX = editorSize[0] + (int32_t)(x * (editorSize[2] - editorSize[0]));
        absPosY = editorSize[3] - (int32_t)(y * (editorSize[3] - editorSize[1]));

        absPosXMod = absPosX;
        absPosYMod = absPosY;

        curveCenterPosY = 0.5;
        curveCenterAbsPosX = (absPosX + ((previous == nullptr) ? editorSize[0] : previous->absPosX)) / 2;
        curveCenterAbsPosY = (absPosY + ((previous == nullptr) ? editorSize[3] : previous->absPosY)) / 2;

        for (int i=0; i<4; i++){
            XYXY[i] = editorSize[i];
        }

        power = pow;
        mode = initMode;
        sineOmega = omega;
        sineOmegaPrevious = omega;
    }

    // Sets relative posXMod and posYMod to x and y respectively, sets absolute positions absPosX and absPosY accordingly and updates the curve center.
    void modulatePositionRelative(float x, float y){
        // clamp to interval [0, 1]
        posXMod = x < 0 ? 0 : x > 1 ? 1 : x;
        posYMod = y < 0 ? 0 : y > 1 ? 1 : y;

        absPosXMod = XYXY[0] + (int32_t)(posXMod * (XYXY[2] - XYXY[0]));
        absPosYMod = XYXY[3] - (int32_t)(posYMod * (XYXY[3] - XYXY[1]));

        updateCurveCenter();
    }

    // Sets absolute absPosX and absPosY to x and y respectively, sets relative positions posX and posY accordingly and updates the curve center.
    void updatePositionAbsolute(int32_t x, int32_t y){
        // clamp to editor size
        absPosX = x < XYXY[0] ? XYXY[0] : x > XYXY[2] ? XYXY[2] : x;
        absPosY = y < XYXY[1] ? XYXY[1] : y > XYXY[3] ? XYXY[3] : y;

        posX = (float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]);
        posY = (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]);

        absPosXMod = absPosX;
        absPosYMod = absPosY;

        posXMod = posX;
        posYMod = posY;

        updateCurveCenter();
    }

    // updates the Curve center point in cases where curveCenter is not directly changed, as for example if the point is being moved.
    void updateCurveCenter(){
        // curve center x is always in the middle of neighbouring points
        curveCenterAbsPosX = (int32_t)(absPosXMod + previous->absPosXMod) / 2;

        if (mode == shapePower){
            int32_t yL = previous->absPosYMod;
            curveCenterAbsPosY = (power > 0) ? yL - pow(0.5, power) * (yL - absPosYMod) : absPosYMod + pow(0.5, -power) * (yL - absPosYMod);
            // curveCenterAbsPosY = yL + curveCenterPosY * (yL - absPosY)
        } else if (mode == shapeSine){
            curveCenterAbsPosY = (int32_t)(absPosYMod + previous->absPosYMod) / 2;
        }
    }

    // updates the Curve center point when manually dragging it
    void updateCurveCenter(int x, int y){
        // nothing to update if line is horizontal
        if (previous->absPosY == absPosY){
            return;
        }

        switch (mode){
            case shapePower:
            {
                int32_t yL = previous->absPosY;

                int32_t yMin = (yL > absPosY) ? absPosY : yL;
                int32_t yMax = (yL < absPosY) ? absPosY : yL;

                // y must not be yL or posY, else there will be a log(0). set y to be at least one pixel off of these values
                y = (y >= yMax) ? yMax - 1 : (y <= yMin) ? yMin + 1 : y;

                // find mouse position normalized to y extent
                curveCenterPosY = (yL - (float)y) / (yL - absPosY);
                power = getPowerFromYCenter(curveCenterPosY);
                // find pixel coordinates corresponding to normalized mouse position
                curveCenterAbsPosY = (power > 0) ? yL - pow(0.5, power) * (yL - absPosY) : absPosY + pow(0.5, -power) * (yL - absPosY);
                break;
            }

            case shapeSine:
            {
                /*For shapeSine the center point stays at the same position, while the sineOmega value is still updated when moving the mouse in y-direction.
                Sine wave will always go through this point. When omega is smaller 0.5, the sine will smoothly connect the points, if larger 0.5,
                it will be increased in discrete steps, such that always n + 1/2 cycles lay in the interval between the points.*/

                curveCenterAbsPosX = (int32_t)(absPosX + previous->absPosX) / 2;
                curveCenterAbsPosY = (int32_t)(absPosY + previous->absPosY) / 2;

                if (sineOmega <= 0.5){
                    sineOmega = sineOmegaPrevious * pow(0.5, (float)(y - curveCenterAbsPosY) / 50);
                    sineOmega = (sineOmega < SHAPE_MIN_OMEGA) ? SHAPE_MIN_OMEGA : sineOmega;
                } else {
                    // TODO if shift or strg or smth is pressed, be continuous
                    sineOmega = sineOmegaPrevious + ((curveCenterAbsPosY - y) / 40);
                    sineOmega -= (int32_t)sineOmega % 2;
                }
                break;
            }
            case shapeBezier:
            break;
        }
    }

    void processMouseClick(){
        sineOmegaPrevious = sineOmega;
    }
};

// Deletes the given ShapePoint and connects this points previous and next point in the list.
void deleteShapePoint(ShapePoint *point){
    if (point->previous == nullptr){
        throw std::invalid_argument("Tried to delete the first shapePoint which can not be deleted.");
    }
    if (point->next == nullptr){
        throw std::invalid_argument("Tried to delete the last shapePoint which can not be deleted.");
    }

    ShapePoint *previous = point->previous;
    ShapePoint *next = point->next;

    previous->next = next;
    next->previous = previous;

    delete point;
}

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

ShapeEditor::ShapeEditor(uint32_t position[4]){

    for (int i=0; i<4; i++){
        XYXYFull[i] = position[i];
    }

    XYXY[0] = position[0] + FRAME_WIDTH;
    XYXY[1] = position[1] + FRAME_WIDTH;
    XYXY[2] = position[2] - FRAME_WIDTH;
    XYXY[3] = position[3] - FRAME_WIDTH;

    shapePoints = new ShapePoint(0., 0., XYXY);
    shapePoints->next = new ShapePoint(1., 1., XYXY);
    shapePoints->next->previous = shapePoints;
}

void ShapeEditor::drawGraph(uint32_t *bits){
    // set whole area to background color
    for (uint32_t i = XYXYFull[0]; i < XYXYFull[2]; i++) {
        for (uint32_t j = XYXYFull[1]; j < XYXYFull[3]; j++) {
            bits[i + j * GUI_WIDTH] = colorEditorBackground;
        }
    }

    drawGrid(bits, XYXY, 3, 2, 0xFFFFFF, alphaGrid);
    drawFrame(bits, XYXY, 3, 0xFFFFFF);

    // first draw all connections between points, then points on top
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawConnection(bits, point, colorCurve);
    }
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawPoint(bits, point->absPosXMod, point->absPosYMod, colorCurve, 20);
        drawPoint(bits, point->curveCenterAbsPosX, point->curveCenterAbsPosY, colorCurve, 16);
    }
}

// Finds the closest ShapePoint or curve center point. Returns pointer to the corresponding point if it is closer than the squareroot of REQUIRED_SQUARED_DISTANCE, else nullptr. The field currentDraggingMode of this instance is set to either position or curveCenter.
ShapePoint* ShapeEditor::getClosestPoint(uint32_t x, uint32_t y, float minimumDistance){
    // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.
    //   This would require every point to check mouse position at any time
    float closestDistance = 10E6;
    ShapePoint *closestPoint;

    // check mouse distance to all points and find closest
    float distance;
    // start with second point in linked list because first point is treated special, see comments at shapePoints declaration
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        // check for distance to point and set mode to position
        distance = (point->absPosX - x)*(point->absPosX - x) + (point->absPosY - y)*(point->absPosY - y);
        if (distance < closestDistance){
            closestDistance = distance;
            closestPoint = point;
            currentDraggingMode = position;
        }
        // check for distance to curve center and set mode to curveCenter
        distance = (point->curveCenterAbsPosX - x)*(point->curveCenterAbsPosX - x) + (point->curveCenterAbsPosY - y)*(point->curveCenterAbsPosY - y);
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

void ShapeEditor::processMouseClick(int32_t x, int32_t y){

    // check if mouse hovers over the graphical interface and return if not
    // check for slightly higher area to be able to grab point from outside the interface
    float offset = pow(REQUIRED_SQUARED_DISTANCE, 0.5);
    if ((x < XYXY[0] - offset) | (x >= XYXY[2] + offset) | (y < XYXY[1] - offset) | (y >= XYXY[3] + offset)){
        return;
    }

    // Get time since last click and substract now. LastMousePress will be updated on Mouse release.
    // long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    // long timePassed = now - lastMousePress;
    // lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    // if in proximity to point mark point as currentlyDragging
    if (closestPoint != nullptr){
        closestPoint->processMouseClick();
        currentlyDragging = closestPoint;
    }
}

// Processes double click. If a ShapePoint was deleted by the double click, a pointer to this ShapePoint is returned. If no point has been deleted, nullptr is returned.
ShapePoint* ShapeEditor::processDoubleClick(uint32_t x, uint32_t y){
    if ((x < XYXY[0]) | (x >= XYXY[2]) | (y < XYXY[1]) | (y >= XYXY[3])){
        return nullptr;
    }

    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    if (closestPoint != nullptr){
        // if close to point and point is not last, which cannot be removed, remmove point
        if (currentDraggingMode == position && closestPoint->next != nullptr){
            ShapePoint *nextPoint = closestPoint->next;
            deleteShapePoint(closestPoint);
            nextPoint->updateCurveCenter();
            return closestPoint;
        }

        // if double click on curve center, reset power
        else if (currentDraggingMode == curveCenter){
            if (closestPoint->mode == shapePower){
                closestPoint->power = 1;
            } else if (closestPoint->mode == shapeSine){
                closestPoint->sineOmega = 0.5;
                closestPoint->sineOmegaPrevious = 0.5;
            }
            closestPoint->updateCurveCenter();
            return nullptr;
        }
    }

    // if not in proximity to point, add one
    if (closestPoint == nullptr){
        // points in shapePoints should be sorted by x-position, find correct index to insert
        uint32_t insertIdx = 0;
        ShapePoint *insertBefore = shapePoints->next;
        while (insertBefore != nullptr){
            if (insertBefore->absPosX >= x){
                break;
            }
            insertBefore = insertBefore->next;
        }

        insertPointBefore(insertBefore, new ShapePoint((float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]), (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]), XYXY));
        insertBefore->updateCurveCenter();
        insertBefore->previous->updateCurveCenter();
    }
    return nullptr;
}

// Process right click and return true if a shapePoint was clicked.
contextMenuType ShapeEditor::processRightClick(uint32_t x, uint32_t y){
    ShapePoint *closestPoint;
    closestPoint = getClosestPoint(x, y);

    // if rightclick on point, show shape menu to change function of curve segment
    if (closestPoint != nullptr && currentDraggingMode == position){
        rightClicked = closestPoint;
        return menuShapePoint;
    }
    // if right click on curve center, reset power
    else if (closestPoint != nullptr && currentDraggingMode == curveCenter){
        if (closestPoint->mode == shapePower){
            closestPoint->power = 1;
        } else if (closestPoint->mode == shapeSine){
            closestPoint->sineOmega = 0.5;
            closestPoint->sineOmegaPrevious = 0.5;
        }
        closestPoint->updateCurveCenter();
    }
    return menuNone;
}

void ShapeEditor::processMenuSelection(WPARAM wParam){
    if (rightClicked == nullptr){
        return;
    }

    switch (wParam) {
        case shapePower:
            rightClicked->mode = shapePower;
            break;
        case shapeSine:
            rightClicked->mode = shapeSine;
            break;
        case shapeBezier:
            rightClicked->mode = shapeBezier;
            break;
    }
    rightClicked->updateCurveCenter();
    rightClicked = nullptr;
}

// Reset currentlyDragging to nullptr and currentDraggingMode to none.
void ShapeEditor::processMouseRelease(){
    currentlyDragging = nullptr;
    currentDraggingMode = none;
}

/* TODO does it make sense to have a single function that transforms values according to the shape of all curve segments? This function could be used to transform x-pixel coordinates to get the y value and also to actually shape the input sound.*/
void ShapeEditor::drawConnection(uint32_t *bits, ShapePoint *point, uint32_t color, float thickness){

    int xMin = point->previous->absPosXMod;
    int xMax = point->absPosXMod;

    int yL = point->previous->absPosYMod;
    int yR = point->absPosYMod;

    switch (point->mode){
        case shapePower:
        {
            // The curve is f(x) = x^power, where x is in [0, 1] and x and y intervals are stretched such that the curve connects this and previous point
            for (int i = xMin; i < xMax; i++) {
                // TODO: antialiasing/ width of curve
                if (point->power > 0){
                    bits[i + (int)((yL - pow((float)(i - xMin) / (xMax - xMin), point->power) * (yL - yR))) * GUI_WIDTH] = color;
                } else {
                    bits[i + (int)((yR + pow((float)(xMax - i) / (xMax - xMin), -point->power) * (yL - yR))) * GUI_WIDTH] = color;
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
                bits[i + (int)(ampCorrection * sin((float)(i - xMin - (xMax - xMin)/2) / (xMax - xMin) * point->sineOmega * 2 * pi) * (yR - yL) / 2 - (yL - yR)/2 + yL) * GUI_WIDTH] = color;
            }
            break;
        }
        case shapeBezier:
        break;
    }
}

void ShapeEditor::processMouseDrag(int32_t x, int32_t y){
    if (!currentlyDragging){
        return;
    }

    if (currentDraggingMode == position){
        // point is not allowed to go beyond neighbouring points in x-direction or below 0 or above 1 if it first or last point.
        int32_t xLowerLim;
        int32_t xUpperLim;

        // The rightmost point must not move in x-direction. If point is last point set both limits to only allowed value, else choose positions of neighbouring points.
        xLowerLim = currentlyDragging->next == nullptr ? XYXY[2] : currentlyDragging->previous->absPosX;
        xUpperLim = currentlyDragging->next == nullptr ? XYXY[2] : currentlyDragging->next->absPosX;

        x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
        y = (y > XYXY[3]) ? XYXY[3] : (y < XYXY[1]) ? XYXY[1] : y;

        // the rightmost point must not move in x-direction
        if (currentlyDragging->next == nullptr){
            x = XYXY[2];
        }

        currentlyDragging->updatePositionAbsolute(x, y);

        if (currentlyDragging->next != nullptr){
            currentlyDragging->next->updateCurveCenter();
        }
    }

    else if (currentDraggingMode == curveCenter){
        currentlyDragging->updateCurveCenter(x, y);
    }
}

// Returns the value of the curve defined by this shapeEditor at the x position input. Input is clamped to [0, 1].
float ShapeEditor::forward(float input){
    // Catching this case is important because due to quantization error, the function might return non-zero values for steep curves, which would result in an DC offset even when no input audio is given.
    if (input == 0){
        return 0;
    }
    input = input < 0 ? 0 : input > 1 ? 1 : input;

    float out;
    // absolute value of input is processed, store information to flip sign after computing if necessary
    bool negativeInput = input < 0;
    input = (input < 0) ? -input : input;
    // limit input to one
    input = (input > 1) ? 1 : input;

    ShapePoint *point = shapePoints->next;

    while(point->posXMod < input){
        point = point->next;
    }

    switch(point->mode){
        case shapePower:
        {
            float xL = point->previous->posXMod;
            float yL = point->previous->posYMod;

            float relX = (point->posXMod == xL) ? xL : (input - xL) / (point->posXMod - xL);
            float factor = (point->posYMod - yL);

            float power = point->power;

            power = power < -SHAPE_MAX_POWER ? -SHAPE_MAX_POWER : power > SHAPE_MAX_POWER ? SHAPE_MAX_POWER : power;

            out = (power > 0) ? yL + pow(relX, power) * factor : point->posY - pow(1-relX, -power) * factor;
        }
        case shapeSine:
        break;

        case shapeBezier:
        break;
    }
    return negativeInput ? -out : out;
}


LinkedParameter::LinkedParameter(ShapePoint *inPoint, float inAmount, modulationMode inMode){
    point = inPoint;
    amount = inAmount;
    amountDragged = inAmount;
    mode = inMode;
}

// Adds modOffset to the linked parameter.
void LinkedParameter::modulate(float modOffset){
    switch (mode){
        case modCurveCenterY:
        {
            // TODO this works but it is not clear whats happening at all
            float yL = point->previous->absPosY;
            float yR = point->absPosY;

            int32_t yMin = (yL > yR) ? yR : yL;
            int32_t yMax = (yL < yR) ? yR : yL;

            float newY = point->curveCenterPosYMod + amount * modOffset;
            newY = (newY >= 1) ? 1 - (float)1/(yMax - yMin) : (newY <= 0) ? (float)1/(yMax - yMin) : newY;

            point->curveCenterPosYMod = newY;
            point->curveCenterAbsPosYMod = yL + newY * (yL - yR);
            point->power = getPowerFromYCenter(newY);
            point->updateCurveCenter();
            break;
        }
        case modPosY:
        {
            point->modulatePositionRelative(point->posXMod, point->posYMod + amount * modOffset);
            point->updateCurveCenter();
            break;
        }
    }
}

void LinkedParameter::setAmount(float offset){
    // Add offset to default value and clamp to boundaries
    amountDragged = amount + offset;
    amountDragged = amountDragged > 1 ? 1 : amountDragged < -1 ? -1 : amountDragged;
}

// Get current modulation amount.
float LinkedParameter::getAmount(){
    return amountDragged;
}

// Returns a pointer to the linked ShapePoint.
ShapePoint *LinkedParameter::getLinkedPoint(){
    return point;
}

// If amount has been changed, saves the changed value as default.
void LinkedParameter::processMouseRelease(){
    amount = amountDragged;
}

// Sets all modulated values to their default unmodulated value.
void LinkedParameter::reset(){
    switch (mode){
        case modCurveCenterY:
        {
            float yL = point->previous->absPosY;
            float yR = point->absPosY;

            point->curveCenterPosYMod = point->curveCenterPosY;
            point->curveCenterAbsPosYMod = yL + point->curveCenterPosY * (yL - yR);
            point->power = getPowerFromYCenter(point->curveCenterPosY);
            point->updateCurveCenter();
            break;
        }
        case modPosY:
        {
            point->modulatePositionRelative(point->posX, point->posY);
            point->updateCurveCenter();
            break;
        }
    }
}

Envelope::Envelope(uint32_t size[4]) : ShapeEditor(size) {};

void Envelope::addLinkedParameter(ShapePoint *point, float amount, modulationMode mode){
    linkedParameters.push_back(LinkedParameter(point, amount, mode));
}

// Sets the amount of the LinkedParameter at index to the given amount
void Envelope::setLinkedParameterAmount(int index, float amount){
    assert (index < linkedParameters.size());
    linkedParameters.at(index).setAmount(amount);
}

void Envelope::removeLinkedParameter(int index){
    linkedParameters.erase(linkedParameters.begin() + index);
}

int Envelope::getLinkedParameterNumber(){
    return linkedParameters.size();
}

float Envelope::getModAmount(int index){
    return linkedParameters.at(index).getAmount();
}

void Envelope::modulateLinkedParameters(double beatPosition, double sampleTimeOffset){
    float modOffset = forward((beatPosition + sampleTimeOffset) / 4);

    for (LinkedParameter parameter : linkedParameters){
        parameter.modulate(modOffset);
    }
}

void Envelope::resetLinkedParameters(){
    for (LinkedParameter parameter : linkedParameters){
        parameter.reset();
    }
}

// Reset currentlyDragging to nullptr and currentDraggingMode to none. Processes changes in knob positions for every LinkedParameter
void Envelope::processMouseRelease(){
    for (int i=0; i<linkedParameters.size(); i++){
        linkedParameters.at(i).processMouseRelease();
    }
    currentlyDragging = nullptr;
    currentDraggingMode = none;
}

// TODO there should be area from which a connection can be dragged to the corresponding parameter to be modulated.
// void Envelope::processMousePressMod(int32_t x, int32_t y);

void Envelope::processRightClickMod(int32_t x, int32_t y){
    if ((x < XYXY[0]) | (x > XYXY[2]) | (y < XYXY[3]) | (y > XYXY[1])){
        return;
    }

}

EnvelopeManager::EnvelopeManager(uint32_t inXYXY[4]){
    for (int i=0; i<4; i++){
        XYXY[i] = inXYXY[i];
    }

    // 10% on left and bottom are reserved for other GUI elements, rest is for Envelopes
    uint32_t width = XYXY[2] - XYXY[0];
    uint32_t height = XYXY[3] - XYXY[1];

    envelopeXYXY[0] = (uint32_t)(XYXY[0] + 0.1*width);
    envelopeXYXY[1] = XYXY[1];
    envelopeXYXY[2] = XYXY[2];
    envelopeXYXY[3] = (uint32_t)(XYXY[3] - 0.2*height);

    selectorXYXY[0] = XYXY[0];
    selectorXYXY[1] = XYXY[1];
    selectorXYXY[2] = (uint32_t)(XYXY[0] + 0.1*width);
    selectorXYXY[3] = (uint32_t)(XYXY[3] - 0.2*height);

    // For y-position, take width of the 3D frame around the active Envelope into account.
    toolsXYXY[0] = (uint32_t)(XYXY[0] + 0.1*width);
    toolsXYXY[1] = (uint32_t)(XYXY[3] - 0.2*height) + FRAME_WIDTH;
    toolsXYXY[2] = XYXY[2];
    toolsXYXY[3] = XYXY[3];

    activeEnvelopeIndex = 0;

    // initiate with three envelopes
    addEnvelope();
    addEnvelope();
    addEnvelope();
}

// Adds a new Envelope to the back of envelopes.
void EnvelopeManager::addEnvelope(){
    envelopes.push_back(Envelope(envelopeXYXY));
    updateGUIElements = true;
}

// Sets the Envelope at index as active and updates the GUI accordingly.
void EnvelopeManager::setActiveEnvelope(int index){
    activeEnvelopeIndex = index;

    updateGUIElements = true;
    toolsUpdated = true;
}

// Returns a pointer to the active Envelope object.
Envelope *EnvelopeManager::getActiveEnvelope(){
    return &envelopes.at(activeEnvelopeIndex);
}

// Draw the initial 3D frames around the displayed Envelope. Only has to be called when creating the GUI or the GUI contents have been changed by an user input.
void EnvelopeManager::setupFrames(uint32_t *canvas){

    for (int y=selectorXYXY[1]; y<selectorXYXY[3]; y++){
        for (int x=selectorXYXY[0]; x<selectorXYXY[2]; x++){
            canvas[x + y * GUI_WIDTH] = colorBackground;
        }
    }

    draw3DFrame(canvas, envelopeXYXY, colorEditorBackground);

    // Fill one rectangle at the left of the Envelop in the same color as the Envelope. y-range is the Envelope y-range divided by the number of Envelopes.
    uint32_t rectYRange = (uint32_t)(envelopeXYXY[3] - envelopeXYXY[1]) / envelopes.size();
    for (uint32_t y = XYXY[1] + activeEnvelopeIndex*rectYRange; y<XYXY[1] + (activeEnvelopeIndex + 1)*rectYRange; y++){
        for (uint32_t x = XYXY[0]; x<=envelopeXYXY[0]; x++){
            canvas[x + y * GUI_WIDTH] = colorEditorBackground;
        }
    }
}

// Draws the GUI of this EnvelopeManager to canvas. Always calls drawGraph on the active Envelope but renders 3D frames, Envelope selection panel and tool panel only if they have been changed.
void EnvelopeManager::renderGUI(uint32_t *canvas){
    envelopes.at(activeEnvelopeIndex).drawGraph(canvas);

    if (updateGUIElements){
        setupFrames(canvas);
    }

    if (toolsUpdated){
        drawKnobs(canvas);
        toolsUpdated = false;
    }
}

// Draws the knobs for the LinkedParameters of the active Envelope to the tool panel. Knobs are positioned next to each other sorted by their index in the linkedParameters vector of the parent Envelope.
void EnvelopeManager::drawKnobs(uint32_t *canvas){
    // first reset whole area
    fillRectangle(canvas, toolsXYXY);

    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getLinkedParameterNumber(); i++){
        float amount = envelopes.at(activeEnvelopeIndex).getModAmount(i); // modulation amount of the ith LinkedParameter
        drawLinkKnob(canvas, toolsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, toolsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE, amount);
    }
}

void EnvelopeManager::processMouseClick(uint32_t x, uint32_t y){
    clickedX = x;
    clickedY = y;

    // switch active Envelope when clicked onto the corresponding switch at the selection panel
    if (isInBox(x, y, selectorXYXY)){
        int newActive = (y - selectorXYXY[1]) * envelopes.size() / (selectorXYXY[3] - selectorXYXY[1]);
        setActiveEnvelope(newActive);

        // After the switch was pressed, assume the following mouse movement to be an attempt to link a parameter to this Envelope:
        currentDraggingMode = addLink;
    }

    // check if it has been clicked on any of the LinkKnobs and set currentdragging mode to moveKnob if true
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getLinkedParameterNumber(); i++){
        if (isInPoint(x, y, toolsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, toolsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE)){
            selectedKnob = i;
            currentDraggingMode = moveKnob;
        }
    }

    // always forward input to active Envelope
    envelopes.at(activeEnvelopeIndex).processMouseClick(x, y);
}

void EnvelopeManager::processDoubleClick(uint32_t x, uint32_t y){
    envelopes.at(activeEnvelopeIndex).processDoubleClick(x, y);
}

contextMenuType EnvelopeManager::processRightClick(uint32_t x, uint32_t y){
    // check all knobs if they have been rightclicked
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getLinkedParameterNumber(); i++){
        if (isInPoint(x, y, toolsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, toolsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE)){
            selectedKnob = i;
            return menuLinkKnob;
        }
    }
    
    return envelopes.at(activeEnvelopeIndex).processRightClick(x, y);
}

void EnvelopeManager::processMenuSelection(WPARAM wParam){
    if (wParam == removeLink && selectedKnob != -1){
        envelopes.at(activeEnvelopeIndex).removeLinkedParameter(selectedKnob);
        toolsUpdated = true;
    }

    for (Envelope envelope : envelopes){
        envelope.processMenuSelection(wParam);
    }
}

// Resets currentDraggingMode, draggedToX, draggedToY and selectedKnob. Calls processMouseRelease() on the active Envelope.
void EnvelopeManager::processMouseRelease(){
    currentDraggingMode = envNone;
    envelopes.at(activeEnvelopeIndex).processMouseRelease();

    // reset dragging position to arbitrary value which is far from any points, so it will not accidentally connect to previously selected point 
    draggedToX = 0;
    draggedToY = 0;

    selectedKnob = -1;
}

void EnvelopeManager::processMouseDrag(uint32_t x, uint32_t y){
    envelopes.at(activeEnvelopeIndex).processMouseDrag(x, y);

    // If the user is trying to add a link, keep track of the mouse position in draggedToX and draggedToY.
    if (currentDraggingMode == addLink){
        draggedToX = x;
        draggedToY = y;
    }
    // If a knob is moved, change the amount of modulation of this link.
    else if (currentDraggingMode == moveKnob){
        float amount = (int)(clickedY - y) * KNOB_SENSITIVITY;
        envelopes.at(activeEnvelopeIndex).setLinkedParameterAmount(selectedKnob, amount);

        // Knob position has changed, so rerender them on the GUI
        toolsUpdated = true;

        // TODO a visualization here would be nice, draw less opaque curve that indicates the shape at maximum modulation
    }

}

// Adds the given point and mode as a ModulatedParameter to the active Envelope.
void EnvelopeManager::addLinkedParameter(ShapePoint *point, float amount, modulationMode mode){
    envelopes.at(activeEnvelopeIndex).addLinkedParameter(point, amount, mode);
    toolsUpdated = true;
}

// Removes all links to the given ShapePoint. Must be called when a ShapePoint is deleted to avoid dangling pointers.
void EnvelopeManager::clearLinksToPoint(ShapePoint *point){
    if (point == nullptr){
        return;
    }

    for (int i=0; i<envelopes.size(); i++){
        // Loop backwards through the linkedParameters vector to not mess up the indicex when removing elements.
        for (int j=envelopes.at(i).linkedParameters.size()-1; j>=0; j--){
            if (envelopes.at(i).linkedParameters.at(j).getLinkedPoint() == point){
                envelopes.at(i).removeLinkedParameter(j);
            }
        }
    }
    toolsUpdated = true;
}

void EnvelopeManager::modulateLinkedParameters(double beatPosition, double sampleTimeOffset){
    for (int i=0; i<envelopes.size(); i++){
        envelopes.at(i).modulateLinkedParameters(beatPosition, sampleTimeOffset);
    }
}

void EnvelopeManager::resetLinkedParameters(){
    for (int i=0; i<envelopes.size(); i++){
        envelopes.at(i).resetLinkedParameters();
    }
}
