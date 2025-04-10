#include "mandelbrot.hpp"
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

const std::string MandelbrotViewer::kernelSource = R"(
    #pragma OPENCL EXTENSION cl_khr_byte_addressable_store : enable

    double3 rainbow_palette(double norm_iter, double shift) {
        double phase = fmod((norm_iter * 3.0) + shift, 1.0);
        double angle = phase * 2.0 * M_PI;
        
        double r = sin(angle) * 0.5 + 0.5;
        double g = sin(angle + 2.0 * M_PI / 3.0) * 0.5 + 0.5;
        double b = sin(angle + 4.0 * M_PI / 3.0) * 0.5 + 0.5;
        
        r = min(r * 1.5, 1.0);
        g = min(g * 1.5, 1.0);
        b = min(b * 1.5, 1.0);
        
        return (double3)(r, g, b);
    }

    double3 fire_palette(double norm_iter, double shift) {
        double phase = fmod(norm_iter + shift, 1.0);
        double r = min(phase * 2.0, 1.0);
        double g = max(0.0, min((phase - 0.3) * 2.0, 1.0));
        return (double3)(r, g, 0.0);
    }

    double3 electric_blue(double norm_iter, double shift) {
        double phase = fmod(norm_iter + shift, 1.0);
        return (double3)(0.0, min(phase * 2.0, 1.0), min(phase * 2.5, 1.0));
    }

    double3 twilight_palette(double norm_iter, double shift) {
        double phase = fmod(norm_iter + shift, 1.0);
        return (double3)(min(phase * 1.5, 1.0), 0.0, min(phase * 2.0, 1.0));
    }

    double3 neon_palette(double norm_iter, double shift) {
        double phase = fmod(norm_iter + shift, 1.0);
        double r = sin(phase * M_PI) * 0.5 + 0.5;
        double g = cos(phase * M_PI) * 0.5 + 0.5;
        double b = sin(phase * M_PI + M_PI/3.0) * 0.5 + 0.5;
        return (double3)(r, g, b);
    }

    double3 vintage_sepia(double norm_iter, double shift) {
        double phase = fmod(norm_iter + shift, 1.0);
        return (double3)(min(phase * 1.2, 1.0), min(phase * 1.1, 1.0), min(phase * 0.9, 1.0));
    }

    double apply_log_smooth(double val) {
        return log(val * 0.5 + 0.5) / log(1.5);
    }

    __kernel void mandelbrot(__global int *iterations_out,
                            __global uchar *rgb_out,
                            __global double *x_array,
                            __global double *y_array,
                            const int width,
                            const int height,
                            const int max_iter,
                            const int color_mode,
                            const double color_shift)
    {
        int gid = get_global_id(0);
        int x = gid % width;
        int y = gid / width;
        
        if (x >= width || y >= height) return;
        
        double x0 = x_array[x];
        double y0 = y_array[y];
        
        double x1 = 0.0;
        double y1 = 0.0;
        double x2 = 0.0;
        double y2 = 0.0;
        
        int iter = 0;
        
        while (x2 + y2 <= 4.0 && iter < max_iter) {
            y1 = 2.0 * x1 * y1 + y0;
            x1 = x2 - y2 + x0;
            x2 = x1 * x1;
            y2 = y1 * y1;
            iter++;
        }
        
        iterations_out[gid] = iter;
        
        if (iter < max_iter) {
            double norm_iter = (double)iter / max_iter;
            norm_iter = apply_log_smooth(norm_iter);
            
            double3 color;
            switch (color_mode) {
                case 0: color = rainbow_palette(norm_iter, color_shift); break;
                case 1: color = fire_palette(norm_iter, color_shift); break;
                case 2: color = electric_blue(norm_iter, color_shift); break;
                case 3: color = twilight_palette(norm_iter, color_shift); break;
                case 4: color = neon_palette(norm_iter, color_shift); break;
                case 5: color = vintage_sepia(norm_iter, color_shift); break;
                default: color = (double3)(0.0, 0.0, 0.0);
            }
            
            int idx = gid * 3;
            rgb_out[idx] = (uchar)(color.x * 255.0);
            rgb_out[idx + 1] = (uchar)(color.y * 255.0);
            rgb_out[idx + 2] = (uchar)(color.z * 255.0);
        } else {
            int idx = gid * 3;
            rgb_out[idx] = 0;
            rgb_out[idx + 1] = 0;
            rgb_out[idx + 2] = 0;
        }
    }
)";

MandelbrotViewer::MandelbrotViewer(int w, int h, int maxIter)
    : width(w), height(h), maxIterations(maxIter), colorMode(0), colorShift(0.0)
{
    std::cout << "Initializing MandelbrotViewer with size " << width << "x" << height << std::endl;
    
    imageData.resize(width * height * 3);
    iterations.resize(width * height);
    xArray.resize(width);
    yArray.resize(height);
    
    try {
        std::cout << "Initializing OpenCL..." << std::endl;
        initializeOpenCL();
        std::cout << "Creating buffers..." << std::endl;
        createBuffers();
        std::cout << "Compiling kernel..." << std::endl;
        compileKernel();
        std::cout << "MandelbrotViewer initialization complete" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error during MandelbrotViewer initialization: " << e.what() << std::endl;
        throw;
    }
}

MandelbrotViewer::~MandelbrotViewer() {
    releaseBuffers();
    clReleaseKernel(kernel);
    clReleaseProgram(program);
    clReleaseCommandQueue(queue);
    clReleaseContext(context);
}

