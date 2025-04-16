#pragma once

#include <string>
#include <fstream>
#include <iostream>

struct ViewState {
    double centerX;
    double centerY;
    double zoom;
    int maxIterations;
    int colorMode;
    double colorShift;
    bool highQualityMode;
    int highQualityMultiplier;
    bool adaptiveRenderScale;
    bool smoothZoomMode;
};

bool saveViewState(const std::string& filename, const ViewState& state) {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for writing: " << filename << std::endl;
        return false;
    }

    file.write(reinterpret_cast<const char*>(&state), sizeof(ViewState));
    return file.good();
}

bool loadViewState(const std::string& filename, ViewState& state) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file for reading: " << filename << std::endl;
        return false;
    }

    file.read(reinterpret_cast<char*>(&state), sizeof(ViewState));
    return file.good();
} 