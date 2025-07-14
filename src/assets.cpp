#include "assets.h"

uint32_t blendColor(uint32_t originalColor, uint32_t newColor, double alpha){
    uint8_t r1 = (originalColor >> 16) & 0xFF;
    uint8_t g1 = (originalColor >> 8) & 0xFF;
    uint8_t b1 = originalColor & 0xFF;

    uint8_t r2 = (newColor >> 16) & 0xFF;
    uint8_t g2 = (newColor >> 8) & 0xFF;
    uint8_t b2 = newColor & 0xFF;

    uint8_t r = r1 * (1 - alpha) + r2 * alpha;
    uint8_t g = g1 * (1 - alpha) + g2 * alpha;
    uint8_t b = b1 * (1 - alpha) + b2 * alpha;

    return (r << 16) | (g << 8) | b;
}

void fillRectangle(uint32_t *canvas, uint32_t GUIWidth, uint32_t XYXY[4], uint32_t color){
    for (int y=XYXY[1]; y<XYXY[3]; y++){
        for (int x=XYXY[0]; x<XYXY[2]; x++){
            canvas[x + y * GUIWidth] = color;
        }
    }
}

void drawPoint(uint32_t *canvas, uint32_t GUIWidth, float posX, float posY, uint32_t color, float size){
    int xMin = static_cast<int>(posX - size / 2);
    int xMax = static_cast<int>(posX + size / 2);
    int yMin = static_cast<int>(posY - size / 2);
    int yMax = static_cast<int>(posY + size / 2);

    for (int y=yMin; y<yMax; y++){
        for (int x=xMin; x<xMax; x++){
            if ((x - posX) * (x - posX) + (y - posY) * (y - posY) <= size*size/4){
                canvas[x + y * GUIWidth] = color;
            }
        }
    }
}

void drawCircle(uint32_t *canvas, uint32_t GUIWidth, uint32_t posX, uint32_t posY, uint32_t color, uint32_t radius, uint32_t width) {
    int rad = static_cast<int>(radius);
    
    for (int y=-rad; y<rad; y++) {
        for (int x=-rad; x<rad; x++) {
            if (x*x + y*y <= radius*radius && x*x + y*y > (radius - width)*(radius - width)) {
                canvas[(y + posY)*GUIWidth + posX + x] = color;
            }
        }
    }
}

void drawFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerBox[4], int thickness, uint32_t color, float alpha){
    for (int y=innerBox[1] - thickness; y<innerBox[3] + thickness; y++){
        for (int x=innerBox[0] - thickness; x<innerBox[2] + thickness; x++){
            if (isInBox(x, y, innerBox)){
                continue;
            }

            canvas[x + y * GUIWidth] = blendColor(canvas[x + y * GUIWidth], color, alpha);
        }
    }
};

void draw3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerBox[4], uint32_t baseColor, int thickness){
    uint32_t colorBright1 = blendColor(colorBackground, 0xFFFFFFFF, 0.52);
    uint32_t colorBright2 = blendColor(colorBackground, 0xFFFFFFFF, 0.4);
    uint32_t colorDark1 = blendColor(colorBackground, 0xFF000000, 0.2);
    uint32_t colorDark2 = blendColor(colorBackground, 0xFF000000, 0.3);

    // draw upper edge in colorDark2
    for (int i=0; i<thickness; i++){
        for (int x=innerBox[0] - i; x<innerBox[2] + i; x++){
            canvas[x + (innerBox[1] - i) * GUIWidth] = colorDark2;
        }
    }

    // draw lower edge in colorBright1
    for (int i=0; i<thickness; i++){
        for (int x=innerBox[0] - i; x<innerBox[2] + i; x++){
            canvas[x + (innerBox[3] + i) * GUIWidth] = colorBright1;
        }
    }

    // draw left edge in colorBright2
    for (int i=0; i<thickness; i++){
        for (int y=innerBox[1] - i; y<innerBox[3] + i; y++){
            canvas[innerBox[0] - i + y * GUIWidth] = colorBright2;
        }
    }

    // draw right edge in colorDark1
    for (int i=0; i<thickness; i++){
        for (int y=innerBox[1] - i; y<innerBox[3] + i; y++){
            canvas[innerBox[2] + i + y * GUIWidth] = colorDark1;
        }
    }
};

