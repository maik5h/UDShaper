#include "shapeEditor.h"

#include <fstream>
#include <iostream>
void logToFile2(const std::string& message) {
    std::ofstream logFile("C:/Users/mm/Desktop/log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    } else {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}

// Claculates the power of the curve for the interpolation mode power from the y value at x=0.5.
// Can be negative, which does NOT mean the curve is actually f(x) = 1 / (x^|power|) but that the curve is flipped vertically and horizontally.
float getPowerFromPosY(float posY){
    // Calculates the power p of a curve f(x) = x^p that goes through yCenter at x=0.5:
    float power = (posY < 0.5) ? log(posY) / log(0.5) :  - log(1 - posY) / log(0.5);

    // Clamp to allowed maximum power
    power = (power < 0) ? ((power < -SHAPE_MAX_POWER) ? -SHAPE_MAX_POWER : power) : ((power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power);
        
    return power;
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
    ShapePoint *parent;

    std::map<Envelope*, float> modulationAmounts; // Maps the pointer to an Envelope to its scaling factor for modulation.

    public:
    ModulatedParameter(ShapePoint *parentPoint, float inBase, float inMinValue=-1, float inMaxValue=1){
        base = inBase;
        minValue = inMinValue;
        maxValue = inMaxValue;
        parent = parentPoint;
    }

    // Registeres the given envelope as a modulator to this parameter. This Envelope will now contribute to the modulation when calling .get() with the given amount.
    void addModulator(Envelope *envelope, float amount){
        // Envelopes can only be added once.
        for (auto a : modulationAmounts){
            if (a.first == envelope){
                return;
            }
        }

        modulationAmounts[envelope] = amount;
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

    // Return a pointer to the ShapePoint that uses this ModulatedParameter instance.
    ShapePoint *getParentPoint(){
        return parent;
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

    /*x, y are relative positions on the graph editor, e.g. they must be between 0 and 1
    editorSize is screen coordinates of parent shapeEditor in XYXY notation: {xMin, yMin, xMax, yMax}
    previousPoint is pointer to the next ShapePoint on the left hand side of the current point. If there is no point to the left, should be nullptr
    pow is parameter defining the shape of the curve segment, its role is dependent on the mode:
        shapePower: f(x) = x^parameter
    */
    ShapePoint(float x, float y, uint32_t editorSize[4], float pow = 1, float omega = 0.5, Shapes initMode = shapePower): posX(this, x), posY(this, y), curveCenterPosY(this, 0.5) {
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

ShapeEditor::ShapeEditor(uint32_t position[4]){

    for (int i=0; i<4; i++){
        XYXYFull[i] = position[i];
    }

    XYXY[0] = position[0] + FRAME_WIDTH;
    XYXY[1] = position[1] + FRAME_WIDTH;
    XYXY[2] = position[2] - FRAME_WIDTH;
    XYXY[3] = position[3] - FRAME_WIDTH;

    // Set up first and last point and link them. These points will always stay first and last point.
    shapePoints = new ShapePoint(0., 0., XYXY);
    shapePoints->next = new ShapePoint(1., 1., XYXY);
    shapePoints->next->previous = shapePoints;
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
    if ((x < XYXY[0] - offset) | (x >= XYXY[2] + offset) | (y < XYXY[1] - offset) | (y >= XYXY[3] + offset)){
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
    if ((x < XYXY[0]) | (x >= XYXY[2]) | (y < XYXY[1]) | (y >= XYXY[3])){
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

        insertPointBefore(insertBefore, new ShapePoint((float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]), (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]), XYXY));
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
        menuRequest = menuShapePoint;
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
    menuRequest = menuNone;
}

void ShapeEditor::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed){
    // set whole area to background color
    for (uint32_t i = XYXYFull[0]; i < XYXYFull[2]; i++) {
        for (uint32_t j = XYXYFull[1]; j < XYXYFull[3]; j++) {
            canvas[i + j * GUI_WIDTH] = colorEditorBackground;
        }
    }

    drawGrid(canvas, XYXY, 3, 2, 0xFFFFFF, alphaGrid);
    drawFrame(canvas, XYXY, 3, 0xFFFFFF);

    // first draw all connections between points, then points on top
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawConnection(canvas, point, beatPosition, secondsPlayed, colorCurve);
    }
    for (ShapePoint *point = shapePoints->next; point != nullptr; point = point->next){
        drawPoint(canvas, point->getAbsPosX(beatPosition, secondsPlayed), point->getAbsPosY(beatPosition, secondsPlayed), colorCurve, 20);
        drawPoint(canvas, point->getCurveCenterAbsPosX(beatPosition, secondsPlayed), point->getCurveCenterAbsPosY(beatPosition, secondsPlayed), colorCurve, 16);
    }
}

// Returns the type of menu requested by the ShapeEditor to open. The type is reset to menuNone when called.
contextMenuType ShapeEditor::getMenuRequestType() {
    contextMenuType requested = menuRequest;
    menuRequest = menuNone;
    return requested;
}

// Returns the pointer to the ShapePoint that was most recently deleted and resets the deletedPoint attribute to nullptr. Returns nullptr, when no point was deleted.
ShapePoint *ShapeEditor::getDeletedPoint() {
    ShapePoint *deleted = deletedPoint;
    deletedPoint = nullptr;
    return deleted;
}

// Is called when an option on any context menu was selected. Changes the interpolation mode between points if this option was one of shapePower, shapeSine, etc.
// Resets rightClicked to nullptr.
void ShapeEditor::processMenuSelection(WPARAM wParam){
    if (rightClicked == nullptr){
        return;
    }

    switch (wParam) {
        case shapePower:
            rightClicked->mode = shapePower;
            break;
        case shapeSine:
            // rightClicked->mode = shapeSine;
            break;
    }
    rightClicked = nullptr;
}

// Reset currentlyDragging to nullptr and currentDraggingMode to none.
void ShapeEditor::processMouseRelease(uint32_t x, uint32_t y){
    currentlyDragging = nullptr;
    currentDraggingMode = none;
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
                    canvas[i + (uint32_t)((yL - pow((float)(i - xMin) / (xMax - xMin), power) * (int32_t)(yL - yR))) * GUI_WIDTH] = color;
                } else {
                    canvas[i + (uint32_t)((yR + pow((float)(xMax - i) / (xMax - xMin), -power) * (int32_t)(yL - yR))) * GUI_WIDTH] = color;
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
                canvas[i + (int)(ampCorrection * sin((float)(i - xMin - (xMax - xMin)/2) / (xMax - xMin) * point->sineOmega * 2 * pi) * (yR - yL) / 2 - (yL - yR)/2 + yL) * GUI_WIDTH] = color;
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
        xLowerLim = currentlyDragging->next == nullptr ? XYXY[2] : currentlyDragging->previous->getAbsPosX();
        xUpperLim = currentlyDragging->next == nullptr ? XYXY[2] : currentlyDragging->next->getAbsPosX();

        x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
        y = (y > (int32_t)XYXY[3]) ? XYXY[3] : (y < XYXY[1]) ? XYXY[1] : y;

        // the rightmost point must not move in x-direction
        if (currentlyDragging->next == nullptr){
            x = XYXY[2];
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

/*The phase of Envelopes is periodic and dependent on time. The frequency with which an Envelope oscillates
can be cahnged by the user by adjusting a "counter" which displays a number. There are different modes
determining how this number is interpreted:
    -envelopeFrequencyTempo: the Envelope loops n times per beat, where n is a power of 2 or one over a power of 2.
    -envelopeFrequencyTempoTriplets: similar to previous, except that it loops in a triplet pattern.
    -envelopeFrequencySeconds: the period of the oscillation in seconds can be set directly using the counter.

The FrequencyPanels that control these properties are stored in EnvelopeManager and only referenced in Envelope.
*/

// An InteractiveGUIElement that lets the user change the frequency with which the corresponding Envelope loops.
// There is one FrequencyPanel for each Envelope in EnvelopeManager, of which only the one corresponding to the active Envelope is displayed.
//
// Consists of two elements that are drawn to the GUI: A "counter", which displays a number that corresponds to the frequency and a dropdown menu, that lets the user select the loop mode.
class FrequencyPanel : InteractiveGUIElement {
    private:
    uint32_t XYXY[4]; // The size and position of this element in pixel in XYXY notation.
    uint32_t counterXYXY[4]; // The size and position of the counter in pixel in XYXY notation.
    uint32_t modeButtonXYXY[4]; // The size and position of the button to select the loop mode in pixel and XYXY notation.

    uint32_t clickedX = 0;
    uint32_t clickedY = 0;

    contextMenuType menuRequest = menuNone;

    bool currentlyDraggingCounter = false; // True if user is dragging on counter and value has to be updated.
    bool updateCounter = true; // Indicates whether the counter has to be redrawn on the GUI. True if counter value has changed after dragging or switching currentLoopMode. Resets to false once renderGUI() is called.
    bool updateButton = true; // Indicates whether the button should be rerendered in case of a switch between modes. Resets to false once renderGUI() is called.

    std::map<envelopeLoopMode, double> counterValue =  {{envelopeFrequencyTempo, 0}, // Mapping between the loop mode and the corresponding counter value.
                                                        {envelopeFrequencySeconds, 1.}};
    double previousValue = 1.; // Is used to store the counter value of the currentLoopMode when the user changes the counter value. 

    
    public:
    envelopeLoopMode currentLoopMode = envelopeFrequencyTempo; // The current looping mode, can be based on tempo, tempo triplets or seconds.

    FrequencyPanel(uint32_t inXYXY[4], envelopeLoopMode initMode = envelopeFrequencyTempo, double initValue = 0.) {
        currentLoopMode = initMode;
        counterValue[initMode] = initValue;

        for (int i=0; i<4; i++) {
            XYXY[i] = inXYXY[i];
        }

        // TODO for now counter and button are just split in half. There is room for visual improvement
        counterXYXY[0] = XYXY[0];
        counterXYXY[1] = XYXY[1];
        counterXYXY[2] = (uint32_t) (XYXY[2] + XYXY[0]) / 2;
        counterXYXY[3] = XYXY[3];

        modeButtonXYXY[0] = (uint32_t) (XYXY[2] + XYXY[0]) / 2;
        modeButtonXYXY[1] = XYXY[1];
        modeButtonXYXY[2] = XYXY[2];
        modeButtonXYXY[3] = XYXY[3];
    }

    // Can either
    //  - request context menu to select loop mode if clicked on the mode button
    //  - start tracking mouse for changing counter value, if clicked on counter
    void processLeftClick(uint32_t x, uint32_t y) {
        clickedX = x;
        clickedY = y;

        if (isInBox(x, y, modeButtonXYXY)) {
            menuRequest = menuEnvelopeLoopMode;
            // TODO on menu selection, if currentLoopMode has changed, update previousValue
        }

        if (isInBox(x, y, counterXYXY)) {
            currentlyDraggingCounter = true;
        }
    };

    // If last left click was on counter, updates counter value based on y-position of mouse.
    void processMouseDrag(uint32_t x, uint32_t y) {
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
    void processMouseRelease(uint32_t x, uint32_t y) {
        previousValue = counterValue[currentLoopMode];
        currentlyDraggingCounter = false;
    };

    void processDoubleClick(uint32_t x, uint32_t y) {};
    void processRightClick(uint32_t x, uint32_t y) {};

    // Renders the counter based on the current counter value
    void renderCounter(uint32_t *canvas) {
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
        fillRectangle(canvas, counterXYXY);
        drawTextBox(canvas, counterText, counterXYXY[0], counterXYXY[1], counterXYXY[2], counterXYXY[3]);
    }

    // Renders the button with the currentLoopMode.
    void renderButton(uint32_t *canvas) {
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
        fillRectangle(canvas, modeButtonXYXY);
        drawTextBox(canvas, buttonText, modeButtonXYXY[0], modeButtonXYXY[1], modeButtonXYXY[2], modeButtonXYXY[3]);
    }

    // Renders two boxes:
    // Left: a counter, which displays a number, right: a textbox displaying the currentLoopMode.
    void renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed) {
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

    contextMenuType getMenuRequestType() {
        contextMenuType requested = menuRequest;
        menuRequest = menuNone;
        return requested;
    }

    void processMenuSelection(WPARAM wParam) {
        envelopeLoopMode previous = currentLoopMode;
        if (wParam == envelopeFrequencyTempo) {
            currentLoopMode = envelopeFrequencyTempo;
        }
        else if (wParam == envelopeFrequencySeconds) {
            currentLoopMode = envelopeFrequencySeconds;
        }
        if (currentLoopMode != previous) {
            updateButton = true;
            updateCounter = true;
            previousValue = counterValue[currentLoopMode];
        }
    }

    // Returns the Envelope phase corresponding to the current beat position and loop mode.
    double getEnvelopePhase(double beatPosition, double secondsPlayed) {
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
    void setupForRerender() {
        updateButton = true;
        updateCounter = true;
    }
};

Envelope::Envelope(uint32_t size[4], FrequencyPanel *inPanel) : ShapeEditor(size) {
    frequencyPanel = inPanel;
};

void Envelope::addModulatedParameter(ShapePoint *point, float amount, modulationMode mode){
    switch (mode) {
        case modCurveCenterY:
        {
            // Add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
            modulatedParameters.push_back(&point->curveCenterPosY);

            // Add this Envelope to the list of modulating Envelopes of the parameter.
            point->curveCenterPosY.addModulator(this, amount);
            break;
        }
        case modPosY:
        {
            // Add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
            modulatedParameters.push_back(&point->posY);

            // Add this Envelope to the list of modulating Envelopes of the parameter.
            point->posY.addModulator(this, amount);
            break;
        }
        case modPosX:
        {
            // Add the ModulatedParameter to the list of ModulatedParameters controlled by this Envelope.
            modulatedParameters.push_back(&point->posX);

            // Add this Envelope to the list of modulating Envelopes of the parameter.
            point->posX.addModulator(this, amount);
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

EnvelopeManager::EnvelopeManager(uint32_t inXYXY[4]){
    for (int i=0; i<4; i++){
        XYXY[i] = inXYXY[i];
    }

    envelopes.reserve(MAX_NUMBER_ENVELOPES);
    frequencyPanels.reserve(MAX_NUMBER_ENVELOPES);

    // Width and height of the EnvelopeManager. 
    // 10% on left and bottom are reserved for other GUI elements, rest is for Envelopes.
    uint32_t width = XYXY[2] - XYXY[0];
    uint32_t height = XYXY[3] - XYXY[1];

    // The Envelope is positioned at the upper right of the EnvelopeManager.
    envelopeXYXY[0] = (uint32_t)(XYXY[0] + 0.1*width);
    envelopeXYXY[1] = XYXY[1];
    envelopeXYXY[2] = XYXY[2];
    envelopeXYXY[3] = (uint32_t)(XYXY[3] - 0.4*height);

    // The selector panel is positioned directly to the left of Envelope, with the same y-extent as the Envelope.
    selectorXYXY[0] = XYXY[0];
    selectorXYXY[1] = XYXY[1];
    selectorXYXY[2] = envelopeXYXY[0];
    selectorXYXY[3] = envelopeXYXY[3];

    // The knobs are positioned below the Envelope, with the same x-extent as the Envelope.
    // For y-position, take width of the 3D frame around the active Envelope into account.
    knobsXYXY[0] = envelopeXYXY[0];
    knobsXYXY[1] = (uint32_t)(XYXY[3] - 0.4*height) + FRAME_WIDTH;
    knobsXYXY[2] = XYXY[2];
    knobsXYXY[3] = (uint32_t)(XYXY[3] - 0.2*height);

    // Tools are positioned below knobs.
    toolsXYXY[0] = envelopeXYXY[0];
    toolsXYXY[1] = (uint32_t)(XYXY[3] - 0.2*height) + FRAME_WIDTH;
    toolsXYXY[2] = XYXY[2];
    toolsXYXY[3] = XYXY[3];

    activeEnvelopeIndex = 0;

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
    frequencyPanels.push_back(FrequencyPanel(toolsXYXY));

    envelopes.push_back(Envelope(envelopeXYXY, &frequencyPanels.back()));
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

// Draws the GUI of this EnvelopeManager to canvas. Always calls renderGUI on the active Envelope but renders 3D frames, Envelope selection panel and tool panel only if they have been changed.
void EnvelopeManager::renderGUI(uint32_t *canvas, double beatPosition, double secondsPlayed){
    envelopes[activeEnvelopeIndex].renderGUI(canvas);

    if (updateGUIElements){
        setupFrames(canvas);
    }

    if (toolsUpdated){
        drawKnobs(canvas);
        toolsUpdated = false;
    }

    frequencyPanels.at(activeEnvelopeIndex).renderGUI(canvas, beatPosition, secondsPlayed);
}

// Set ups the EnvelopeManager to fully rerender the next time renderGUI() is called. Use when plugin window is reopened after being closed.
void EnvelopeManager::setupForRerender() {
    frequencyPanels.at(activeEnvelopeIndex).setupForRerender();
}

// Draws the knobs for the ModulatedParameters of the active Envelope to the tool panel. Knobs are positioned next to each other sorted by their index in the modulatedParameters vector of the parent Envelope.
void EnvelopeManager::drawKnobs(uint32_t *canvas){
    // first reset whole area
    fillRectangle(canvas, knobsXYXY);

    for (int i=0; i<envelopes[activeEnvelopeIndex].getModulatedParameterNumber(); i++){
        float amount = envelopes[activeEnvelopeIndex].getModAmount(i); // modulation amount of the ith ModulatedParameter
        drawLinkKnob(canvas, knobsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, knobsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE, amount);
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
    if (isInBox(x, y, selectorXYXY)){
        int newActive = (y - selectorXYXY[1]) * envelopes.size() / (selectorXYXY[3] - selectorXYXY[1]);
        setActiveEnvelope(newActive);

        // After the switch was pressed, assume the following mouse movement to be an attempt to link a parameter to this Envelope:
        currentDraggingMode = addLink;
    }

    // check if it has been clicked on any of the LinkKnobs and set currentdragging mode to moveKnob if true
    for (int i=0; i<envelopes.at(activeEnvelopeIndex).getModulatedParameterNumber(); i++){
        if (isInPoint(x, y, knobsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, knobsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE)){
            selectedKnob = i;
            selectedKnobAmount = envelopes.at(activeEnvelopeIndex).getModAmount(i);
            currentDraggingMode = moveKnob;
        }
    }

    // always forward input to active Envelope
    envelopes.at(activeEnvelopeIndex).processLeftClick(x, y);
    frequencyPanels.at(activeEnvelopeIndex).processLeftClick(x, y);
    // FrequencyPanel might request opening a context menu
    menuRequest = frequencyPanels.at(activeEnvelopeIndex).getMenuRequestType();
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
        if (isInPoint(x, y, knobsXYXY[0] + LINK_KNOB_SPACING*i + LINK_KNOB_SPACING/2, knobsXYXY[1] + (int)(LINK_KNOB_SPACING/2), LINK_KNOB_SIZE)){
            selectedKnob = i;
            menuRequest = menuLinkKnob;
            return;
        }
    }
    
    envelopes.at(activeEnvelopeIndex).processRightClick(x, y);
    menuRequest = envelopes.at(activeEnvelopeIndex).getMenuRequestType();
    frequencyPanels.at(activeEnvelopeIndex).processRightClick(x, y);
}

void EnvelopeManager::processMenuSelection(WPARAM wParam){
    if (wParam == removeLink && selectedKnob != -1){
        envelopes.at(activeEnvelopeIndex).removeModulatedParameter(selectedKnob);
        toolsUpdated = true;
    }
    // If it was attempted to modulate a point position, add a ModulatedParameter based on the modulationMode.
    // Set attemptedToModulate to nullptr afterwards to make sure no link is created by accident.
    else if (wParam == modPosX){
        if (attemptedToModulate != nullptr) {
            addModulatedParameter(attemptedToModulate, 1, modPosX);
        }
        attemptedToModulate = nullptr;
    }
    else if (wParam == modPosY) {
        if (attemptedToModulate != nullptr) {
            addModulatedParameter(attemptedToModulate, 1, modPosY);
        }
        attemptedToModulate = nullptr;
    }

    envelopes.at(activeEnvelopeIndex).processMenuSelection(wParam);
    frequencyPanels.at(activeEnvelopeIndex).processMenuSelection(wParam);
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

    envelopes[activeEnvelopeIndex].addModulatedParameter(point, amount, mode);
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

// Returns the type of menu requested by the EnvelopeManager to open. The type is reset to menuNone when called.
contextMenuType EnvelopeManager::getMenuRequestType() {
    contextMenuType requested = menuRequest;
    menuRequest = menuNone;
    return requested;
}
