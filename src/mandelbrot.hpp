#pragma once

#include <vector>
#include <string>
#include <CL/cl.h>
#include "color_palettes.hpp"

class MandelbrotViewer {
public:
    MandelbrotViewer(int width, int height, int maxIterations, int colorMode, double colorShift);
    ~MandelbrotViewer();
    
    void computeFrame(double centerX, double centerY, double zoom);
    void setColorMode(int mode);
    void setColorShift(double shift);
    void setMaxIterations(int maxIter);
    int getMaxIterations() const;
    
    const std::vector<unsigned char>& getImageData() const { return imageData; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    void resize(int newWidth, int newHeight);

private:
    void initializeOpenCL();
    void createBuffers();
    void releaseBuffers();
    void compileKernel();
    void updateImage();

    int width;
    int height;
    int maxIterations;
    double scale;
    double offsetX;
    double offsetY;
    double zoom;
    double centerX;
    double centerY;
    int colorMode;
    double colorShift;

    // OpenCL resources
    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_kernel kernel;
    cl_mem iterationsBuffer;
    cl_mem rgbBuffer;
    cl_mem xArrayBuffer;
    cl_mem yArrayBuffer;
    cl_mem imageBuffer;

    std::vector<unsigned char> imageData;
    std::vector<int> iterations;
    std::vector<double> xArray;
    std::vector<double> yArray;

    cl_int err;

    static const std::string kernelSource;
}; 