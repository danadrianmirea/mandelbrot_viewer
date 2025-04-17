#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>
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
#define DEFAULT_MAX_ITERATIONS 200

// Constants for UI
const int PANEL_WIDTH = 230;
const int PANEL_HEIGHT = 315;  // Reduced by 30% from 450
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
bool viewMenuOpen = false;  // New: View menu state
bool helpMenuOpen = false;  // New: Help menu state
bool isMaximized = false;   // New: Window maximized state
const int MENU_HEIGHT = 20;
const int MENU_ITEM_HEIGHT = 20;
const int MENU_ITEM_PADDING = 5;
const int MENU_ITEM_SAVE = 2;
const int MENU_ITEM_LOAD = 3;
const int MENU_ITEM_ABOUT = 6;  // Changed to 6 to avoid conflicts with other menu items

// Add after the existing menu state variables
std::string lastFilename = "";  // Persistent filename for both save and load dialogs

// Add after other timer variables
Uint32 dialogCloseTime = 0;  // Time when dialog was closed
const Uint32 DIALOG_CLOSE_DELAY = 100;  // Delay in milliseconds to ignore input after dialog close

// Add after other state variables
bool pendingPopup = false;  // Flag to indicate a popup should be shown on next frame
int pendingMenuItem = -1;   // Store which menu item was clicked

// Add after other timer variables
Uint32 popupDelayTime = 0;  // Time when menu was closed and popup should be shown
const Uint32 POPUP_DELAY = 100;  // Delay in milliseconds before showing popup

// Add after other constants near the top
const int RENDER_WIDTH = 1920;
const int RENDER_HEIGHT = 1080;

// Add after other menu state variables
bool renderMenuOpen = false;
std::string lastRenderFilename = "render.png";  // Default render filename

// Add after other menu item constants
const int MENU_ITEM_RENDER = 5;  // New constant for render menu item

const std::string colorNames[] = {"Rainbow", "Fire", "Electric Blue", "Twilight", "Neon", "Vintage"};

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
std::string findFontPath(const std::string& fontName);
bool renderHighResImage(const std::string& filename, SDL_Renderer* renderer, 
                       double centerX, double centerY, double zoom, 
                       int maxIterations, int colorMode, double colorShift);
