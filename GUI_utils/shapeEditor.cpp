/*
Everything concerning the graph editors
*/

#include <vector>
#include <cstdint>
#include <chrono>
#include <tuple>
#include <cmath>
#include <iostream>

// different shapes a curve segment can follow. Most stll have to be implemented, more might follow
enum Shapes{
    shapePower,
    shapeStep,
    shapeBezier,
    shapeSine,
};

// TODO move this to gui_w32.cpp since it is platform dependent
void ShowShapeMenu(HWND hwnd, int xPos, int yPos) {
    // Create a menu
    HMENU hMenu = CreatePopupMenu();

    // title and separator
    AppendMenu(hMenu, MF_STRING | MF_DISABLED | MF_GRAYED, 0, "Function:");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    
    // Add items to the menu
    AppendMenu(hMenu, MF_STRING, shapePower, "Power");
    AppendMenu(hMenu, MF_STRING, shapeStep, "Step");
    AppendMenu(hMenu, MF_STRING, shapeSine, "Sine");
    AppendMenu(hMenu, MF_STRING, shapeBezier, "Bezier");

    // Display the menu at the given position
    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_TOPALIGN, xPos, yPos, 0, hwnd, NULL);

    // Clean up the menu after use
    DestroyMenu(hMenu);
}

void drawPoint(uint32_t *bits, float x, float y, uint32_t color = 0x000000, float size = 5){
    for (int i=(int)-size/2; i<(int)size/2; i++){
        for (int j=(int)-size/2; j<(int)size/2; j++){
            if (i*i + j*j <= size*size/4){
                bits[(int)x + i + ((int)y + j) * GUI_WIDTH] = color;
            }
        }
    }
}

class ShapePoint{
    private:
        float maxPower = 30;

    public:
        float posX;
        float posY;
        float absPosX;
        float absPosY;
        float power;
        bool belowPrevious = false;
        bool flipPower = false;
        int XYXY[4];
        float curveCenterAbsPosX;
        float curveCenterAbsPosY;
        bool hasChanged;
        Shapes mode = shapePower;

    ShapePoint(float x, float y, int editorSize[4], ShapePoint *previousPoint, float pow = 1){
        assert ((0 <= x) && (x <= 1) && (0 <= y) && (y <= 1));

        // x and y are relative pixel coordinates on the Graph
        posX = x;
        posY = y;

        // in absolute coordinates y is mirrored
        // TODO turn this into int
        absPosX = editorSize[0] + x * (editorSize[2] - editorSize[0]);
        absPosY = editorSize[3] - y * (editorSize[3] - editorSize[1]);
        power = pow;

        curveCenterAbsPosX = (absPosX + ((previousPoint == nullptr) ? editorSize[0] : previousPoint->absPosX)) / 2;
        curveCenterAbsPosY = (absPosY + ((previousPoint == nullptr) ? editorSize[3] : previousPoint->absPosY)) / 2;

        for (int i=0; i<4; i++){
            XYXY[i] = editorSize[i];
        }

        hasChanged = true;
    }

    void updatePositionAbsolute(uint32_t x, uint32_t y, ShapePoint *previous){
        absPosX = x;
        absPosY = y;

        posX = (x - XYXY[0]) / (XYXY[2] - XYXY[0]);
        posY = (XYXY[3] - y) / (XYXY[3] - XYXY[1]);

        updateCurveCenter(previous);
        hasChanged = true;
    }

    void updateCurveCenter(ShapePoint *previous){
        // updates the Curve center point in cases where point position changes
        curveCenterAbsPosX = (absPosX + ((previous == nullptr) ? XYXY[0] : previous->absPosX)) / 2;

        float yL = (previous == nullptr) ? XYXY[3] : previous->absPosY;

        // curveCenterAbsPosY = yMax - pow(0.5 * (absPosX - xL), power) * (yMax - yMin);
        belowPrevious = (absPosY > yL);
        // magic is happening here
        flipPower = belowPrevious ? (absPosY - curveCenterAbsPosY < curveCenterAbsPosY - yL) : (yL - curveCenterAbsPosY < curveCenterAbsPosY - absPosY);
        curveCenterAbsPosY = flipPower ? absPosY + pow(0.5, power) * (yL - absPosY) : yL - pow(0.5, power) * (yL - absPosY);
    
        hasChanged = true;
    }

