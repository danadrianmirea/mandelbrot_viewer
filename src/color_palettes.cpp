#include "color_palettes.hpp"
#include <algorithm>

namespace ColorPalettes {

float applyLogSmooth(float val) {
    return std::log(val * 0.5f + 0.5f) / std::log(1.5f);
}

Color rainbowPalette(float normIter, float shift) {
    // Apply shift and wrap to [0,1]
    float phase = std::fmod((normIter * 3.0f) + shift, 1.0f);
    
    // Convert phase to angle in radians (0 to 2π)
    float angle = phase * 2.0f * M_PI;
    
    // Calculate RGB components using sine waves (120° phase shifts)
    float r = std::sin(angle) * 0.5f + 0.5f;
    float g = std::sin(angle + 2.0f * M_PI / 3.0f) * 0.5f + 0.5f;
    float b = std::sin(angle + 4.0f * M_PI / 3.0f) * 0.5f + 0.5f;
    
    // Scale to enhance colors
    r = std::min(r * 1.5f, 1.0f);
    g = std::min(g * 1.5f, 1.0f);
    b = std::min(b * 1.5f, 1.0f);
    
    return {r, g, b};
}

Color firePalette(float normIter, float shift) {
    float phase = std::fmod(normIter + shift, 1.0f);
    
    // Create a fire-like gradient from black through red to yellow
    float r = std::min(phase * 2.0f, 1.0f);
    float g = std::max(0.0f, std::min((phase - 0.3f) * 2.0f, 1.0f));
    float b = 0.0f;
    
    return {r, g, b};
}

Color electricBlue(float normIter, float shift) {
    float phase = std::fmod(normIter + shift, 1.0f);
    
    // Create an electric blue gradient
    float r = 0.0f;
    float g = std::min(phase * 2.0f, 1.0f);
    float b = std::min(phase * 2.5f, 1.0f);
    
    return {r, g, b};
}

Color twilightPalette(float normIter, float shift) {
    float phase = std::fmod(normIter + shift, 1.0f);
    
    // Create a twilight gradient from deep blue to purple
    float r = std::min(phase * 1.5f, 1.0f);
    float g = 0.0f;
    float b = std::min(phase * 2.0f, 1.0f);
    
    return {r, g, b};
}

Color neonPalette(float normIter, float shift) {
    float phase = std::fmod(normIter + shift, 1.0f);
    
    // Create a neon gradient
    float r = std::sin(phase * M_PI) * 0.5f + 0.5f;
    float g = std::cos(phase * M_PI) * 0.5f + 0.5f;
    float b = std::sin(phase * M_PI + M_PI/3.0f) * 0.5f + 0.5f;
    
    return {r, g, b};
}

Color vintageSepia(float normIter, float shift) {
    float phase = std::fmod(normIter + shift, 1.0f);
    
    // Create a sepia tone gradient
    float r = std::min(phase * 1.2f, 1.0f);
    float g = std::min(phase * 1.1f, 1.0f);
    float b = std::min(phase * 0.9f, 1.0f);
    
    return {r, g, b};
}

} // namespace ColorPalettes 