#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "mandelbrot.hpp"
#include "view_state.hpp"

// Structure to hold zoom state for smooth transitions
struct ZoomState {
    double centerX;
    double centerY;
    double zoom;
    int maxIterations;
};
std::vector<ZoomState> zoomHistory;

#define FULLSCREEN 0
#define DEFAULT_MAX_ITERATIONS 100

// Constants for UI
const int PANEL_WIDTH = 230;
const int PANEL_HEIGHT = 450;  // Increased from 400 to accommodate all text
const int FONT_SIZE = 12;
const int TITLE_FONT_SIZE = 13;;
const int MESSAGE_FONT_SIZE = 14;

// Color shift constants
const double MIN_COLOR_SHIFT = 0.0;
const double MAX_COLOR_SHIFT = 6.28; // 2 * PI for full color cycle
const double DEFAULT_COLOR_SHIFT = 1.8; // Adjusted for better fire theme appearance

// Window dimensions (will be set in main)
int WINDOW_WIDTH;
int WINDOW_HEIGHT;

// UI state
bool showUI = false;
bool debugMode = false;
bool highQualityMode = true;
bool adaptiveRenderScale = false;
bool smoothZoomMode = true;
bool isDragging = false;
bool isPanning = false;
bool drawing = false;
bool showMenu = true;  // Menu visibility state
bool ignoreMouseActions = false;  // Flag to ignore mouse actions after menu interaction
Uint32 menuActionTime = 0;  // Time when last menu action occurred
const Uint32 MENU_ACTION_DELAY = 100;  // Delay in milliseconds to ignore input after menu action

// Menu state
bool fileMenuOpen = false;
const int MENU_HEIGHT = 20;
const int MENU_ITEM_HEIGHT = 20;
const int MENU_ITEM_PADDING = 5;
const int MENU_ITEM_SAVE = 2;
const int MENU_ITEM_LOAD = 3;

// Add after the existing menu state variables
std::string lastFilename = "";  // Persistent filename for both save and load dialogs

// Add after other timer variables
Uint32 dialogCloseTime = 0;  // Time when dialog was closed
const Uint32 DIALOG_CLOSE_DELAY = 100;  // Delay in milliseconds to ignore input after dialog close

// Function to normalize color shift to [0, 2*PI] range
double normalizeColorShift(double shift) {
    while (shift < MIN_COLOR_SHIFT) shift += MAX_COLOR_SHIFT;
    while (shift >= MAX_COLOR_SHIFT) shift -= MAX_COLOR_SHIFT;
    return shift;
}

// View parameters
double centerX = -0.5;
double centerY = 0.0;
double zoom = 1.5;
int colorMode = 1; // Changed from 0 to 1 for Fire theme
double colorShift = DEFAULT_COLOR_SHIFT;
int maxIterations = DEFAULT_MAX_ITERATIONS;
int highQualityMultiplier = 4;
int minQualityMultiplier = 1;
double renderScale = 1.0;
double minRenderScale = 0.25;
double smoothZoomFactor = 1.01;
double fastSmoothZoomFactor = 1.04;

double panSpeed = 0.01;

// Mouse state
int lastMouseX = 0;
int lastMouseY = 0;
int startX = 0;
int startY = 0;
int currentX = 0;
int currentY = 0;
Uint32 lastZoomTime = 0;  // Track last zoom time
const Uint32 ZOOM_INTERVAL = 10;  // Minimum time between zooms in milliseconds

// Key states for diagonal panning
bool keyPressed[4] = {false, false, false, false}; // up, down, left, right

// Function declarations
void drawUI(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* titleFont, TTF_Font* messageFont, int width, int height);
void drawMenu(SDL_Renderer* renderer, TTF_Font* font, int width);
void drawSelectionRectangle(SDL_Renderer* renderer, int startX, int startY, int currentX, int currentY);
void zoomToSelection(int startX, int startY, int currentX, int currentY, double& centerX, double& centerY, double& zoom);
void smoothZoomToCursor(bool zoomOut, int mouseX, int mouseY, double& centerX, double& centerY, double& zoom);
void panView(bool& isPanning, double& centerX, double& centerY, double zoom);
void toggleQualityMode(bool& highQualityMode, int& maxIterations, int highQualityMultiplier);
void adjustQualityMultiplier(bool increase, int& highQualityMultiplier, int minQualityMultiplier);
void resetView(double& centerX, double& centerY, double& zoom, int& maxIterations, MandelbrotViewer& viewer);
void saveViewToHistory(double centerX, double& centerY, double& zoom, int maxIterations);
void zoomOut(double& centerX, double& centerY, double& zoom, int& maxIterations, MandelbrotViewer& viewer);
bool showFileDialog(SDL_Renderer* renderer, TTF_Font* font, const std::string& title, std::string& filename);

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting Mandelbrot Viewer..." << std::endl;
        
        std::cout << "Initializing SDL..." << std::endl;
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return 1;
        }

        // Initialize SDL_ttf
        if (TTF_Init() < 0) {
            std::cerr << "SDL_ttf initialization failed: " << TTF_GetError() << std::endl;
            SDL_Quit();
            return 1;
        }

        // Get the display bounds for the primary display
        SDL_DisplayMode displayMode;
        if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
            std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        // Leave some margin for taskbars and window borders
        const int margin = 100;
#if FULLSCREEN==1
        WINDOW_WIDTH = displayMode.w - margin;
        WINDOW_HEIGHT = displayMode.h - margin;
