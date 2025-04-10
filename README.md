# Mandelbrot Viewer

A GPU-accelerated Mandelbrot set viewer written in C++ using OpenCL and SFML.

## Features

- GPU-accelerated computation using OpenCL
- Smooth coloring with multiple color palettes
- Interactive navigation with mouse
- Color palette cycling and adjustment
- Dynamic iteration count adjustment

## Requirements

- C++17 compatible compiler
- CMake 3.10 or higher
- OpenCL development files
- SDL2 2.30.5 or higher

## Building

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure with CMake:
```bash
cmake ..
```

3. Build the project:
```bash
cmake --build .
```

## Controls

- Left Mouse Button + Drag: Pan the view
- Mouse Wheel: Zoom in/out
- C: Cycle through color palettes
- Left/Right Arrow: Adjust color shift
- Up/Down Arrow: Increase/decrease maximum iterations
- ESC: Close the window

## Color Palettes

1. Rainbow (default)
2. Fire
3. Electric Blue
4. Twilight
5. Neon
6. Vintage Sepia

## License

This project is open source and available under the MIT License. 
