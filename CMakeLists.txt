cmake_minimum_required(VERSION 3.10)
project(mandelbrot_viewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define OpenCL version
add_definitions(-DCL_TARGET_OPENCL_VERSION=120)

# Set SDL2 paths explicitly
set(SDL2_ROOT "D:/SDL2-2.30.5" CACHE PATH "SDL2 root directory")
set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")
set(SDL2_LIBRARY_DIR "${SDL2_ROOT}/lib/x64")

# Find required packages
find_package(OpenCL REQUIRED)

# Add source files
set(SOURCES
    src/main.cpp
    src/mandelbrot.cpp
    src/color_palettes.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCL_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIR}
)

# Link directories
target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${SDL2_LIBRARY_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    OpenCL::OpenCL
    SDL2main
    SDL2
)

# Copy SDL2 DLL to output directory if it exists
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_ROOT}/lib/x64/SDL2.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()