    void updateCurveCenter(ShapePoint *previous, int x, int y){
        // updates the Curve center point when manually dragging it

        // nothing to update if line if flat
        if (((previous == nullptr) ? 0 : previous->absPosY) == absPosY){
            return;
        }

        float minCenterY = pow(0.5, maxPower);
        float maxCenterY = 1. - minCenterY;

        float xL = (previous == nullptr) ? XYXY[0] : previous->absPosX; 
        float yL = (previous == nullptr) ? XYXY[3] : previous->absPosY;
        float yMin = (yL <= absPosY) ? yL : absPosY;
        float yMax = (yL > absPosY) ? yL : absPosY;

        // find mouse position normalized to y extent and clamp it to boundaries
        float relExtent = (yMax - (float)y) / (yMax - yMin);
        relExtent = (relExtent > maxCenterY) ? maxCenterY : (relExtent < minCenterY) ? minCenterY : relExtent;

        // first find mouse position on normalized y-interval [0, 1]
        if (relExtent < 0.5){
            power = log(relExtent) / log(0.5);
            flipPower = false;
        }
        else{
            power = log(1 - relExtent) / log(0.5);
            flipPower = true;
        }

        // for curve calculation, determine if point is higher or lower than previous
        belowPrevious = (absPosY > yL);

        curveCenterAbsPosY = flipPower ? absPosY + pow(0.5, power) * (yL - absPosY) : yL - pow(0.5, power) * (yL - absPosY);
        if (absPosY > yL){
            curveCenterAbsPosY = absPosY - curveCenterAbsPosY + yL;
        }


        hasChanged = true;
    }
};

enum draggingMode{
    none,
    position,
    curveCenter
};

class ShapeEditor{
    private:
        // static const uint32_t colorBackground = 0x86D5F9;
        static const uint32_t colorBackground = 0xFFCF7F;
        static const uint32_t colorThinLines = 0xC0C0C0;
        static const uint32_t colorCurve = 0xFFFFFF;

        const float requiredSquaredDistance = 200;

        std::vector<ShapePoint> shapePoints;
        int XYXY[4];
        uint32_t *canvas;

