/*
Everything concerning the shape editors.
A shape editor is a region on the UI that can be used to design a curve by adding, moving and deleting points between
which the curve is interpolated. The way the curve is interpolated in each region between two points ("curve segment") can be
chosen from a dialog box if rightclicking the point to the right of the segment. There are the options power and sine, see implementation for further explanations.
Some shapes can further be edited by dragging the center of the curve.

Envelopes are shapeEditors that can be used to modulate certain parameters.
*/

#include <vector>
#include <cstdint>
#include <chrono>
#include <tuple>
#include <cmath>
#include <iostream>
#include <assert.h>
#include <set>
#include "../config.h"

enum Shapes{
    // Function each curve segment between two points can follow
    shapePower, // curve follows shape of f(x) = (x < 0.5) ? x^power : 1-(1-x)^power, for 0 <= x <= 1, streched to the corresponding x and y intervals
    shapeSine,
    shapeBezier // i think i will drop this because i do NOT feel like programming this... more points and some more clamping to the box etc
};

enum draggingMode{
    // When MouseDrag is processed, decide which parameters to change depending on these values
    none, // ignore
    position, // moves position of selected point
    curveCenter // adjusts curveCenter and therefore parameter of curve function of next point to the right
};

enum modulationMode{
    modCurveCenterY,
    modPosX,
    modPosY
};

void drawPoint(uint32_t *bits, float x, float y, uint32_t color = 0x000000, float size = 5){
    for (int i=(int)-size/2; i<(int)size/2; i++){
        for (int j=(int)-size/2; j<(int)size/2; j++){
            if (i*i + j*j <= size*size/4){
                bits[(int)x + i + ((int)y + j) * GUI_WIDTH] = color;
            }
        }
    }
}

/*Returns the power p of a curve f(x) = x^p that goes through yCenter at x=0.5. User is able to deform curves by dragging the curve center. This function is used to get the power of the curve from the mouse position.
If yCenter > 0.5, the power is negative, which does NOT mean f(x)=1/(x^power), but that the curve is flipped f(x)=1-(1-x)^power. This convention is always used for shapes with mode shapePower.*/
float getPowerFromYCenter(float yCenter){
    float power = (yCenter < 0.5) ? log(yCenter) / log(0.5) :  - log(1 - yCenter) / log(0.5);
    power = (power < 0) ? ((power < -SHAPE_MAX_POWER) ? -SHAPE_MAX_POWER : power) : ((power > SHAPE_MAX_POWER) ? SHAPE_MAX_POWER : power);
    return power;
}

class Modulator;

/* Class to store all information about a curve Segment. Curve segments are handled in a way that information about the function
(e.g. mode and parameters) are stored together with the point next to the right, which marks the beginning of the next segment.
The reason for that is that curve has to always satisfy f(0) = 0 or else the plugin would generate a DC offset, so the first curve segment
has a point only on its right end.*/
class ShapePoint{
    public:
        // position and size of parent shapeEditor
        int32_t XYXY[4];

        // parameters of the point
        float posX;
        float posY;
        int32_t absPosX;
        int32_t absPosY;
        bool belowPrevious = false;
        bool flipPower = false;

        // parameters of the curve left to the point
        float curveCenterPosY;
        float curveCenterPosYMod;
        uint16_t curveCenterAbsPosX;
        uint16_t curveCenterAbsPosY;
        uint16_t curveCenterAbsPosYMod;
        float power; // power can be negative, which does NOT mean the curve is actually f(x) = 1 / (x^|power|) but that the curve is flipped vertically
        float sineOmegaPrevious;
        float sineOmega;
        Shapes mode = shapePower;

        bool isActive;

    /*some methods need a pointer to the previous ShapePoint. I tried saving it as a member as in a linked list but that did not work, it would sometimes
    break and lose track of previous somehow.*/