void MandelbrotViewer::initializeOpenCL() {
    cl_platform_id platform;
    cl_device_id device;
    cl_int err;

    // Get platform
    err = clGetPlatformIDs(1, &platform, nullptr);
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to get OpenCL platform. Error code: " << err << std::endl;
        throw std::runtime_error("Failed to get OpenCL platform");
    }

    // Get device
    err = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to get OpenCL device. Error code: " << err << std::endl;
        throw std::runtime_error("Failed to get OpenCL device");
    }

    // Check for double precision support
    cl_device_fp_config fpConfig;
    err = clGetDeviceInfo(device, CL_DEVICE_DOUBLE_FP_CONFIG, sizeof(cl_device_fp_config), &fpConfig, nullptr);
    if (err != CL_SUCCESS || fpConfig == 0) {
        std::cerr << "Device does not support double precision floating point operations" << std::endl;
        throw std::runtime_error("Double precision not supported");
    }

    // Create context
    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to create OpenCL context. Error code: " << err << std::endl;
        throw std::runtime_error("Failed to create OpenCL context");
    }

    // Create command queue
    queue = clCreateCommandQueue(context, device, 0, &err);
    if (err != CL_SUCCESS) {
        std::cerr << "Failed to create command queue. Error code: " << err << std::endl;
        throw std::runtime_error("Failed to create command queue");
    }
}

void MandelbrotViewer::createBuffers() {
    cl_int err;

    iterationsBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY, 
        width * height * sizeof(int), nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create iterations buffer");

    rgbBuffer = clCreateBuffer(context, CL_MEM_WRITE_ONLY,
        width * height * 3 * sizeof(unsigned char), nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create RGB buffer");

    xArrayBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY,
        width * sizeof(double), nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create X array buffer");

    yArrayBuffer = clCreateBuffer(context, CL_MEM_READ_ONLY,
        height * sizeof(double), nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create Y array buffer");
}

void MandelbrotViewer::releaseBuffers() {
    clReleaseMemObject(iterationsBuffer);
    clReleaseMemObject(rgbBuffer);
    clReleaseMemObject(xArrayBuffer);
    clReleaseMemObject(yArrayBuffer);
}

void MandelbrotViewer::compileKernel() {
    cl_int err;
    
    // Add M_PI definition if not available
    std::string sourceWithDefines = "#define M_PI 3.14159265358979323846\n" + kernelSource;
    const char* source = sourceWithDefines.c_str();
    
    program = clCreateProgramWithSource(context, 1, &source, nullptr, &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create program");

    err = clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);
    if (err != CL_SUCCESS) {
        size_t logSize;
        cl_device_id device;
        clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
        std::vector<char> log(logSize);
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, log.data(), nullptr);
        throw std::runtime_error("Failed to build program: " + std::string(log.data()));
    }

    kernel = clCreateKernel(program, "mandelbrot", &err);
    if (err != CL_SUCCESS) throw std::runtime_error("Failed to create kernel");
}

void MandelbrotViewer::computeFrame(double centerX, double centerY, double zoom) {
    try {
        // Calculate coordinate arrays
        double aspectRatio = static_cast<double>(width) / height;
        double scale = 4.0 / zoom;
        
        for (int x = 0; x < width; ++x) {
            xArray[x] = centerX + (x - width/2.0) * scale / width * aspectRatio;
        }
        
        for (int y = 0; y < height; ++y) {
            yArray[y] = centerY + (y - height/2.0) * scale / height;
        }

        // Copy coordinate arrays to device
        cl_int err = clEnqueueWriteBuffer(queue, xArrayBuffer, CL_TRUE, 0,
            width * sizeof(double), xArray.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Failed to write X array. Error code: " << err << std::endl;
            throw std::runtime_error("Failed to write X array");
        }

        err = clEnqueueWriteBuffer(queue, yArrayBuffer, CL_TRUE, 0,
            height * sizeof(double), yArray.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Failed to write Y array. Error code: " << err << std::endl;
            throw std::runtime_error("Failed to write Y array");
        }

        // Set kernel arguments with error checking
        cl_int argErr;
        if ((argErr = clSetKernelArg(kernel, 0, sizeof(cl_mem), &iterationsBuffer)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 1, sizeof(cl_mem), &rgbBuffer)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 2, sizeof(cl_mem), &xArrayBuffer)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 3, sizeof(cl_mem), &yArrayBuffer)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 4, sizeof(int), &width)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 5, sizeof(int), &height)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 6, sizeof(int), &maxIterations)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 7, sizeof(int), &colorMode)) != CL_SUCCESS ||
            (argErr = clSetKernelArg(kernel, 8, sizeof(double), &colorShift)) != CL_SUCCESS) {
            std::cerr << "Failed to set kernel argument. Error code: " << argErr << std::endl;
            throw std::runtime_error("Failed to set kernel argument");
        }

        // Execute kernel
        size_t globalSize = width * height;
        err = clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &globalSize, nullptr, 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Failed to execute kernel. Error code: " << err << std::endl;
            throw std::runtime_error("Failed to execute kernel");
        }

        // Read results
        err = clEnqueueReadBuffer(queue, rgbBuffer, CL_TRUE, 0,
            width * height * 3 * sizeof(unsigned char), imageData.data(), 0, nullptr, nullptr);
        if (err != CL_SUCCESS) {
            std::cerr << "Failed to read RGB buffer. Error code: " << err << std::endl;
            throw std::runtime_error("Failed to read RGB buffer");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error in computeFrame: " << e.what() << std::endl;
        throw;
    }
}

void MandelbrotViewer::setColorMode(int mode) {
    colorMode = mode;
}

void MandelbrotViewer::setColorShift(double shift) {
    colorShift = shift;
}

void MandelbrotViewer::setMaxIterations(int maxIter) {
    maxIterations = maxIter;
}

int MandelbrotViewer::getMaxIterations() const {
    return maxIterations;
} 