#else
        WINDOW_WIDTH = 800;
        WINDOW_HEIGHT = 600;
#endif



        std::cout << "Creating window..." << std::endl;
        SDL_Window* window = SDL_CreateWindow(
            "Mandelbrot Viewer",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            WINDOW_WIDTH,
            WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN
        );

        if (!window) {
            std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        SDL_Texture* texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_RGB24,
            SDL_TEXTUREACCESS_STREAMING,
            WINDOW_WIDTH,
            WINDOW_HEIGHT
        );

        if (!texture) {
            std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        // Load fonts
        TTF_Font* font = TTF_OpenFont("fonts/arial.ttf", FONT_SIZE);
        TTF_Font* titleFont = TTF_OpenFont("fonts/arial.ttf", TITLE_FONT_SIZE);
        TTF_Font* messageFont = TTF_OpenFont("fonts/arial.ttf", MESSAGE_FONT_SIZE);
        
        if (!font || !titleFont || !messageFont) {
            std::cerr << "Failed to load fonts: " << TTF_GetError() << std::endl;
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        std::cout << "Creating Mandelbrot viewer..." << std::endl;
        MandelbrotViewer viewer(WINDOW_WIDTH, WINDOW_HEIGHT, maxIterations, colorMode, colorShift);

        // Save initial view to history
        saveViewToHistory(centerX, centerY, zoom, maxIterations);

        std::cout << "Entering main loop..." << std::endl;
        bool running = true;
        SDL_Event event;

        while (running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_QUIT:
                        running = false;
                        break;

                    case SDL_MOUSEBUTTONDOWN:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            if (showMenu && event.button.y < MENU_HEIGHT) {
                                // Check if File menu was clicked
                                if (event.button.x >= 130 && event.button.x <= 170) {
                                    fileMenuOpen = !fileMenuOpen;
                                    ignoreMouseActions = true;
                                    menuActionTime = SDL_GetTicks();
                                }
                            } else if (fileMenuOpen) {
                                // Check if click is outside menu area
                                if (event.button.y < MENU_HEIGHT || 
                                    event.button.x < 130 || 
                                    event.button.x > 230 || 
                                    event.button.y > MENU_HEIGHT + MENU_ITEM_HEIGHT * 4) {
                                    fileMenuOpen = false;
                                    dialogCloseTime = SDL_GetTicks();  // Set timer when menu is closed by clicking outside
                                } else if (event.button.y >= MENU_HEIGHT && event.button.y < MENU_HEIGHT + MENU_ITEM_HEIGHT * 4) {
                                    // Check if menu items were clicked
                                    if (event.button.x >= 130 && event.button.x <= 230) {
                                        int menuItem = (event.button.y - MENU_HEIGHT) / MENU_ITEM_HEIGHT;
                                        if (menuItem >= 0 && menuItem < 4) {
                                            // Close menu before handling any menu item
                                            fileMenuOpen = false;
                                            ignoreMouseActions = true;
                                            menuActionTime = SDL_GetTicks();
                                            
                                            switch (menuItem) {
                                                case 0: // Reset
                                                    resetView(centerX, centerY, zoom, maxIterations, viewer);
                                                    break;
                                                case 1: // Save
                                                    {
                                                        std::string filename = lastFilename;
                                                        if (showFileDialog(renderer, font, "Enter filename to save:", filename)) {
                                                            lastFilename = filename;  // Update persistent filename
                                                            ViewState state = {
                                                                centerX, centerY, zoom, maxIterations,
                                                                colorMode, colorShift, highQualityMode,
                                                                highQualityMultiplier, adaptiveRenderScale,
                                                                smoothZoomMode
                                                            };
                                                            if (saveViewState(filename.c_str(), state)) {
                                                                std::cout << "View state saved successfully" << std::endl;
                                                            }
                                                        }
                                                    }
                                                    break;
                                                case 2: // Load
                                                    {
                                                        std::string filename = lastFilename;
                                                        if (showFileDialog(renderer, font, "Enter filename to load:", filename)) {
                                                            lastFilename = filename;  // Update persistent filename
                                                            ViewState state;
                                                            if (loadViewState(filename.c_str(), state)) {
                                                                centerX = state.centerX;
                                                                centerY = state.centerY;
                                                                zoom = state.zoom;
                                                                maxIterations = state.maxIterations;
                                                                colorMode = state.colorMode;
                                                                colorShift = state.colorShift;
                                                                highQualityMode = state.highQualityMode;
                                                                highQualityMultiplier = state.highQualityMultiplier;
                                                                adaptiveRenderScale = state.adaptiveRenderScale;
                                                                smoothZoomMode = state.smoothZoomMode;
                                                                
                                                                viewer.setColorMode(colorMode);
                                                                viewer.setColorShift(colorShift);
                                                                viewer.setMaxIterations(highQualityMode ? 
                                                                    maxIterations * highQualityMultiplier : maxIterations);
                                                                
                                                                std::cout << "View state loaded successfully" << std::endl;
                                                            }
                                                        }
                                                    }
                                                    break;
                                                case 3: // Quit
                                                    running = false;
                                                    break;
                                            }
                                        }
                                    }
                                }
                            } else if (!ignoreMouseActions && (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY) && 
                                      (SDL_GetTicks() - dialogCloseTime > DIALOG_CLOSE_DELAY)) {  // Check dialog close timer
                                if (smoothZoomMode) {
                                    // In smooth zoom mode, just update current position
                                    currentX = event.button.x;
                                    currentY = event.button.y;
                                } else {
                                    // Start drawing selection rectangle
                                    drawing = true;
                                    startX = event.button.x;
                                    startY = event.button.y;
                                    currentX = startX;
                                    currentY = startY;
                                }
                            }
                        } else if (event.button.button == SDL_BUTTON_RIGHT) {
                            if (!ignoreMouseActions && (!showMenu || event.button.y >= MENU_HEIGHT) && 
                                (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY) &&
                                (SDL_GetTicks() - dialogCloseTime > DIALOG_CLOSE_DELAY)) {  // Check dialog close timer
                                if (!smoothZoomMode) {
                                    // Zoom out to previous view
                                    zoomOut(centerX, centerY, zoom, maxIterations, viewer);
                                }
                            }
                        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                            if (!ignoreMouseActions && (!showMenu || event.button.y >= MENU_HEIGHT) && 
                                (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY) &&
                                (SDL_GetTicks() - dialogCloseTime > DIALOG_CLOSE_DELAY)) {  // Check dialog close timer
                                // Middle click for panning
                                isDragging = true;
                                lastMouseX = event.button.x;
                                lastMouseY = event.button.y;
                            }
                        }
                        break;

                    case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            if (!ignoreMouseActions && (!showMenu || event.button.y >= MENU_HEIGHT) && 
                                (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY)) {  // Check timer
                                if (drawing) {
                                    // Complete selection rectangle
                                    drawing = false;
                                    if (abs(currentX - startX) > 5 && abs(currentY - startY) > 5) {
                                        zoomToSelection(startX, startY, currentX, currentY, centerX, centerY, zoom);
                                    }
                                }
                            }
                            ignoreMouseActions = false;  // Reset the flag on mouse button up
                        } else if (event.button.button == SDL_BUTTON_MIDDLE) {
                            if (!ignoreMouseActions && (!showMenu || event.button.y >= MENU_HEIGHT) && 
                                (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY)) {  // Check timer
                                // Stop panning
                                isDragging = false;
                            }
                        }
                        break;

                    case SDL_MOUSEMOTION:
                        if (!ignoreMouseActions && (!showMenu || event.motion.y >= MENU_HEIGHT) && 
                            (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY)) {  // Check timer
                            if (isDragging) {
                                // Panning with middle mouse button
                                int currentX = event.motion.x;
                                int currentY = event.motion.y;
                                int deltaX = currentX - lastMouseX;
                                int deltaY = currentY - lastMouseY;
                                
                                // Convert pixel movement to complex plane movement
                                double scale = 4.0 / zoom;
                                centerX -= deltaX * scale / WINDOW_WIDTH;
                                centerY -= deltaY * scale / WINDOW_HEIGHT;  // Inverted y-axis by changing + to -
                                
                                lastMouseX = currentX;
                                lastMouseY = currentY;
                            } else if (drawing) {
                                // Update selection rectangle
                                currentX = event.motion.x;
                                currentY = event.motion.y;
                            }
                        }
                        break;

                    case SDL_MOUSEWHEEL:
                        {
                            int mouseX, mouseY;
                            SDL_GetMouseState(&mouseX, &mouseY);
                            double mouseXPlane = centerX + (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
                            double mouseYPlane = centerY - (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;
                            
                            if (event.wheel.y > 0) {
                                zoom *= 1.1;
                            } else {
                                zoom /= 1.1;
                            }
                            
                            // Adjust center to keep mouse position fixed
                            centerX = mouseXPlane - (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
                            centerY = mouseYPlane + (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;
                            
                            saveViewToHistory(centerX, centerY, zoom, maxIterations);
                        }
                        break;

                    case SDL_KEYDOWN:
                        // Ignore key events when file dialog is open
                        if (fileMenuOpen) {
                            break;
                        }
                        switch (event.key.keysym.sym) {
                            case SDLK_c:
                                colorMode = (colorMode + 1) % 6;
                                viewer.setColorMode(colorMode);
                                break;
                            case SDLK_z:
                                colorShift = normalizeColorShift(colorShift - 0.1);
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_x:
                                colorShift = normalizeColorShift(colorShift + 0.1);
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_i:
                                {
                                    int newMaxIter = maxIterations * 2;
                                    maxIterations = newMaxIter;
                                    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
                                    viewer.setMaxIterations(effectiveMaxIter);
                                }
                                break;
                            case SDLK_o:
                                {
                                    int newMaxIter = maxIterations / 2;
                                    if (newMaxIter > 0) {
                                        maxIterations = newMaxIter;
                                        int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
                                        viewer.setMaxIterations(effectiveMaxIter);
                                    }
                                }
                                break;
                            case SDLK_m:
                                smoothZoomMode = !smoothZoomMode;
                                std::cout << "Zoom mode: " << (smoothZoomMode ? "Smooth" : "Rectangle") << std::endl;
                                break;
                            case SDLK_e:
                                {
                                    // Start smooth zooming in
                                    smoothZoomMode = true;
                                    smoothZoomToCursor(false, currentX, currentY, centerX, centerY, zoom);
                                    saveViewToHistory(centerX, centerY, zoom, maxIterations);
                                }
                                break;
                            case SDLK_q:
                                {
                                    // Start smooth zooming out
                                    smoothZoomMode = true;
                                    smoothZoomToCursor(true, currentX, currentY, centerX, centerY, zoom);
                                    saveViewToHistory(centerX, centerY, zoom, maxIterations);
                                }
                                break;
                            case SDLK_y:
                                toggleQualityMode(highQualityMode, maxIterations, highQualityMultiplier);
                                viewer.setMaxIterations(maxIterations);
                                break;
                            case SDLK_j:
                                adjustQualityMultiplier(false, highQualityMultiplier, minQualityMultiplier);
                                break;
                            case SDLK_k:
                                adjustQualityMultiplier(true, highQualityMultiplier, minQualityMultiplier);
                                break;
                            case SDLK_t:
                                adaptiveRenderScale = !adaptiveRenderScale;
                                std::cout << "Adaptive render scale: " << (adaptiveRenderScale ? "Enabled" : "Disabled") << std::endl;
                                break;
                            case SDLK_v:
                                debugMode = !debugMode;
                                std::cout << "Debug mode: " << (debugMode ? "Enabled" : "Disabled") << std::endl;
                                break;
                            case SDLK_r:
                                resetView(centerX, centerY, zoom, maxIterations, viewer);
                                break;
                            case SDLK_h:
                                showUI = !showUI;
                                break;
                            case SDLK_BACKSPACE:
                                zoomOut(centerX, centerY, zoom, maxIterations, viewer);
                                break;
                            case SDLK_ESCAPE:
                                break;
                            case SDLK_w:
                                keyPressed[0] = true; // up
                                isPanning = true;
                                break;
                            case SDLK_s:
                                keyPressed[1] = true; // down
                                isPanning = true;
                                break;
                            case SDLK_a:
                                keyPressed[2] = true; // left
                                isPanning = true;
                                break;
                            case SDLK_d:
                                keyPressed[3] = true; // right
                                isPanning = true;
                                break;
                            case SDLK_LEFT:
                                colorShift = normalizeColorShift(colorShift - 0.1);
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_RIGHT:
                                colorShift = normalizeColorShift(colorShift + 0.1);
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_UP:
                                {
                                    int newMaxIter = maxIterations * 2;
                                    maxIterations = newMaxIter;
                                    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
                                    viewer.setMaxIterations(effectiveMaxIter);
                                }
                                break;
                            case SDLK_DOWN:
                                {
                                    int newMaxIter = maxIterations / 2;
                                    if (newMaxIter > 0) {
                                        maxIterations = newMaxIter;
                                        int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
                                        viewer.setMaxIterations(effectiveMaxIter);
                                    }
                                }
                                break;
                            case SDLK_p:
                                std::cout << "Current settings: centerX=" << centerX << ", centerY=" << centerY 
                                          << ", zoom=" << zoom << ", maxIterations=" << maxIterations << std::endl;
                                std::cout << "Color mode: " << colorMode << ", Color shift: " << colorShift << std::endl;
                                std::cout << "Zoom history depth: " << zoomHistory.size() << std::endl;
                                break;
                        }
                        break;
                        
                    case SDL_KEYUP:
                        switch (event.key.keysym.sym) {
                            case SDLK_e:
                            case SDLK_q:
                                // Stop smooth zooming
                                smoothZoomMode = false;
                                break;
                            case SDLK_w:
                                keyPressed[0] = false; // up
                                break;
                            case SDLK_s:
                                keyPressed[1] = false; // down
                                break;
                            case SDLK_a:
                                keyPressed[2] = false; // left
                                break;
                            case SDLK_d:
                                keyPressed[3] = false; // right
                                break;
                        }
                        
                        // Check if any direction keys are still pressed
                        isPanning = keyPressed[0] || keyPressed[1] || keyPressed[2] || keyPressed[3];
                        break;

                    case SDL_WINDOWEVENT:
                        switch (event.window.event) {
                            case SDL_WINDOWEVENT_FOCUS_LOST:
                                // Close menu when window loses focus
                                fileMenuOpen = false;
                                break;
                        }
                        break;
                }
            }

            // Handle continuous zooming in the main loop
            if (smoothZoomMode) {
                Uint32 mouseState = SDL_GetMouseState(&currentX, &currentY);
                if (!showMenu || currentY >= MENU_HEIGHT) {
                    if (SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY && 
                        SDL_GetTicks() - dialogCloseTime > DIALOG_CLOSE_DELAY) {  // Check dialog close timer
                        if (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) {
                            // Zoom in while left button is held
                            smoothZoomToCursor(false, currentX, currentY, centerX, centerY, zoom);
                        } else if (mouseState & SDL_BUTTON(SDL_BUTTON_RIGHT)) {
                            // Zoom out while right button is held
                            smoothZoomToCursor(true, currentX, currentY, centerX, centerY, zoom);
                        }
                    }
                }
            }

            // Handle panning
            if (isPanning && SDL_GetTicks() - menuActionTime > MENU_ACTION_DELAY) {  // Check timer
                panView(isPanning, centerX, centerY, zoom);
            }

            // Update render scale based on movement
            if (adaptiveRenderScale) {
                if (isPanning || smoothZoomMode || drawing) {
                    renderScale = minRenderScale;
                } else {
                    renderScale = 1.0;
                }
            } else {
                renderScale = 1.0;
            }

            // Compute frame with current parameters
            int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
            viewer.setMaxIterations(effectiveMaxIter);
            viewer.computeFrame(centerX, centerY, zoom);

            // Update texture
            const std::vector<unsigned char>& imageData = viewer.getImageData();
            if (imageData.empty()) {
                std::cerr << "Error: Image data is empty!" << std::endl;
                continue;
            }

            SDL_UpdateTexture(texture, nullptr, imageData.data(), WINDOW_WIDTH * 3);

            // Draw frame
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            
            // Draw selection rectangle if active
            if (drawing) {
                drawSelectionRectangle(renderer, startX, startY, currentX, currentY);
            }
            
            // Draw UI if enabled
            if (showUI) {
                drawUI(renderer, font, titleFont, messageFont, WINDOW_WIDTH, WINDOW_HEIGHT);
            }
            
            // Draw menu if enabled
            if (showMenu) {
                drawMenu(renderer, font, WINDOW_WIDTH);
            }
            
            // Always draw top message
            SDL_Color textColor = {255, 255, 255, 255};
            SDL_Surface* messageSurface = TTF_RenderText_Solid(messageFont, "Press H to toggle help and debug info", textColor);
            SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(renderer, messageSurface);
            
            SDL_Rect messageRect;
            messageRect.x = (WINDOW_WIDTH - messageSurface->w) / 2;
            messageRect.y = 30;
            messageRect.w = messageSurface->w;
            messageRect.h = messageSurface->h;
            
            SDL_RenderCopy(renderer, messageTexture, nullptr, &messageRect);
            
            SDL_FreeSurface(messageSurface);
            SDL_DestroyTexture(messageTexture);
            
            // Draw iterations count
            std::string iterText = "Iterations: " + std::to_string(effectiveMaxIter);
            SDL_Surface* iterSurface = TTF_RenderText_Solid(messageFont, iterText.c_str(), textColor);
            SDL_Texture* iterTexture = SDL_CreateTextureFromSurface(renderer, iterSurface);
            
            SDL_Rect iterRect;
            iterRect.x = (WINDOW_WIDTH - iterSurface->w) / 2;
            iterRect.y = messageRect.y + messageRect.h + 5;  // Position below the "Press H" text
            iterRect.w = iterSurface->w;
            iterRect.h = iterSurface->h;
            
            SDL_RenderCopy(renderer, iterTexture, nullptr, &iterRect);
            
            SDL_FreeSurface(iterSurface);
            SDL_DestroyTexture(iterTexture);
            
            SDL_RenderPresent(renderer);
        }

        // Clean up
        TTF_CloseFont(font);
        TTF_CloseFont(titleFont);
        TTF_CloseFont(messageFont);
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}

// Function implementations
void drawUI(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* titleFont, TTF_Font* messageFont, int width, int height) {
    // Create semi-transparent background for panels
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    
    // Left panel (Help)
    SDL_Rect leftPanel = {10, height - PANEL_HEIGHT - 60, PANEL_WIDTH, PANEL_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200);
    SDL_RenderFillRect(renderer, &leftPanel);
    SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
    SDL_RenderDrawRect(renderer, &leftPanel);
    
    // Right panel (Settings)
    SDL_Rect rightPanel = {width - PANEL_WIDTH - 10, height - PANEL_HEIGHT - 60, PANEL_WIDTH, PANEL_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200);
    SDL_RenderFillRect(renderer, &rightPanel);
    SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
    SDL_RenderDrawRect(renderer, &rightPanel);
    
    // Help text
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Color titleColor = {255, 255, 255, 255};
    SDL_Color infoColor = {220, 220, 255, 255};
    
    std::vector<std::string> helpTexts = {
        "Controls:",
        " ",  // Added empty line for spacing
        "M: Toggle zoom mode (smooth/selection)",
        smoothZoomMode ? "Zoom Mode: Smooth" : "Zoom Mode: Rectangle Selection",
        "In Smooth Zoom Mode:",
        "  Left click (hold): Zoom in at cursor", 
        "  Right click (hold): Zoom out at cursor",
        "  Hold Shift for faster zooming",
        "In Rectangle Mode:",
        "  Left click and drag: Select zoom area",
        "  Right click: Zoom out to previous view",
        "Panning:",
        "  W: Pan up",
        "  S: Pan down",
        "  A: Pan left",
        "  D: Pan right",
        "E (hold): Smooth zoom in toward cursor",
        "Q (hold): Smooth zoom out from cursor",
        "C: Change color mode",
        "Z/X: Shift colors left/right",
        "I/O: Increase/Decrease iterations", 
        "J/K: Decrease/Increase quality multiplier",
        "T: Toggle adaptive render scaling",
        "Y: Toggle high quality mode",
        "V: Toggle debug mode",
        "R: Reset view",
        "P: Print current settings",
        "Backspace: Zoom out",
        "H: Toggle help panels",
    };
    
    // Settings text
    std::string colorNames[] = {"Rainbow", "Fire", "Electric Blue", "Twilight", "Neon", "Vintage"};
    std::string qualityText = highQualityMode ? "HQ " + std::to_string(highQualityMultiplier) + "x" : "Standard";
    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
    std::string debugText = debugMode ? "Debug ON" : "Debug OFF";
    std::string renderScaleText = adaptiveRenderScale ? "Enabled" : "Disabled";
    
    std::vector<std::string> settingsTexts = {
        "Current Settings:",
        " ",  // Added empty line for spacing
        "Center: (" + std::to_string(centerX) + ", " + std::to_string(centerY) + ")",
        "Zoom: " + std::to_string(zoom),
        "Iterations: " + std::to_string(effectiveMaxIter) + " (" + qualityText + ")",
        "Color: " + colorNames[colorMode] + " (Shift: " + std::to_string(colorShift) + ")",
        "Debug: " + debugText,
        "Resolution: " + std::to_string(width) + "x" + std::to_string(height)
    };
    
    if (adaptiveRenderScale) {
        settingsTexts.push_back("Render Scale: " + std::to_string(static_cast<int>(renderScale * 100)) + "% (" + renderScaleText + ")");
    }
    
    // Draw help text
    int yOffset = height - PANEL_HEIGHT - 55;  // Adjusted to match new panel position
    for (size_t i = 0; i < helpTexts.size(); i++) {
        SDL_Surface* textSurface;
        if (i == 0) {
            textSurface = TTF_RenderText_Solid(titleFont, helpTexts[i].c_str(), titleColor);
        } else {
            textSurface = TTF_RenderText_Solid(font, helpTexts[i].c_str(), infoColor);
        }
        
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        
        SDL_Rect textRect;
        textRect.x = 15;
        textRect.y = yOffset;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;
        
        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
        
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
        
        yOffset += 15;
    }
    
    // Draw settings text
    yOffset = height - PANEL_HEIGHT - 55;  // Adjusted to match new panel position
    for (size_t i = 0; i < settingsTexts.size(); i++) {
        SDL_Surface* textSurface;
        if (i == 0) {
            textSurface = TTF_RenderText_Solid(titleFont, settingsTexts[i].c_str(), titleColor);
        } else {
            textSurface = TTF_RenderText_Solid(font, settingsTexts[i].c_str(), infoColor);
        }
        
        SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        
        SDL_Rect textRect;
        textRect.x = width - PANEL_WIDTH - 5;
        textRect.y = yOffset;
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;
        
        SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
        
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
        
        yOffset += 15;
    }
}

void drawMenu(SDL_Renderer* renderer, TTF_Font* font, int width) {
    if (!showMenu) return;

    // Draw menu bar background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_Rect menuBar = {0, 0, width, MENU_HEIGHT};
    SDL_RenderFillRect(renderer, &menuBar);

    // Draw menu bar border
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawLine(renderer, 0, MENU_HEIGHT, width, MENU_HEIGHT);

    // Draw File menu
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect fileMenuRect = {130, 0, 40, MENU_HEIGHT};
    SDL_RenderDrawRect(renderer, &fileMenuRect);
    
    // Draw File text
    SDL_Color textColor = {0, 0, 0, 255};
    SDL_Surface* textSurface = TTF_RenderText_Solid(font, "File", textColor);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_Rect textRect = {135, 2, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    // Draw File menu items if open
    if (fileMenuOpen) {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect menuRect = {130, MENU_HEIGHT, 100, MENU_ITEM_HEIGHT * 4};
        SDL_RenderFillRect(renderer, &menuRect);
        
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Draw menu items
        const char* menuItems[] = {"Reset", "Save", "Load", "Quit"};
        for (int i = 0; i < 4; i++) {
            textSurface = TTF_RenderText_Solid(font, menuItems[i], textColor);
            textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            textRect = {135, MENU_HEIGHT + 2 + i * MENU_ITEM_HEIGHT, textSurface->w, textSurface->h};
            SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
            SDL_FreeSurface(textSurface);
            SDL_DestroyTexture(textTexture);
        }
    }
}

void drawSelectionRectangle(SDL_Renderer* renderer, int startX, int startY, int currentX, int currentY) {
    // Draw the original rectangle (dimmed)
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect originalRect = {
        std::min(startX, currentX),
        std::min(startY, currentY),
        std::abs(currentX - startX),
        std::abs(currentY - startY)
    };
    SDL_RenderDrawRect(renderer, &originalRect);
    
    // Calculate the square that will be zoomed into
    int centerX = (startX + currentX) / 2;
    int centerY = (startY + currentY) / 2;
    int size = std::max(std::abs(currentX - startX), std::abs(currentY - startY));
    
    // Draw the square (highlighted)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_Rect squareRect = {
        centerX - size / 2,
        centerY - size / 2,
        size,
        size
    };
    SDL_RenderDrawRect(renderer, &squareRect);
    
    // Draw a crosshair at the center
    int crosshairSize = 5;
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    SDL_RenderDrawLine(renderer, centerX - crosshairSize, centerY, centerX + crosshairSize, centerY);
    SDL_RenderDrawLine(renderer, centerX, centerY - crosshairSize, centerX, centerY + crosshairSize);
}

void zoomToSelection(int startX, int startY, int currentX, int currentY, double& centerX, double& centerY, double& zoom) {
    // Calculate the center of the selection
    int centerScreenX = (startX + currentX) / 2;
    int centerScreenY = (startY + currentY) / 2;
    
    // Calculate the size of the selection
    int size = std::max(std::abs(currentX - startX), std::abs(currentY - startY));
    
    // Map screen coordinates to complex plane
    double mouseXPlane = centerX + (centerScreenX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
    double mouseYPlane = centerY - (centerScreenY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;
    
    // Calculate new zoom level
    double zoomFactor = static_cast<double>(WINDOW_WIDTH) / size;
    double newZoom = zoom * zoomFactor;
    
    // Save current view to history
    saveViewToHistory(centerX, centerY, zoom, maxIterations);
    
    // Update view parameters
    centerX = mouseXPlane;
    centerY = mouseYPlane;
    zoom = newZoom;
    
    std::cout << "Zooming to: centerX=" << centerX << ", centerY=" << centerY << ", zoom=" << zoom << std::endl;
}

void smoothZoomToCursor(bool zoomOut, int mouseX, int mouseY, double& centerX, double& centerY, double& zoom) {
    // Get current time
    Uint32 currentTime = SDL_GetTicks();
    
    // Check if enough time has passed since last zoom
    if (currentTime - lastZoomTime < ZOOM_INTERVAL) {
        return;  // Skip this zoom step if not enough time has passed
    }
    
    // Map mouse position to complex plane
    double mouseXPlane = centerX + (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
    double mouseYPlane = centerY + (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;  // Inverted y-axis
    
    // Check if Shift key is being held
    bool shiftPressed = (SDL_GetModState() & KMOD_SHIFT) != 0;
    
    // Choose the appropriate zoom factor based on Shift key state
    double currentZoomFactor = shiftPressed ? fastSmoothZoomFactor : smoothZoomFactor;
    
    // Apply zoom
    if (zoomOut) {
        zoom /= currentZoomFactor;
    } else {
        zoom *= currentZoomFactor;
    }
    
    // Adjust center to keep mouse position fixed
    centerX = mouseXPlane - (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
    centerY = mouseYPlane - (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;  // Inverted y-axis
    
    // Update last zoom time
    lastZoomTime = currentTime;
}

void panView(bool& isPanning, double& centerX, double& centerY, double zoom) {
    // Calculate pan amount based on current view size
    double panAmountX = (4.0 / zoom) * panSpeed;
    double panAmountY = (4.0 / zoom) * panSpeed;
    
    // Apply panning based on key states
    if (keyPressed[0]) { // up
        centerY -= panAmountY;
    }
    if (keyPressed[1]) { // down
        centerY += panAmountY;
    }
    if (keyPressed[2]) { // left
        centerX -= panAmountX;
    }
    if (keyPressed[3]) { // right
        centerX += panAmountX;
    }
}

void toggleQualityMode(bool& highQualityMode, int& maxIterations, int highQualityMultiplier) {
    highQualityMode = !highQualityMode;
    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
    std::cout << (highQualityMode ? "High quality mode enabled: " : "Standard quality mode: ") 
              << effectiveMaxIter << " iterations" << std::endl;
}

void adjustQualityMultiplier(bool increase, int& highQualityMultiplier, int minQualityMultiplier) {
    int oldMultiplier = highQualityMultiplier;
    
    if (increase) {
        highQualityMultiplier = std::min(highQualityMultiplier * 2, 320);
    } else {
        highQualityMultiplier = std::max(highQualityMultiplier / 2, minQualityMultiplier);
    }
    
    if (oldMultiplier != highQualityMultiplier) {
        std::cout << "Quality multiplier " << (increase ? "increased" : "decreased") 
                  << " to " << highQualityMultiplier << "x, iterations now: " << highQualityMultiplier * maxIterations << std::endl;
    }
}

void resetView(double& centerX, double& centerY, double& zoom, int& maxIterations, MandelbrotViewer& viewer) {
    centerX = -0.5;
    centerY = 0.0;
    zoom = 1.0;
    maxIterations = DEFAULT_MAX_ITERATIONS;
    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
    viewer.setMaxIterations(effectiveMaxIter);
    std::cout << "View reset to initial state" << std::endl;
}

void saveViewToHistory(double centerX, double& centerY, double& zoom, int maxIterations) {
    ZoomState state = {centerX, centerY, zoom, maxIterations};
    zoomHistory.push_back(state);
    
    // Limit history size
    if (zoomHistory.size() > 50) {
        zoomHistory.erase(zoomHistory.begin());
    }
}

void zoomOut(double& centerX, double& centerY, double& zoom, int& maxIterations, MandelbrotViewer& viewer) {
    if (zoomHistory.size() > 1) {
        // Remove current view
        zoomHistory.pop_back();
        
        // Get previous view
        ZoomState previousView = zoomHistory.back();
        centerX = previousView.centerX;
        centerY = previousView.centerY;
        zoom = previousView.zoom;
        maxIterations = previousView.maxIterations;
        int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
        viewer.setMaxIterations(effectiveMaxIter);
        
        std::cout << "Zoomed out to: centerX=" << centerX << ", centerY=" << centerY 
                  << ", zoom=" << zoom << std::endl;
    } else {
        std::cout << "No more zoom history available" << std::endl;
    }
}

// Function to show a popup dialog for file operations
bool showFileDialog(SDL_Renderer* renderer, TTF_Font* font, const std::string& title, std::string& filename) {
    const int DIALOG_WIDTH = 400;
    const int DIALOG_HEIGHT = 150;
    const int DIALOG_X = (WINDOW_WIDTH - DIALOG_WIDTH) / 2;
    const int DIALOG_Y = (WINDOW_HEIGHT - DIALOG_HEIGHT) / 2;
    
    // Button dimensions and positions
    const int BUTTON_WIDTH = 80;
    const int BUTTON_HEIGHT = 25;
    const int BUTTON_Y = DIALOG_Y + DIALOG_HEIGHT - BUTTON_HEIGHT - 10;
    const int OK_BUTTON_X = DIALOG_X + DIALOG_WIDTH - BUTTON_WIDTH * 2 - 20;
    const int CANCEL_BUTTON_X = DIALOG_X + DIALOG_WIDTH - BUTTON_WIDTH - 10;
    
    bool done = false;
    bool result = false;
    std::string inputText = filename.empty() ? "": filename;
    
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = true;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    // Check if click is outside dialog
                    if (event.button.x < DIALOG_X || 
                        event.button.x > DIALOG_X + DIALOG_WIDTH ||
                        event.button.y < DIALOG_Y || 
                        event.button.y > DIALOG_Y + DIALOG_HEIGHT) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
                    // Check OK button click
                    else if (event.button.x >= OK_BUTTON_X && 
                             event.button.x <= OK_BUTTON_X + BUTTON_WIDTH &&
                             event.button.y >= BUTTON_Y && 
                             event.button.y <= BUTTON_Y + BUTTON_HEIGHT) {
                        filename = inputText;
                        result = true;
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
                    // Check Cancel button click
                    else if (event.button.x >= CANCEL_BUTTON_X && 
                             event.button.x <= CANCEL_BUTTON_X + BUTTON_WIDTH &&
                             event.button.y >= BUTTON_Y && 
                             event.button.y <= BUTTON_Y + BUTTON_HEIGHT) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_RETURN) {
                        filename = inputText;
                        result = true;
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    } else if (event.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
                        inputText.pop_back();
                    } else if (event.key.keysym.sym == SDLK_c && (event.key.keysym.mod & KMOD_CTRL)) {
                        SDL_SetClipboardText(inputText.c_str());
                    } else if (event.key.keysym.sym == SDLK_v && (event.key.keysym.mod & KMOD_CTRL)) {
                        char* clipboardText = SDL_GetClipboardText();
                        if (clipboardText) {
                            inputText += clipboardText;
                            SDL_free(clipboardText);
                        }
                    }
                    break;
                    
                case SDL_TEXTINPUT:
                    inputText += event.text.text;
                    break;
            }
        }
        
        // Draw dialog background
        SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
        SDL_Rect dialogRect = {DIALOG_X, DIALOG_Y, DIALOG_WIDTH, DIALOG_HEIGHT};
        SDL_RenderFillRect(renderer, &dialogRect);
        
        // Draw dialog border
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &dialogRect);
        
        // Draw title
        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title.c_str(), textColor);
        if (!titleSurface) {
            std::cerr << "Failed to render title text: " << TTF_GetError() << std::endl;
            titleSurface = TTF_RenderText_Solid(font, "Error rendering title", textColor);
            if (!titleSurface) {
                std::cerr << "Critical error: Cannot render any text" << std::endl;
                return false;
            }
        }
        SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
        SDL_Rect titleRect = {DIALOG_X + 10, DIALOG_Y + 10, titleSurface->w, titleSurface->h};
        SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);
        SDL_FreeSurface(titleSurface);
        SDL_DestroyTexture(titleTexture);
        
        // Draw input text background (textbox)
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_Rect inputBoxRect = {DIALOG_X + 10, DIALOG_Y + 45, DIALOG_WIDTH - 20, 25};
        SDL_RenderFillRect(renderer, &inputBoxRect);
        
        // Draw input text
        SDL_Surface* inputSurface = TTF_RenderText_Solid(font, inputText.c_str(), textColor);
        if(inputSurface) {
            SDL_Texture* inputTexture = SDL_CreateTextureFromSurface(renderer, inputSurface);
            SDL_Rect inputRect = {DIALOG_X + 15, DIALOG_Y + 50, inputSurface->w, inputSurface->h};
            SDL_RenderCopy(renderer, inputTexture, nullptr, &inputRect);
            SDL_FreeSurface(inputSurface);
            SDL_DestroyTexture(inputTexture);
        }
        
        // Draw OK button
        SDL_SetRenderDrawColor(renderer, 100, 150, 100, 255);
        SDL_Rect okButtonRect = {OK_BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT};
        SDL_RenderFillRect(renderer, &okButtonRect);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &okButtonRect);
        
        // Draw OK text
        SDL_Surface* okSurface = TTF_RenderText_Solid(font, "OK", textColor);
        if (okSurface) {
            SDL_Texture* okTexture = SDL_CreateTextureFromSurface(renderer, okSurface);
            SDL_Rect okTextRect = {
                OK_BUTTON_X + (BUTTON_WIDTH - okSurface->w) / 2,
                BUTTON_Y + (BUTTON_HEIGHT - okSurface->h) / 2,
                okSurface->w,
                okSurface->h
            };
            SDL_RenderCopy(renderer, okTexture, nullptr, &okTextRect);
            SDL_FreeSurface(okSurface);
            SDL_DestroyTexture(okTexture);
        }
        
        // Draw Cancel button
        SDL_SetRenderDrawColor(renderer, 150, 100, 100, 255);
        SDL_Rect cancelButtonRect = {CANCEL_BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT};
        SDL_RenderFillRect(renderer, &cancelButtonRect);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &cancelButtonRect);
        
        // Draw Cancel text
        SDL_Surface* cancelSurface = TTF_RenderText_Solid(font, "Cancel", textColor);
        if (cancelSurface) {
            SDL_Texture* cancelTexture = SDL_CreateTextureFromSurface(renderer, cancelSurface);
            SDL_Rect cancelTextRect = {
                CANCEL_BUTTON_X + (BUTTON_WIDTH - cancelSurface->w) / 2,
                BUTTON_Y + (BUTTON_HEIGHT - cancelSurface->h) / 2,
                cancelSurface->w,
                cancelSurface->h
            };
            SDL_RenderCopy(renderer, cancelTexture, nullptr, &cancelTextRect);
            SDL_FreeSurface(cancelSurface);
            SDL_DestroyTexture(cancelTexture);
        }
        
        SDL_RenderPresent(renderer);
    }
    
    return result;
}