    /*x, y are relative positions on the graph editor, e.g. they must be between 0 and 1
    editorSize is screen coordinates of parent shapeEditor in XYXY notation: {xMin, yMin, xMax, yMax}
    previousPoint is pointer to the next ShapePoint on the left hand side of the current point. If there is no point to the left, should be nullptr
    pow is parameter defining the shape of the curve segment, its role is dependent on the mode:
        shapePower: f(x) = x^parameter
    */
    ShapePoint(float x, float y, uint16_t editorSize[4], ShapePoint *previous, float pow = 1, float omega = 0.5, Shapes initMode = shapePower){
        assert ((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));

        // x and y are relative coordinates on the Graph 0 <= x, y <= 1
        posX = x;
        posY = y;

        // in absolute coordinates y is mirrored
        absPosX = editorSize[0] + (int32_t)(x * (editorSize[2] - editorSize[0]));
        absPosY = editorSize[3] - (int32_t)(y * (editorSize[3] - editorSize[1]));

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

        isActive = true;
    }

    void updatePositionAbsolute(int32_t x, int32_t y, ShapePoint *previous){
        absPosX = x;
        absPosY = y;

        posX = (float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]);
        posY = (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]);

        updateCurveCenter(previous);
    }

    // updates the Curve center point in cases where curveCenter is not directly changed, as for example if the point is being moved.
    void updateCurveCenter(ShapePoint *previous){
        // curve center x is always in the middle of neighbouring points
        curveCenterAbsPosX = (int32_t)(absPosX + ((previous == nullptr) ? XYXY[0] : previous->absPosX)) / 2;

        if (mode == shapePower){
            int32_t yL = (previous == nullptr) ? XYXY[3] : previous->absPosY;
            curveCenterAbsPosY = (power > 0) ? yL - pow(0.5, power) * (yL - absPosY) : absPosY + pow(0.5, -power) * (yL - absPosY);
            // curveCenterAbsPosY = yL + curveCenterPosY * (yL - absPosY)
        } else if (mode == shapeSine){
            curveCenterAbsPosY = (int32_t)(absPosY + ((previous == nullptr) ? XYXY[3] : previous->absPosY)) / 2;
        }
    }

    // updates the Curve center point when manually dragging it
    void updateCurveCenter(ShapePoint *previous, int x, int y){
        // nothing to update if line is horizontal
        if (((previous == nullptr) ? 0 : previous->absPosY) == absPosY){
            return;
        }

        switch (mode){
            case shapePower:
                // logarithm will be problematic if y position reaches zero. Clamp y value between these limits so that power can not extend maxPower
            {
                int32_t yL = (previous == nullptr) ? XYXY[3] : previous->absPosY;

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

                curveCenterAbsPosX = (int32_t)(absPosX + ((previous == nullptr) ? XYXY[0] : previous->absPosX)) / 2;
                curveCenterAbsPosY = (int32_t)(absPosY + ((previous == nullptr) ? XYXY[3] : previous->absPosY)) / 2;

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

    void processMousePress(){
        sineOmegaPrevious = sineOmega;
    }
};

class ShapeEditor{
    private:
        // square of the minimum distance the mouse needs to have from a point in order to connect any input to this point
        // TODO why this cant be static const???
        float requiredSquaredDistance = 200;

        uint32_t *canvas;

        // this is only relevant for Linux since xlib does not feature automatic double click detection
        // std::chrono::_V2::system_clock::time_point time = std::chrono::high_resolution_clock::now();
        // long lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();

        // place to store point which is currently edited and the edit mode
        ShapePoint *currentlyDragging = nullptr;
        draggingMode currentDraggingMode = none;

        // TODO this and currentlyDragging could be combined to smth like "currentlyEdited" or "active" since only one point can be edited at a time.
        ShapePoint *rightClicked = nullptr;

    public:
        uint16_t XYXY[4];
        // all points of this editor are stored in a vector, they must always be sorted by ther absolute x position
        std::vector<ShapePoint> shapePoints;

    ShapeEditor(uint16_t position[4], std::vector<ShapePoint> points = {}){
        if (points.empty()){
            // if no points are given, use the default points which diagonally span across the whole Graph.
            shapePoints = {ShapePoint(1., 1., position, nullptr)};
        }
        else{
            shapePoints = points;
        }

        for (int i=0; i<4; i++){
            XYXY[i] = position[i];
        }
    }

    void drawGraph(uint32_t *bits){
        // set whole area to background color
        for (uint32_t i = XYXY[0]; i < XYXY[2]; i++) {
            for (uint32_t j = XYXY[1]; j < XYXY[3]; j++) {
                bits[i + j * GUI_WIDTH] = colorBackground;
            }
        }

        // first draw all connections between points, then points on top
        for (uint16_t i=0; i< shapePoints.size(); i++){
            drawConnection(bits, i, colorCurve);
        }
        for (ShapePoint point : shapePoints){
            drawPoint(bits, point.absPosX, point.absPosY, colorCurve, 20);
            drawPoint(bits, point.curveCenterAbsPosX, point.curveCenterAbsPosY, colorCurve, 16);
        }

    }

    std::tuple<float, int> getClosestPoint(uint32_t x, uint32_t y){
        // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.
        //   This would require every point to check mouse position at any time
        float closestDistance = 10E5;
        int closestIndex;

        // check mouse distance to all points and find closest
        float distance;
        for (uint16_t i=0; i<shapePoints.size(); i++){
            // check for distance to point and set mode to position
            distance = (shapePoints[i].absPosX - x)*(shapePoints[i].absPosX - x) + (shapePoints[i].absPosY - y)*(shapePoints[i].absPosY - y);
            if (distance < closestDistance){
                closestDistance = distance;
                closestIndex = i;
                currentDraggingMode = position;
            }
            // check for distance to curve center and set mode to curveCenter
            distance = (shapePoints[i].curveCenterAbsPosX - x)*(shapePoints[i].curveCenterAbsPosX - x) + (shapePoints[i].curveCenterAbsPosY - y)*(shapePoints[i].curveCenterAbsPosY - y);
            if (distance < closestDistance){
                closestDistance = distance;
                closestIndex = i;
                currentDraggingMode = curveCenter;
            }
        }

        return std::make_tuple(closestDistance, closestIndex);
    }

    void processMousePress(int32_t x, int32_t y){

        // check if mouse hovers over the graphical interface and return if not
        // check for slightly higher area to be able to grab point from outside the interface
        float offset = pow(requiredSquaredDistance, 0.5);
        if ((x < XYXY[0] - offset) | (x >= XYXY[2] + offset) | (y < XYXY[1] - offset) | (y >= XYXY[3] + offset)){
            return;
        }

        // Get time since last click and substract now. LastMousePress will be updated on Mouse release.
        // long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        // long timePassed = now - lastMousePress;
        // lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        // if in proximity to point mark point as currentlyDragging
        if (closestDistance <= requiredSquaredDistance){
            shapePoints.at(closestIndex).processMousePress();
            currentlyDragging = &shapePoints.at(closestIndex);
        }
    }

    void processDoubleClick(uint32_t x, uint32_t y){
        if ((x < XYXY[0]) | (x >= XYXY[2]) | (y < XYXY[1]) | (y >= XYXY[3])){
            return;
        }

        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        if (closestDistance <= requiredSquaredDistance){
            // if close to point and point is not last, which cannot be removed, remmove point
            if (currentDraggingMode == position && closestIndex != shapePoints.size()){
                shapePoints.erase(shapePoints.begin() + closestIndex);
                shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));
            }

            // if double click on curve center, reset power
            else if (currentDraggingMode == curveCenter){
                if (shapePoints.at(closestIndex).mode == shapePower){
                    shapePoints.at(closestIndex).power = 1;
                } else if (shapePoints.at(closestIndex).mode == shapeSine){
                    shapePoints.at(closestIndex).sineOmega = 0.5;
                    shapePoints.at(closestIndex).sineOmegaPrevious = 0.5;
                }
                shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));

            }
        }

        // if not in proximity to point, add one
        if (closestDistance > requiredSquaredDistance){
            // points in shapePoints should be sorted by x-position, find correct index to insert
            uint16_t insertIdx = 0;
            while (insertIdx < shapePoints.size()){
                if (shapePoints.at(insertIdx).absPosX >= x){
                    break;
                }
                insertIdx ++;
            }

            ShapePoint *pPrevious = (insertIdx == 0) ? nullptr : &shapePoints.at(insertIdx - 1);
            shapePoints.insert(shapePoints.begin() + insertIdx, ShapePoint((float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]), (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]), XYXY, pPrevious));
            shapePoints.at(insertIdx + 1).updateCurveCenter(&shapePoints.at(insertIdx));
        }
    }

    ShapePoint* processRightClick(uint32_t x, uint32_t y){
        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        // if rightclick on point, show shape menu to change function of curve segment
        if (closestDistance <= requiredSquaredDistance && currentDraggingMode == position){
            rightClicked = &shapePoints.at(closestIndex);
            return (currentDraggingMode == position) ? rightClicked : nullptr;
        }
        // if right click on curve center, reset power
        else if (closestDistance <= requiredSquaredDistance && currentDraggingMode == curveCenter){
            if (shapePoints.at(closestIndex).mode == shapePower){
                shapePoints.at(closestIndex).power = 1;
            } else if (shapePoints.at(closestIndex).mode == shapeSine){
                shapePoints.at(closestIndex).sineOmega = 0.5;
                shapePoints.at(closestIndex).sineOmegaPrevious = 0.5;
            }
            shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));
        }
        return nullptr;
    }

    void processMenuSelection(WPARAM wParam){
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
        rightClicked->updateCurveCenter((rightClicked == &shapePoints.front()) ? nullptr : rightClicked);
    }

    void processMouseRelease(){
        currentlyDragging = nullptr;
        currentDraggingMode = none;
    }

    /* TODO does it make sense to have a single function that transforms values according to the shape of all curve segments? This function could be used to transform x-pixel coordinates to get the y value and also to actually shape the input sound.*/
    void drawConnection(uint32_t *bits, int index, uint32_t color = 0x000000, float thickness = 5){

        int xMin = ((index == 0) ? (float)XYXY[0] : shapePoints.at(index - 1).absPosX);
        int xMax = shapePoints.at(index).absPosX;

        int yL = ((index == 0) ? (float)XYXY[3] : shapePoints.at(index - 1).absPosY);
        int yR = shapePoints.at(index).absPosY;

        switch (shapePoints.at(index).mode){
            case shapePower:
            {
                // The curve is f(x) = x^power, where x is in [0, 1] and x and y intervals are stretched such that the curve connects this and previous point
                for (int i = xMin; i < xMax; i++) {
                    // TODO: antialiasing/ width of curve
                    if (shapePoints.at(index).power > 0){
                        bits[i + (int)((yL - pow((float)(i - xMin) / (xMax - xMin), shapePoints.at(index).power) * (yL - yR))) * GUI_WIDTH] = color;
                    } else {
                        bits[i + (int)((yR + pow((float)(xMax - i) / (xMax - xMin), -shapePoints.at(index).power) * (yL - yR))) * GUI_WIDTH] = color;
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
                    float ampCorrection = (shapePoints.at(index).sineOmega < 0.5) ? 1 / sin(shapePoints.at(index).sineOmega * pi) : 1;
                    // this works trust me
                    bits[i + (int)(ampCorrection * sin((float)(i - xMin - (xMax - xMin)/2) / (xMax - xMin) * shapePoints.at(index).sineOmega * 2 * pi) * (yR - yL) / 2 - (yL - yR)/2 + yL) * GUI_WIDTH] = color;
                }
                break;
            }
            case shapeBezier:
            break;
        }
    }

    void processMouseDrag(int32_t x, int32_t y){
        if (!currentlyDragging){
            return;
        }

        ShapePoint *currentPrevious = (currentlyDragging == &shapePoints.front()) ? nullptr : (currentlyDragging - 1);

        if (currentDraggingMode == position){
            // point is not allowed to go beyond neighbouring points in x-direction or below 0 or above 1 if it first or last point.
            int32_t xLowerLim;
            int32_t xUpperLim;

            if (currentlyDragging == &shapePoints.front()){
                xLowerLim = XYXY[0];
            }
            else{
                xLowerLim = (currentlyDragging - 1)->absPosX;
            }

            if (currentlyDragging == &shapePoints.back()){
                xUpperLim = XYXY[2];
            }
            else{
                xUpperLim = (currentlyDragging + 1)->absPosX;
            }

            x = (x > xUpperLim) ? xUpperLim : (x < xLowerLim) ? xLowerLim : x;
            y = (y > XYXY[3]) ? XYXY[3] : (y < XYXY[1]) ? XYXY[1] : y;

            // the rightmost point must not move in x-direction
            if (currentlyDragging == &shapePoints.back()){
                x = XYXY[2];
            }

            currentlyDragging->updatePositionAbsolute(x, y, currentPrevious);

            if (currentlyDragging != &shapePoints.back()){
                (currentlyDragging + 1)->updateCurveCenter(currentlyDragging);
            }
        }

        else if (currentDraggingMode == curveCenter){
            currentlyDragging->updateCurveCenter(currentPrevious, x, y);
        }
    }

    float forward(float input){
        float out;
        // absolute value of input is processed, store information to flip sign after computing if necessary
        bool negativeInput = input < 0;
        input = (input < 0) ? -input : input;
        // limit input to one
        input = (input > 1) ? 1 : input;

        ShapePoint *point = &shapePoints.front();
        ShapePoint *previous = nullptr;

        while(point->posX < input){
            previous = point;
            point ++;
        }

        switch(point->mode){
            case shapePower:
            {
                float xL = (previous == nullptr) ? 0 : previous->posX;
                float yL = (previous == nullptr) ? 0 : previous->posY;

                float relX = (point->posX == xL) ? xL : (input - xL) / (point->posX - xL);
                float factor = (point->posY - yL);

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
};

/*Class that stores all information necessary to modulate a parameter, i.e. pointers to the affected shape points, amount of modulation and modulation mode.
Every modulatable parameter has a counterpart called [parameter_name_here]Mod. This value is the "active" value of the parameter and is used for displaying on GUI and rendering audio. It is recalculated at every audio sample. The raw parameter value is the "default" to which the modulation offset is added.

The "modulation" im referring to here is conceptually a modulation, but not technically speaking. These parameters are NOT reported to the host as modulatable or automatable parameter. The modulation is exclusively handled inside the plugin.*/
struct ModulatedParameter{
    ShapePoint *point;
    ShapePoint *previous;
    float amount;
    modulationMode mode;

    ModulatedParameter(ShapePoint *inPoint, ShapePoint *inPrevious, float inAmount, modulationMode inMode){
        point = inPoint;
        amount = inAmount;
        mode = inMode;
        previous = inPrevious;
    }

    void modulate(float modOffset){
        switch (mode){
            case modCurveCenterY:
            {
                float yL = previous == nullptr ? point->XYXY[3] : previous->absPosY;
                float yR = point->absPosY;

                int32_t yMin = (yL > yR) ? yR : yL;
                int32_t yMax = (yL < yR) ? yR : yL;

                float newY = point->curveCenterPosY + amount * modOffset;
                newY = (newY >= 1) ? 1 - (float)1/(yMax - yMin) : (newY <= 0) ? (float)1/(yMax - yMin) : newY;

                point->curveCenterPosYMod = newY;
                point->curveCenterAbsPosYMod = yL + newY * (yL - yR);
                point->power = getPowerFromYCenter(newY);
                point->updateCurveCenter(previous);
                break;
            }
        }
    }
};

/* Envelopes inherit the graphical editing capabilities from ShapeEditor but include methods to add, remove and update
controlled parameters */
class Envelope : public ShapeEditor {
    private:
        std::vector<ModulatedParameter> modulatedParameters;

    public:
        Envelope(uint16_t size[4]) : ShapeEditor(size) {};

        void addControlledParameter(ShapePoint *pPoint, ShapePoint *pPrevious, float amount, modulationMode mode){
            modulatedParameters.push_back(ModulatedParameter(pPoint, pPrevious, amount, mode));
        }

        void removeControlledParameter(int index){
            modulatedParameters.erase(modulatedParameters.begin() + index);
        }

        void updateModulatedParameters(double beatPosition){
            float modOffset = forward(beatPosition / 4);

            for (ModulatedParameter parameter : modulatedParameters){
                parameter.modulate(modOffset);
            }
        }
};
