#include "Ball.h"
#include "Renderer.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int INITIAL_BALL_COUNT = 12;
constexpr float MAX_DELTA_TIME = 0.033F;

std::vector<Ball> createInitialBalls() {
    const std::array<BallColor, 4> colors {
        BallColor::Cyan, BallColor::Magenta, BallColor::Yellow, BallColor::Green
    };

    std::vector<Ball> initialBalls;
    initialBalls.reserve(INITIAL_BALL_COUNT);
    for (int index = 0; index < INITIAL_BALL_COUNT; ++index) {
        const float x = -4.5F + static_cast<float>(index % 6) * 1.8F;
        const float y = 1.5F + static_cast<float>(index / 6) * 2.4F + static_cast<float>(index % 3) * 0.35F;
        const float z = -0.7F + static_cast<float>(index % 3) * 0.7F;
        const float direction = index % 2 == 0 ? 1.0F : -1.0F;
        initialBalls.push_back({
            {x, y, z}, {direction * (0.55F + 0.05F * index), 0.0F, direction * 0.18F},
            0.38F, 1.0F, colors[static_cast<std::size_t>(index) % colors.size()], true
        });
    }
    return initialBalls;
}

bool processEvents(SDL_Window* window) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
            return false;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            resizeRenderer(event.window.data1, event.window.data2);
        }
    }
    return window != nullptr;
}
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "No se pudo iniciar SDL2: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Plinko 3D Paralelo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (window == nullptr) {
        std::cerr << "No se pudo crear la ventana: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        std::cerr << "No se pudo crear el contexto OpenGL: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(1);
    initializeRenderer(WINDOW_WIDTH, WINDOW_HEIGHT);

    // Elementos de la simulación almacenados en memoria.
    std::vector<Ball> balls = createInitialBalls();

    bool running = true;
    auto previousTime = std::chrono::steady_clock::now();
    auto fpsStart = previousTime;
    int frameCount = 0;

    while (running) {
        running = processEvents(window);

        const auto currentTime = std::chrono::steady_clock::now();
        const float dt = std::min(
            std::chrono::duration<float>(currentTime - previousTime).count(), MAX_DELTA_TIME
        );
        previousTime = currentTime;

        updateBallsSerial(balls, dt);
        renderScene(balls);
        SDL_GL_SwapWindow(window);

        ++frameCount;
        const float fpsElapsed = std::chrono::duration<float>(currentTime - fpsStart).count();
        if (fpsElapsed >= 1.0F) {
            const int fps = static_cast<int>(frameCount / fpsElapsed);
            SDL_SetWindowTitle(window, ("Plinko 3D Paralelo | FPS: " + std::to_string(fps)).c_str());
            frameCount = 0;
            fpsStart = currentTime;
        }
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
