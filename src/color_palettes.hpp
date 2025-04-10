#pragma once
#include <cmath>

struct Color {
    float r, g, b;
};

namespace ColorPalettes {
    // Apply log smoothing to normalized value
    float applyLogSmooth(float val);

    // Various color palette functions
    Color rainbowPalette(float normIter, float shift);
    Color firePalette(float normIter, float shift);
    Color electricBlue(float normIter, float shift);
    Color twilightPalette(float normIter, float shift);
    Color neonPalette(float normIter, float shift);
    Color vintageSepia(float normIter, float shift);
}; 