void drawPartial3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerBox[4], uint32_t baseColor, uint32_t thickness) {
    uint32_t colorBright1 = blendColor(colorBackground, 0xFFFFFF, 0.52);
    uint32_t colorBright2 = blendColor(colorBackground, 0xFFFFFF, 0.4);
    uint32_t colorDark1 = blendColor(colorBackground, 0x000000, 0.2);
    uint32_t colorDark2 = blendColor(colorBackground, 0x000000, 0.3);

    // Draw upper edge in colorDark2. Corner on the right is y-mirrored compared to the edge in regular 3D frames.
    for (int y=0; y<thickness; y++){
        for (int x=innerBox[0] - y; x<innerBox[2] + thickness - y; x++){
            canvas[x + (innerBox[1] - y) * GUIWidth] = colorDark2;
        }
    }

    // Draw lower edge in colorBright1. Corner on the right is y-mirrored compared to the edge in regular 3D frames.
    for (int y=0; y<thickness; y++){
        for (int x=innerBox[0] - y; x<innerBox[2] + thickness - y; x++){
            canvas[x + (innerBox[3] + y) * GUIWidth] = colorBright1;
        }
    }

    // Draw left edge in colorBright2.
    for (int x=0; x<thickness; x++){
        for (int y=innerBox[1] - x; y<innerBox[3] + x; y++){
            canvas[innerBox[0] - x + y * GUIWidth] = colorBright2;
        }
    }
}

void drawGrid(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], int numberLines, int thickness, uint32_t color, float alpha){
    int distanceX = (int)(box[2] - box[0]) / (numberLines + 1);
    int distanceY = (int)(box[3] - box[1]) / (numberLines + 1);

    for (uint32_t y=1; y<=numberLines; y++){
        for (uint32_t x=box[0]; x<box[2]; x++){
            canvas[box[1] * GUIWidth + x + y * distanceY * GUIWidth] = blendColor(canvas[box[1] * GUIWidth + x + y * distanceY * GUIWidth], color, alpha);
        }
    }

    for (uint32_t y=box[1]; y<box[3]; y++){
        for (int x=1; x<=numberLines; x++){
            canvas[box[0] + x * distanceX + y * GUIWidth] = blendColor(canvas[x * distanceX + y * GUIWidth], color, alpha);
        }
    }
}

// The knob this function draws is ring shaped and partially filled with color. The colored segment always
// starts at the top of the ring and extends clockwise or counterclowise up to certain angle. This angle is
// given by value, where +-1 stands for a fully filled ring.
void drawLinkKnob(uint32_t *canvas, uint32_t GUIWidth, uint32_t posX, uint32_t posY, uint32_t size, float value, uint32_t color){
    float thickness = 0.45; // Relative thickness of the ring
    float r;                // squared distance to center
    float phi;              // angle

    int cX = posX;          // x-center as signed int
    int cY = posY;          // y-center as signed int

    for (int y=posY-size/2; y<=posY+size/2; y++){
        for (int x=posX-size/2; x<posX+size/2; x++){
            r = (x - posX)*(x - posX) + (y - posY)*(y - posY);
            if ((r < size*size/4) && (r > size*size/4*(1-thickness)*(1-thickness))){
                // if y == posY or x == posX, to avoid division by zero, phi is directly set to the correct value
                if (posY == y){
                    phi = (x > posX) ? M_PI/2 : 3./2*M_PI;
                }
                else if (posX == x){
                    phi = (y > posY) ? M_PI : 0;
                }
                else{
                    // calculate absolute arctan and add phase offset depending on quadrant
                    if ((x - cX >= 0) && (cY - y > 0)){
                        phi = atan((float)(x - cX) / (cY - y));
                    }
                    else if ((x - cX > 0) && (cY - y < 0)){
                        phi = -atan((float)(cY - y) / (x - cX)) + M_PI/2.;
                    }
                    else if ((x - cX < 0) && (cY - y < 0)){
                        phi = atan((float)(x - cX) / (cY - y)) + M_PI;
                    }
                    else{
                        phi = -atan((float)(cY - y) / (x - cX)) + 3./2*M_PI;
                    }
                }

                // Change color depending on the relation of phi to 2*pi * value
                if ((value > 0) ? (phi < 2.*M_PI*value) : (2*M_PI - phi <= -2.*M_PI*value)){
                    canvas[x + y * GUIWidth] = color;
                }
                else{
                    canvas[x + y * GUIWidth] = 0U;
                }
            }
        }
    }
}