        std::chrono::_V2::system_clock::time_point time = std::chrono::high_resolution_clock::now();
        long lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()).count();

        ShapePoint *currentlyDragging = nullptr;
        draggingMode currentDraggingMode = none;

        // TODO this and currentlyDragging could be combined to smth like "currentlyEdited" or "active" since only one point can be edited at a time.
        ShapePoint *rightClicked = nullptr;

    public:
    ShapeEditor(int position[4], std::vector<ShapePoint> points = {}){
        if (points.empty()){
            // if no points are given, use the default points which diagonally span across the whole Graph.
            // ShapePoint test = ShapePoint(1, 1, position, nullptr);
            // shapePoints = {test};
            // assert(false);
            shapePoints = {ShapePoint(1., 1., position, nullptr)};
        }
        else{
            shapePoints = points;
        }

        for (int i=0; i<4; i++){
            XYXY[i] = position[i];
        }
        // canvas = bits;
        // drawGraph(canvas);
    }

    void drawGraph(uint32_t *bits){
        for (uint32_t i = XYXY[0]; i < XYXY[2]; i++) {
            for (uint32_t j = XYXY[1]; j < XYXY[3]; j++) {
                bits[i + j * GUI_WIDTH] = colorBackground;
            }
        }

        ShapePoint *previous = nullptr;
        for (ShapePoint point : shapePoints){
            // point.draw(bits, previous, colorBackground, colorCurve);
            // previous = &point;
            drawPoint(bits, point.absPosX, point.absPosY, colorCurve, 20);
            drawPoint(bits, point.curveCenterAbsPosX, point.curveCenterAbsPosY, colorCurve, 16);
        }
        for (int i=0; i< shapePoints.size(); i++){
            drawConnection(bits, i, colorCurve);
        }
    }

    std::tuple<float, int> getClosestPoint(uint32_t x, uint32_t y){
        // TODO i think it might be a better user experience if points always give visual feedback if the mouse is hovering over them.
        //   This would require every point to check mouse position at any time
        float closestDistance = 10E5;
        int closestIndex;

        // TODO would be faster if checking for time first
        float distance;
        for (int i=0; i<shapePoints.size(); i++){
            distance = (shapePoints[i].absPosX - x)*(shapePoints[i].absPosX - x) + (shapePoints[i].absPosY - y)*(shapePoints[i].absPosY - y);
            if (distance < closestDistance){
                closestDistance = distance;
                closestIndex = i;

                // TODO this is very much not optimal i think. Should only be changed if actually started dragging?
                currentDraggingMode = position;
            }
            distance = (shapePoints[i].curveCenterAbsPosX - x)*(shapePoints[i].curveCenterAbsPosX - x) + (shapePoints[i].curveCenterAbsPosY - y)*(shapePoints[i].curveCenterAbsPosY - y);
            if (distance < closestDistance){
                closestDistance = distance;
                closestIndex = i;

                // TODO this is very much not optimal i think. Should only be changed if actually started dragging?
                currentDraggingMode = curveCenter;
            }
        }

        return std::make_tuple(closestDistance, closestIndex);
    }

    void processMousePress(int32_t x, int32_t y){

        // check if mouse hovers over the graphical interface and return if not
        if (x < XYXY[0] | x >= XYXY[2] | y < XYXY[1] | y >= XYXY[3]){
            return;
        }

        // check distance of mouse from all existing points and choose closest if close enough

        // Get time since last click and substract now. LastMousePress will be updated on Mouse release.
        long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        long timePassed = now - lastMousePress;
        lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();

        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        // if in proximity to point mark point as currentlyDragging or remove it if double click
        if (closestDistance <= requiredSquaredDistance){
            currentlyDragging = &shapePoints.at(closestIndex);
        }
    }

    void processDoubleClick(uint32_t x, uint32_t y){
        if (x < XYXY[0] | x >= XYXY[2] | y < XYXY[1] | y >= XYXY[3]){
            return;
        }

        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        if (closestDistance <= requiredSquaredDistance){
            // if close to point and point is not last, which cannot be removed, remmove point
            if (currentDraggingMode == position && closestIndex != shapePoints.size()){
                shapePoints.erase(shapePoints.begin() + closestIndex);

                // update curve center of next point
                shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));
            }

            // if double click on curve center, reset power
            else if (currentDraggingMode == curveCenter){
                shapePoints.at(closestIndex).power = 1;
                shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));
            }
        }

        // if not in proximity to point, add one
        if (closestDistance > requiredSquaredDistance){
            // points in shapePoints should be sorted by x-position, find correct index to insert
            int insertIdx = 0;
            while (insertIdx < shapePoints.size()){
                if (shapePoints.at(insertIdx).absPosX >= x){
                    break;
                }
                insertIdx ++;
            }

            ShapePoint *previous = (insertIdx == 0) ? nullptr : &shapePoints.at(insertIdx - 1);
            shapePoints.insert(shapePoints.begin() + insertIdx, ShapePoint((float)(x - XYXY[0]) / (XYXY[2] - XYXY[0]), (float)(XYXY[3] - y) / (XYXY[3] - XYXY[1]), XYXY, previous));
            shapePoints.at(insertIdx + 1).updateCurveCenter(&shapePoints.at(insertIdx));
            // TODO can be optimized by saving the state and comparing with previous state to only update parts that have changed.
        }
    }

    void processRightClick(HWND window, uint32_t x, uint32_t y){
        float closestDistance;
        int closestIndex;
        std::tie(closestDistance, closestIndex) = getClosestPoint(x, y);

        // if rightclick on point, show shape menu
        if (closestDistance <= requiredSquaredDistance && currentDraggingMode == position){
            rightClicked = &shapePoints.at(closestIndex);
            ShowShapeMenu(window, x, y);
        }
        // if right click on curve center, reset power
        else if (closestDistance <= requiredSquaredDistance && currentDraggingMode == curveCenter){
            shapePoints.at(closestIndex).power = 1;
            shapePoints.at(closestIndex).updateCurveCenter((closestIndex == 0) ? nullptr : &shapePoints.at(closestIndex - 1));
        }
    }

    void processMenuSelection(WPARAM wParam){
        switch (wParam) {
            case shapePower:
                rightClicked->mode = shapePower;
                break;
            case shapeStep:
                rightClicked->mode = shapeStep;
                break;
            case shapeSine:
                rightClicked->mode = shapeSine;
                break;
            case shapeBezier:
                rightClicked->mode = shapeBezier;
                break;
        }
    }

    void processMouseRelease(){
        // save time of release only if mouse was not dragging (feels better i think (nah))
        // if (currentDraggingMode != none){
        //     lastMousePress = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        // }
        currentlyDragging = nullptr;
        currentDraggingMode = none;
    }

    void drawConnection(uint32_t *bits, int index, uint32_t color = 0x000000, float thickness = 5){
        // The curve is given by a * x^power + b with a and b such that it connects the two points
        // b = shapePoints.at(index).posY;
        // a = (((index == 0) ? 0 : shapePoints.at(index-1).posY) - b) / (pow(shapePoints.at(index).posX - shapePoints.at(index-1).posX, shapePoints.at(index).power));

        int xMin = ((index == 0) ? (float)XYXY[0] : shapePoints.at(index - 1).absPosX);
        int xMax = shapePoints.at(index).absPosX;
        int yL = ((index == 0) ? (float)XYXY[3] : shapePoints.at(index - 1).absPosY);
        int yR = shapePoints.at(index).absPosY;

        // logToFile(std::to_string(shapePoints.at(index).flipX) + "\n" + std::to_string(shapePoints.at(index).flipY) + "\n");

        for (int i = xMin; i < xMax; i++) {
            // TODO: antialiasing/ width of curve
            if (shapePoints.at(index).flipPower && shapePoints.at(index).belowPrevious){
                bits[i + (int)((yL + pow((float)(i - xMin) / (xMax - xMin), shapePoints.at(index).power) * (yR - yL))) * GUI_WIDTH] = color;
            } else if (shapePoints.at(index).flipPower){
                bits[xMax - i + xMin + (int)((yR - pow((float)(i - xMin) / (xMax - xMin), shapePoints.at(index).power) * (yR - yL))) * GUI_WIDTH] = color;
            } else if (shapePoints.at(index).belowPrevious){
                bits[xMax - i + xMin + (int)((yR - pow((float)(i - xMin) / (xMax - xMin), shapePoints.at(index).power) * (yR - yL))) * GUI_WIDTH] = color;
            } else {
                bits[i + (int)((yL + pow((float)(i - xMin) / (xMax - xMin), shapePoints.at(index).power) * (yR - yL))) * GUI_WIDTH] = color;
            }
        }
    }

    void processMouseDrag(int32_t x, int32_t y){
        if (!currentlyDragging){
            return;
        }

        // point is not allowed to go beyond neighbouring points in x-direction or below 0 or above 1 if it first or last point.
        int32_t xLowerLim;
        int32_t xUpperLim;

        ShapePoint *currentPrevious = nullptr;

        if (currentlyDragging == &shapePoints.front()){
            xLowerLim = XYXY[0];
        }
        else{
            xLowerLim = (currentlyDragging - 1)->absPosX;
            currentPrevious = currentlyDragging - 1;
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

        if (currentDraggingMode == position){
            currentlyDragging->updatePositionAbsolute(x, y, currentPrevious);

            if (currentlyDragging != &shapePoints.back()){
                (currentlyDragging + 1)->updateCurveCenter(currentlyDragging);
            }
        }

        else if (currentDraggingMode == curveCenter){
            // // search for a power (1/maxPower <= power <= maxPower) such that the curve center is at mouse position
            // float yL = (currentPrevious == nullptr) ? XYXY[3] : currentPrevious->absPosY;
            // float yMax = (yL > currentlyDragging->absPosY) ? yL : currentlyDragging->absPosY;
            // float yMin = (yL <= currentlyDragging->absPosY) ? yL : currentlyDragging->absPosY;
            // if (y >= yMin && y <= yMax){
            //     float newPower = log((currentlyDragging->absPosY - y) / (yMax - yMin)) / log(0.5);

            //     // clamp between 1/maxPower and maxPower
            //     newPower = (newPower < 1 / maxPower) ? 1/maxPower : (newPower > maxPower) ? maxPower : newPower;
            //     currentlyDragging->power = newPower;
                currentlyDragging->updateCurveCenter(currentPrevious, x, y);
            // }
        }
    }
};
