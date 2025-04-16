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

### Navigation
- WASD: Pan the view in any direction
- Mouse Wheel: Zoom in/out
- M: Toggle between smooth zoom and rectangle selection modes
- E (hold): Smooth zoom in toward cursor
- Q (hold): Smooth zoom out from cursor
- Backspace: Zoom out to previous view

### In Smooth Zoom Mode:
- Left click (hold): Zoom in at cursor
- Right click (hold): Zoom out at cursor
- Shift + Left click (hold): Fast zoom in at cursor
- Shift + Right click (hold): Fast zoom out at cursor

### In Rectangle Selection Mode:
- Left click and drag: Select area to zoom into
- Right click: Zoom out to previous view

### Color Controls
- C: Cycle through color palettes
- Z/X: Shift colors left/right
- Left/Right Arrow: Alternative way to shift colors

### Quality Controls
- I/O: Increase/decrease maximum iterations
- Y: Toggle high quality mode
- J/K: Decrease/increase quality multiplier
- T: Toggle adaptive render scaling (reduces resolution during movement)

### Other Controls
- H: Toggle help panels
- P: Print current settings
- R: Reset view
- V: Toggle debug mode

## Color Palettes

1. Rainbow (default)
2. Fire
3. Electric Blue
4. Twilight
5. Neon
6. Vintage Sepia

## License

This project is open source and available under the MIT License. 
