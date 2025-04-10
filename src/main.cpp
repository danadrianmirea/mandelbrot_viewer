#include <SDL2/SDL.h>
#include <iostream>
#include <string>
#include "mandelbrot.hpp"

int main(int argc, char* argv[]) {
    try {
        std::cout << "Starting Mandelbrot Viewer..." << std::endl;
        
        const int WINDOW_WIDTH = 800;
        const int WINDOW_HEIGHT = 600;
        const int MAX_ITERATIONS = 1000;

        std::cout << "Initializing SDL..." << std::endl;
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return 1;
        }

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
            SDL_Quit();
            return 1;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
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
            SDL_Quit();
            return 1;
        }

        std::cout << "Creating Mandelbrot viewer..." << std::endl;
        MandelbrotViewer viewer(WINDOW_WIDTH, WINDOW_HEIGHT, MAX_ITERATIONS);

        // Initial view parameters
        double centerX = -0.5;
        double centerY = 0.0;
        double zoom = 1.0;
        int colorMode = 0;
        double colorShift = 0.0;

        // Mouse state for zooming
        bool isDragging = false;
        int lastMouseX = 0;
        int lastMouseY = 0;
        double zoomFactor = 1.1;

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
                            isDragging = true;
                            lastMouseX = event.button.x;
                            lastMouseY = event.button.y;
                        }
                        break;

                    case SDL_MOUSEBUTTONUP:
                        if (event.button.button == SDL_BUTTON_LEFT) {
                            isDragging = false;
                        }
                        break;

                    case SDL_MOUSEMOTION:
                        if (isDragging) {
                            int currentX = event.motion.x;
                            int currentY = event.motion.y;
                            int deltaX = currentX - lastMouseX;
                            int deltaY = currentY - lastMouseY;
                            
                            // Convert pixel movement to complex plane movement
                            double scale = 4.0 / zoom;
                            centerX -= deltaX * scale / WINDOW_WIDTH;
                            centerY += deltaY * scale / WINDOW_HEIGHT;
                            
                            lastMouseX = currentX;
                            lastMouseY = currentY;
                        }
                        break;

                    case SDL_MOUSEWHEEL:
                        {
                            int mouseX, mouseY;
                            SDL_GetMouseState(&mouseX, &mouseY);
                            double mouseXPlane = centerX + (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
                            double mouseYPlane = centerY - (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;
                            
                            if (event.wheel.y > 0) {
                                zoom *= zoomFactor;
                            } else {
                                zoom /= zoomFactor;
                            }
                            
                            // Adjust center to keep mouse position fixed
                            centerX = mouseXPlane - (mouseX - WINDOW_WIDTH/2.0) * (4.0/zoom) / WINDOW_WIDTH;
                            centerY = mouseYPlane + (mouseY - WINDOW_HEIGHT/2.0) * (4.0/zoom) / WINDOW_HEIGHT;
                        }
                        break;

                    case SDL_KEYDOWN:
                        switch (event.key.keysym.sym) {
                            case SDLK_c:
                                colorMode = (colorMode + 1) % 6;
                                viewer.setColorMode(colorMode);
                                break;
                            case SDLK_LEFT:
                                colorShift -= 0.1;
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_RIGHT:
                                colorShift += 0.1;
                                viewer.setColorShift(colorShift);
                                break;
                            case SDLK_UP:
                                {
                                    int newMaxIter = viewer.getMaxIterations() * 2;
                                    viewer.setMaxIterations(newMaxIter);
                                }
                                break;
                            case SDLK_DOWN:
                                {
                                    int newMaxIter = viewer.getMaxIterations() / 2;
                                    if (newMaxIter > 0) {
                                        viewer.setMaxIterations(newMaxIter);
                                    }
                                }
                                break;
                            case SDLK_ESCAPE:
                                running = false;
                                break;
                        }
                        break;
                }
            }

            std::cout << "Computing frame..." << std::endl;
            viewer.computeFrame(centerX, centerY, zoom);

            std::cout << "Updating texture..." << std::endl;
            const std::vector<unsigned char>& imageData = viewer.getImageData();
            if (imageData.empty()) {
                std::cerr << "Error: Image data is empty!" << std::endl;
                continue;
            }

            SDL_UpdateTexture(texture, nullptr, imageData.data(), WINDOW_WIDTH * 3);

            std::cout << "Drawing frame..." << std::endl;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, nullptr, nullptr);
            SDL_RenderPresent(renderer);
        }

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}