void showAboutDialog(SDL_Renderer* renderer, TTF_Font* font);

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

        // Initialize SDL_image
        if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
            std::cerr << "SDL_image initialization failed: " << IMG_GetError() << std::endl;
            TTF_Quit();
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
        std::string fontPath = findFontPath("arial.ttf");
        if (fontPath.empty()) {
            std::cerr << "Failed to find arial.ttf in any of the search paths" << std::endl;
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return 1;
        }

        TTF_Font* font = TTF_OpenFont(fontPath.c_str(), FONT_SIZE);
        TTF_Font* titleFont = TTF_OpenFont(fontPath.c_str(), TITLE_FONT_SIZE);
        TTF_Font* messageFont = TTF_OpenFont(fontPath.c_str(), MESSAGE_FONT_SIZE);
        
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
                                    viewMenuOpen = false;  // Close View menu if open
                                    helpMenuOpen = false;  // Close Help menu if open
                                    ignoreMouseActions = true;
                                    menuActionTime = SDL_GetTicks();
                                }
                                // Check if View menu was clicked
                                else if (event.button.x >= 180 && event.button.x <= 230) {
                                    viewMenuOpen = !viewMenuOpen;
                                    fileMenuOpen = false;  // Close File menu if open
                                    helpMenuOpen = false;  // Close Help menu if open
                                    ignoreMouseActions = true;
                                    menuActionTime = SDL_GetTicks();
                                }
                                // Check if Help menu was clicked
                                else if (event.button.x >= 310 && event.button.x <= 360) {
                                    helpMenuOpen = !helpMenuOpen;
                                    fileMenuOpen = false;  // Close other menus
                                    viewMenuOpen = false;
                                    renderMenuOpen = false;
                                    ignoreMouseActions = true;
                                    menuActionTime = SDL_GetTicks();
                                }
                                // Check if Render menu was clicked
                                else if (event.button.x >= 240 && event.button.x <= 300) {
                                    renderMenuOpen = !renderMenuOpen;
                                    fileMenuOpen = false;  // Close other menus
                                    viewMenuOpen = false;
                                    helpMenuOpen = false;
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
                                    dialogCloseTime = SDL_GetTicks();
                                } else if (event.button.y >= MENU_HEIGHT && event.button.y < MENU_HEIGHT + MENU_ITEM_HEIGHT * 4) {
                                    // Check if menu items were clicked
                                    if (event.button.x >= 130 && event.button.x <= 230) {
                                        int menuItem = (event.button.y - MENU_HEIGHT) / MENU_ITEM_HEIGHT;
                                        if (menuItem >= 0 && menuItem < 4) {
                                            // Store the menu action to execute after delay
                                            pendingMenuItem = menuItem;
                                            fileMenuOpen = false;
                                            ignoreMouseActions = true;
                                            menuActionTime = SDL_GetTicks();
                                            popupDelayTime = SDL_GetTicks();  // Start popup delay timer
                                        }
                                    }
                                }
                            } else if (viewMenuOpen) {
                                // Check if click is outside menu area
                                if (event.button.y < MENU_HEIGHT || 
                                    event.button.x < 180 || 
                                    event.button.x > 280 || 
                                    event.button.y > MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    viewMenuOpen = false;
                                    dialogCloseTime = SDL_GetTicks();
                                } else if (event.button.y >= MENU_HEIGHT && event.button.y < MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    // Check if menu item was clicked
                                    if (event.button.x >= 180 && event.button.x <= 280) {
                                        isMaximized = !isMaximized;
                                        viewMenuOpen = false;
                                        ignoreMouseActions = true;
                                        menuActionTime = SDL_GetTicks();
                                        popupDelayTime = SDL_GetTicks();

                                        // Store the resize action
                                        pendingMenuItem = 4;  // Use 4 for View menu actions
                                    }
                                }
                            } else if (helpMenuOpen) {
                                // Check if click is outside menu area
                                if (event.button.y < MENU_HEIGHT || 
                                    event.button.x < 310 || 
                                    event.button.x > 410 || 
                                    event.button.y > MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    helpMenuOpen = false;
                                    dialogCloseTime = SDL_GetTicks();
                                } else if (event.button.y >= MENU_HEIGHT && 
                                         event.button.y < MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    // About menu item clicked
                                    if (event.button.x >= 310 && event.button.x <= 410) {
                                        pendingMenuItem = MENU_ITEM_ABOUT;
                                        helpMenuOpen = false;
                                        ignoreMouseActions = true;
                                        menuActionTime = SDL_GetTicks();
                                        popupDelayTime = SDL_GetTicks();
                                    }
                                }
                            } else if (renderMenuOpen) {
                                // Check if click is outside menu area
                                if (event.button.y < MENU_HEIGHT || 
                                    event.button.x < 240 || 
                                    event.button.x > 340 || 
                                    event.button.y > MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    renderMenuOpen = false;
                                    dialogCloseTime = SDL_GetTicks();
                                } else if (event.button.y >= MENU_HEIGHT && 
                                         event.button.y < MENU_HEIGHT + MENU_ITEM_HEIGHT) {
                                    // Image menu item clicked
                                    if (event.button.x >= 240 && event.button.x <= 340) {
                                        pendingMenuItem = MENU_ITEM_RENDER;
                                        renderMenuOpen = false;
                                        ignoreMouseActions = true;
                                        menuActionTime = SDL_GetTicks();
                                        popupDelayTime = SDL_GetTicks();
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
                        if (fileMenuOpen || viewMenuOpen || renderMenuOpen || helpMenuOpen) {
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
                            case SDLK_m:
                                smoothZoomMode = !smoothZoomMode;
                                std::cout << "Zoom mode: " << (smoothZoomMode ? "Smooth" : "Rectangle") << std::endl;
                                break;
                            case SDLK_q:
                                adjustQualityMultiplier(false, highQualityMultiplier, minQualityMultiplier);
                                break;
                            case SDLK_e:
                                adjustQualityMultiplier(true, highQualityMultiplier, minQualityMultiplier);
                                break;
                            case SDLK_r:
                                resetView(centerX, centerY, zoom, maxIterations, viewer);
                                break;
                            case SDLK_h:
                                showUI = !showUI;
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
                        }
                        break;
                        
                    case SDL_KEYUP:
                        switch (event.key.keysym.sym) {
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
                                viewMenuOpen = false;
                                renderMenuOpen = false;
                                helpMenuOpen = false;
                                break;
                        }
                        break;
                }
            }

            // Handle pending popup when delay has elapsed
            if (pendingMenuItem != -1 && SDL_GetTicks() - popupDelayTime > POPUP_DELAY) {
                int menuItem = pendingMenuItem;
                pendingMenuItem = -1;  // Reset pending menu item
                
                switch (menuItem) {
                    case 0: // Reset
                        resetView(centerX, centerY, zoom, maxIterations, viewer);
                        break;
                    case 1: // Save
                        {
                            std::string filename = lastFilename;
                            if (showFileDialog(renderer, font, "Enter filename to save:", filename)) {
                                lastFilename = filename;
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
                                lastFilename = filename;
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
                    case 4: // Maximize/Minimize
                        {
                            if (isMaximized) {
                                // Get the display bounds for the primary display
                                SDL_DisplayMode displayMode;
                                if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
                                    std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
                                    break;
                                }
                                // Leave some margin for taskbars and window borders
                                const int margin = 100;
                                WINDOW_WIDTH = displayMode.w - margin;
                                WINDOW_HEIGHT = displayMode.h - margin;
                            } else {
                                WINDOW_WIDTH = 800;
                                WINDOW_HEIGHT = 600;
                            }
                            
                            // Recreate the window with new size
                            SDL_SetWindowSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);
                            
                            // Center the window on the screen
                            SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                            
                            // Recreate the texture with new size
                            SDL_DestroyTexture(texture);
                            texture = SDL_CreateTexture(
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
                            
                            // Update viewer size
                            viewer.resize(WINDOW_WIDTH, WINDOW_HEIGHT);
                        }
                        break;
                    case MENU_ITEM_RENDER: // Render Image
                        {
                            std::string filename = lastRenderFilename;
                            if (showFileDialog(renderer, font, "Enter filename to save render:", filename)) {
                                lastRenderFilename = filename;
                                if (renderHighResImage(filename, renderer, centerX, centerY, zoom, 
                                                     maxIterations, colorMode, colorShift)) {
                                    std::cout << "High resolution image saved successfully" << std::endl;
                                }
                            }
                        }
                        break;
                    case MENU_ITEM_ABOUT:
                        showAboutDialog(renderer, font);
                        break;
                }
            }

            // Handle continuous zooming in the main loop
            if (smoothZoomMode) {
                Uint32 mouseState = SDL_GetMouseState(&currentX, &currentY);
                // Prevent zooming if menu is open or y is in menu bar
                if (!showMenu || (currentY < 0 || currentY >= MENU_HEIGHT)) {
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
            
            SDL_Color textColor = {255, 255, 255, 255};

            // Draw settings info in top right
            std::string qualityText = highQualityMode ? "HQ " + std::to_string(highQualityMultiplier) + "x" : "Standard";
            
            // Format numbers consistently with fixed precision
            std::stringstream ss;
            ss << std::fixed << std::setprecision(2);
            
            // Create settings text with consistent formatting
            std::string settingsText[] = {
                "Iterations: " + std::to_string(effectiveMaxIter) + " (" + qualityText + ")",
                "Center: (" + std::to_string(static_cast<int>(centerX * 100) / 100.0) + ", " + 
                            std::to_string(static_cast<int>(centerY * 100) / 100.0) + ")",
                "Zoom: " + std::to_string(static_cast<int>(zoom * 100) / 100.0),
                "Color: " + colorNames[colorMode] + " (Shift: " + 
                            std::to_string(static_cast<int>(colorShift * 100) / 100.0) + ")",
                "H for help"
            };
            
            // Pre-calculate all surfaces and find maximum dimensions
            std::vector<SDL_Surface*> surfaces;
            int maxWidth = 0;
            int totalHeight = 0;
            
            for (const auto& text : settingsText) {
                SDL_Surface* surface = TTF_RenderText_Solid(messageFont, text.c_str(), textColor);
                if (surface) {
                    surfaces.push_back(surface);
                    maxWidth = std::max(maxWidth, surface->w);
                    totalHeight += surface->h;
                }
            }
            
            // Calculate spacing
            const int LINE_SPACING = 10;
            const int RIGHT_MARGIN = 10;
            const int NUM_LINES = surfaces.size();
            const int TOTAL_SPACING = (NUM_LINES - 1) * LINE_SPACING;
            const int TOTAL_HEIGHT = totalHeight + TOTAL_SPACING;
            
            // Start y-position to center the text block vertically
            int settingsY = 30;
            
            // Render all text elements
            for (size_t i = 0; i < surfaces.size(); ++i) {
                SDL_Surface* surface = surfaces[i];
                if (surface) {
                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                    if (texture) {
                        SDL_Rect rect;
                        rect.x = WINDOW_WIDTH - maxWidth - RIGHT_MARGIN;
                        rect.y = settingsY;
                        rect.w = surface->w;
                        rect.h = surface->h;
                        
                        SDL_RenderCopy(renderer, texture, nullptr, &rect);
                        SDL_DestroyTexture(texture);
                    }
                    settingsY += surface->h + LINE_SPACING;
                }
            }
            
            // Clean up surfaces
            for (SDL_Surface* surface : surfaces) {
                SDL_FreeSurface(surface);
            }
            
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
        IMG_Quit();  // Cleanup SDL_image

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
    
    // Left panel (Help) - centered vertically
    SDL_Rect leftPanel = {10, (height - PANEL_HEIGHT) / 2, PANEL_WIDTH, PANEL_HEIGHT};
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 200);
    SDL_RenderFillRect(renderer, &leftPanel);
    SDL_SetRenderDrawColor(renderer, 100, 100, 150, 255);
    SDL_RenderDrawRect(renderer, &leftPanel);
    
    // Help text
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Color titleColor = {255, 255, 255, 255};
    SDL_Color infoColor = {220, 220, 255, 255};
    
    std::vector<std::string> helpTexts = {
        "Controls:",
        " ",  
        " ",  
        "M: Toggle zoom mode (smooth/selection)",
        "In Smooth Zoom Mode:",
        "  Left/Right click (hold): Smooth zoom", 
        "  Hold Shift for faster zooming",
        "In Rectangle Selection Mode:",
        "  Left click and drag: Select zoom area",
        "  Right click: Zoom out to previous view",
        "W/A/S/D: Pan the view",
        "C: Change color mode",
        "Z/X: Shift colors left/right",
        "Q/E: Decrease/Increase quality multiplier",
        "R: Reset view",
        "H: Toggle help panels",
    };
    
    // Draw help text - centered vertically
    int yOffset = (height - PANEL_HEIGHT) / 2 + 5;  // 5 pixels padding from top
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

    // Draw View menu
    SDL_Rect viewMenuRect = {180, 0, 50, MENU_HEIGHT};
    SDL_RenderDrawRect(renderer, &viewMenuRect);
    
    // Draw View text
    textSurface = TTF_RenderText_Solid(font, "View", textColor);
    textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    textRect = {185, 2, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    // Draw Render menu
    SDL_Rect renderMenuRect = {240, 0, 60, MENU_HEIGHT};
    SDL_RenderDrawRect(renderer, &renderMenuRect);
    
    // Draw Render text
    textSurface = TTF_RenderText_Solid(font, "Render", textColor);
    textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    textRect = {245, 2, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    // Draw Help menu
    SDL_Rect helpMenuRect = {310, 0, 50, MENU_HEIGHT};
    SDL_RenderDrawRect(renderer, &helpMenuRect);
    
    // Draw Help text
    textSurface = TTF_RenderText_Solid(font, "Help", textColor);
    textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    textRect = {315, 2, textSurface->w, textSurface->h};
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

    // Draw View menu items if open
    if (viewMenuOpen) {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect menuRect = {180, MENU_HEIGHT, 100, MENU_ITEM_HEIGHT};
        SDL_RenderFillRect(renderer, &menuRect);
        
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Draw Maximize/Minimize text
        const char* menuText = isMaximized ? "Minimize" : "Maximize";
        textSurface = TTF_RenderText_Solid(font, menuText, textColor);
        textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        textRect = {185, MENU_HEIGHT + 2, textSurface->w, textSurface->h};
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }

    // Draw Render menu items if open
    if (renderMenuOpen) {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect menuRect = {240, MENU_HEIGHT, 100, MENU_ITEM_HEIGHT};
        SDL_RenderFillRect(renderer, &menuRect);
        
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Draw Image text
        textSurface = TTF_RenderText_Solid(font, "Image", textColor);
        textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        textRect = {245, MENU_HEIGHT + 2, textSurface->w, textSurface->h};
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }

    // Draw Help menu items if open
    if (helpMenuOpen) {
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_Rect menuRect = {310, MENU_HEIGHT, 100, MENU_ITEM_HEIGHT};
        SDL_RenderFillRect(renderer, &menuRect);
        
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Draw About text
        textSurface = TTF_RenderText_Solid(font, "About", textColor);
        textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
        textRect = {315, MENU_HEIGHT + 2, textSurface->w, textSurface->h};
        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
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
        
        // Draw OK text
        SDL_Surface* okSurface = TTF_RenderText_Solid(font, "OK", textColor);
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
        
        // Draw Cancel button
        SDL_SetRenderDrawColor(renderer, 150, 100, 100, 255);
        SDL_Rect cancelButtonRect = {CANCEL_BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT};
        SDL_RenderFillRect(renderer, &cancelButtonRect);
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &cancelButtonRect);
        
        // Draw Cancel text
        SDL_Surface* cancelSurface = TTF_RenderText_Solid(font, "Cancel", textColor);
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
        
        SDL_RenderPresent(renderer);
    }
    
    return result;
}

std::string findFontPath(const std::string& fontName) {
    // Try different possible locations
    std::vector<std::string> possiblePaths = {
        "fonts/" + fontName,           // Current directory
        "../fonts/" + fontName,        // One level up
        "../../fonts/" + fontName,     // Two levels up
        "./fonts/" + fontName,         // Explicit current directory
        "../mandelbrot_viewer/fonts/" + fontName  // From build directory to project root
    };
    
    for (const auto& path : possiblePaths) {
        FILE* file = fopen(path.c_str(), "r");
        if (file) {
            fclose(file);
            std::cout << "Found font at: " << path << std::endl;
            return path;
        }
    }
    
    std::cerr << "Could not find font file: " << fontName << std::endl;
    std::cerr << "Searched in the following locations:" << std::endl;
    for (const auto& path : possiblePaths) {
        std::cerr << "  - " << path << std::endl;
    }
    return "";
}

bool renderHighResImage(const std::string& filename, SDL_Renderer* renderer,
                       double centerX, double centerY, double zoom,
                       int maxIterations, int colorMode, double colorShift) {
    // Create a temporary high-resolution viewer with the exact same parameters
    int effectiveMaxIter = highQualityMode ? maxIterations * highQualityMultiplier : maxIterations;
    MandelbrotViewer highResViewer(RENDER_WIDTH, RENDER_HEIGHT, effectiveMaxIter, colorMode, colorShift);
    
    // Use the same quality settings as the main viewer
    if (highQualityMode) {
        highResViewer.setMaxIterations(effectiveMaxIter);
    }
    
    // Compute the high-resolution frame
    highResViewer.computeFrame(centerX, centerY, zoom);
    
    // Get the image data
    const std::vector<unsigned char>& imageData = highResViewer.getImageData();
    if (imageData.empty()) {
        std::cerr << "Error: Failed to generate high-resolution image data" << std::endl;
        return false;
    }

    // Create an SDL surface from the image data
    SDL_Surface* surface = SDL_CreateRGBSurface(0, RENDER_WIDTH, RENDER_HEIGHT, 24,
        0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (!surface) {
        std::cerr << "Error creating surface: " << SDL_GetError() << std::endl;
        return false;
    }

    // Copy the image data to the surface
    memcpy(surface->pixels, imageData.data(), RENDER_WIDTH * RENDER_HEIGHT * 3);

    // Save the surface as PNG
    if (IMG_SavePNG(surface, filename.c_str()) != 0) {
        std::cerr << "Error saving PNG: " << IMG_GetError() << std::endl;
        SDL_FreeSurface(surface);
        return false;
    }

    SDL_FreeSurface(surface);
    std::cout << "Successfully rendered high-resolution image to: " << filename << std::endl;
    std::cout << "Render parameters:" << std::endl;
    std::cout << "  Resolution: " << RENDER_WIDTH << "x" << RENDER_HEIGHT << std::endl;
    std::cout << "  Center: (" << centerX << ", " << centerY << ")" << std::endl;
    std::cout << "  Zoom: " << zoom << std::endl;
    std::cout << "  Iterations: " << effectiveMaxIter << std::endl;
    std::cout << "  Color mode: " << colorMode << std::endl;
    std::cout << "  Color shift: " << colorShift << std::endl;
    std::cout << "  Quality mode: " << (highQualityMode ? "High" : "Standard") << std::endl;
    if (highQualityMode) {
        std::cout << "  Quality multiplier: " << highQualityMultiplier << "x" << std::endl;
    }
    return true;
}

void showAboutDialog(SDL_Renderer* renderer, TTF_Font* font) {
    const int DIALOG_WIDTH = 500;
    const int DIALOG_HEIGHT = 450;
    const int DIALOG_X = (WINDOW_WIDTH - DIALOG_WIDTH) / 2;
    const int DIALOG_Y = (WINDOW_HEIGHT - DIALOG_HEIGHT) / 2;
    
    // Button dimensions
    const int BUTTON_WIDTH = 100;
    const int BUTTON_HEIGHT = 30;
    const int BUTTON_X = DIALOG_X + (DIALOG_WIDTH - BUTTON_WIDTH) / 2;
    const int BUTTON_Y = DIALOG_Y + DIALOG_HEIGHT - BUTTON_HEIGHT - 20;
    
    bool done = false;
    
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    done = true;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    // Check if click is on OK button
                    if (event.button.x >= BUTTON_X && 
                        event.button.x <= BUTTON_X + BUTTON_WIDTH &&
                        event.button.y >= BUTTON_Y && 
                        event.button.y <= BUTTON_Y + BUTTON_HEIGHT) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
                    // Check if click is outside dialog
                    else if (event.button.x < DIALOG_X || 
                        event.button.x > DIALOG_X + DIALOG_WIDTH ||
                        event.button.y < DIALOG_Y || 
                        event.button.y > DIALOG_Y + DIALOG_HEIGHT) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        dialogCloseTime = SDL_GetTicks();
                        done = true;
                    }
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
        SDL_Surface* titleSurface = TTF_RenderText_Solid(font, "About Mandelbrot Viewer", textColor);
        SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(renderer, titleSurface);
        SDL_Rect titleRect = {DIALOG_X + 10, DIALOG_Y + 10, titleSurface->w, titleSurface->h};
        SDL_RenderCopy(renderer, titleTexture, nullptr, &titleRect);
        SDL_FreeSurface(titleSurface);
        SDL_DestroyTexture(titleTexture);
        
        // Draw about text
        std::vector<std::string> aboutText = {
            "Mandelbrot Viewer is an interactive application for exploring",
            "the Mandelbrot set fractal. It allows you to zoom, pan, and",
            "customize the visualization in real-time.",
            "Controls:",
            " ",
            "  - Left click/Right click (hold): Zoom in at cursor",
            "  - Middle click and drag: Pan the view",
            "  - Mouse wheel: Zoom in/out at cursor position",
            "  - W/A/S/D: Pan the view",
            "  - C: Change color mode",
            "  - Z/X: Shift colors left/right",
            "  - Q/E: Decrease/Increase quality multiplier",
            "  - R: Reset view",
            "  - M: Toggle zoom mode (smooth/selection)",
            "  - H: Toggle help panels",
            "  - P: Print current settings"
        };
        
        int yOffset = DIALOG_Y + 50;
        for (const auto& line : aboutText) {
            SDL_Surface* textSurface = TTF_RenderText_Solid(font, line.c_str(), textColor);
            SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
            SDL_Rect textRect = {DIALOG_X + 20, yOffset, textSurface->w, textSurface->h};
            SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);
            SDL_FreeSurface(textSurface);
            SDL_DestroyTexture(textTexture);
            yOffset += 20;
        }
        
        // Draw OK button
        SDL_SetRenderDrawColor(renderer, 100, 150, 100, 255);
        SDL_Rect buttonRect = {BUTTON_X, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT};
        SDL_RenderFillRect(renderer, &buttonRect);
        
        // Draw button border
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        SDL_RenderDrawRect(renderer, &buttonRect);
        
        // Draw OK text
        SDL_Surface* okSurface = TTF_RenderText_Solid(font, "OK", textColor);
        SDL_Texture* okTexture = SDL_CreateTextureFromSurface(renderer, okSurface);
        SDL_Rect okRect = {
            BUTTON_X + (BUTTON_WIDTH - okSurface->w) / 2,
            BUTTON_Y + (BUTTON_HEIGHT - okSurface->h) / 2,
            okSurface->w,
            okSurface->h
        };
        SDL_RenderCopy(renderer, okTexture, nullptr, &okRect);
        SDL_FreeSurface(okSurface);
        SDL_DestroyTexture(okTexture);
        
        SDL_RenderPresent(renderer);
    }
}