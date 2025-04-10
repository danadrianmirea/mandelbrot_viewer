cmake_minimum_required(VERSION 3.10)
project(mandelbrot_viewer VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define OpenCL version
add_definitions(-DCL_TARGET_OPENCL_VERSION=120)

# Set SDL2 paths explicitly
set(SDL2_ROOT "D:/SDL2-2.30.5" CACHE PATH "SDL2 root directory")
set(SDL2_INCLUDE_DIR "${SDL2_ROOT}/include")
set(SDL2_LIBRARY_DIR "${SDL2_ROOT}/lib/x64")

# Set SDL2_ttf paths explicitly
set(SDL2_TTF_ROOT "D:/SDL2_ttf-2.20.2" CACHE PATH "SDL2_ttf root directory")
set(SDL2_TTF_INCLUDE_DIR "${SDL2_TTF_ROOT}/include")
set(SDL2_TTF_LIBRARY_DIR "${SDL2_TTF_ROOT}/lib/x64")

# Find OpenCL package
find_package(OpenCL)
if(NOT OpenCL_FOUND)
    message(FATAL_ERROR "OpenCL not found. Please install OpenCL development files.")
endif()

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
    ${SDL2_TTF_INCLUDE_DIR}
)

# Link directories
target_link_directories(${PROJECT_NAME}
    PRIVATE
    ${SDL2_LIBRARY_DIR}
    ${SDL2_TTF_LIBRARY_DIR}
    ${OpenCL_LIBRARIES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    OpenCL::OpenCL
    SDL2main
    SDL2
    SDL2_ttf
)

# Set output directories
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# Copy SDL2 DLL to output directory if it exists
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_ROOT}/lib/x64/SDL2.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_TTF_ROOT}/lib/x64/SDL2_ttf.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    
    # Only try to copy freetype.dll if it exists
    if(EXISTS "${SDL2_TTF_ROOT}/lib/x64/freetype.dll")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SDL2_TTF_ROOT}/lib/x64/freetype.dll"
                $<TARGET_FILE_DIR:${PROJECT_NAME}>
        )
    endif()

    # Copy fonts directory to build output
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/fonts"
    )
endif()