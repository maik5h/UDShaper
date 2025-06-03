#include "assets.h"

// Blends between originalColor and newColor, such that for alpha = 0 the original color is preserved and for alpha = 1 the new color is chosen.
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

// Fills the area given by box with color. Assumes input canvas is the GUI.
void fillRectangle(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], uint32_t color){
    for (int y=box[1]; y<box[3]; y++){
        for (int x=box[0]; x<box[2]; x++){
            canvas[x + y * GUIWidth] = color;
        }
    }
}

// Draws a point of color and size at position x, y onto canvas.
void drawPoint(uint32_t *canvas, uint32_t GUIWidth, float x, float y, uint32_t color, float size){
    for (int i=(int)-size/2; i<(int)size/2; i++){
        for (int j=(int)-size/2; j<(int)size/2; j++){
            if (i*i + j*j <= size*size/4){
                canvas[(int)x + i + ((int)y + j) * GUIWidth] = color;
            }
        }
    }
}

// Draws a circle with a given width and radius to position (x, y) on canvas. radius gives the radius of the
// outer edge.
void drawCircle(uint32_t *canvas, uint32_t GUIWidth, uint32_t posX, uint32_t posY, uint32_t color, uint32_t radius, uint32_t width) {
    for (int y=-static_cast<int>(radius); y<static_cast<int>(radius); y++) {
        for (int x=-static_cast<int>(radius); x<static_cast<int>(radius); x++) {
            if (x*x + y*y <= radius*radius && x*x + y*y > (radius - width)*(radius - width)) {
                canvas[(y + posY)*GUIWidth + posX + x] = color;
            }
        }
    }
}

// Draws a simple frame to canvas. The frame extends the number of pixel given in thickness from the innerBox.
void drawFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerBox[4], int thickness, uint32_t color, float alpha){
    for (int i=innerBox[1] - thickness; i<innerBox[3] + thickness; i++){
        for (int j=innerBox[0] - thickness; j<innerBox[2] + thickness; j++){
            if (isInBox(j, i, innerBox)){
                continue;
            }

            canvas[j + i * GUIWidth] = blendColor(canvas[j + i * GUIWidth], color, alpha);
        }
    }
};

// Draw a frame that has each side shaded in a different color to create the impression of depth.
void draw3DFrame(uint32_t *canvas, uint32_t GUIWidth, uint32_t innerBox[4], uint32_t baseColor, int thickness){
    uint32_t colorBright1 = blendColor(colorBackground, 0xFFFFFF, 0.52);
    uint32_t colorBright2 = blendColor(colorBackground, 0xFFFFFF, 0.4);
    uint32_t colorDark1 = blendColor(colorBackground, 0x000000, 0.2);
    uint32_t colorDark2 = blendColor(colorBackground, 0x000000, 0.3);

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

// Draws a 3D frame similar to the function draw3DFrame, but the segment on the right is missing and connects to another 3D frame positioned to the right.
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

// Draws a grid inside box to canvas. Thickness is not yet implemented.
void drawGrid(uint32_t *canvas, uint32_t GUIWidth, uint32_t box[4], int numberLines, int thickness, uint32_t color, float alpha){
    int distanceX = (int)(box[2] - box[0]) / (numberLines + 1);
    int distanceY = (int)(box[3] - box[1]) / (numberLines + 1);

    // horizontal lines
    for (uint32_t y=1; y<=numberLines; y++){
        for (uint32_t x=box[0]; x<box[2]; x++){
            canvas[box[1] * GUIWidth + x + y * distanceY * GUIWidth] = blendColor(canvas[box[1] * GUIWidth + x + y * distanceY * GUIWidth], color, alpha);
        }
    }

    // vertical lines
    for (uint32_t y=box[1]; y<box[3]; y++){
        for (int x=1; x<=numberLines; x++){
            canvas[box[0] + x * distanceX + y * GUIWidth] = blendColor(canvas[x * distanceX + y * GUIWidth], color, alpha);
        }
    }
}

// Draws a LinkKnob at (posX, posY) to canvas. The knob is ring shaped and filled with color from the very top up to value, where value = +-1 fills the whole circle. The area is filled clockwise for positive and counter clockwise for negative values. It is used to control the links between Envelopes and Parameters, hence the name.
void drawLinkKnob(uint32_t *canvas, uint32_t GUIWidth, uint32_t posX, uint32_t posY, uint32_t size, float value, uint32_t color){
    float thickness = 0.45; // Relative thickness of the ring
    float r; // squared distance to center
    float phi; // angle

    int cX = posX; // x-center as signed int
    int cY = posY; // y-center as signed int

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
