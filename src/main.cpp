// ---------------------------------------------------------------------------
// Plinko 3D Paralelo - punto de entrada.
//
// Flujo general del programa:
//   1. Captura y validacion de los argumentos de la linea de comandos.
//   2. Solicitud por consola de los datos que falten (solo si hay terminal).
//   3. Modo banco de pruebas (sin ventana) o modo screensaver.
//   4. Creacion de la ventana SDL2 y del contexto de OpenGL.
//   5. Ciclo principal: eventos -> fisica (secuencial o paralela) -> dibujo.
//   6. Liberacion ordenada de texturas, contexto, ventana y SDL.
// ---------------------------------------------------------------------------
#include "Benchmark.h"
#include "Renderer.h"
#include "Simulation.h"
#include "SimulationConfig.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

constexpr float MAX_DELTA_TIME = 1.0F / 30.0F;   // Techo del paso de integracion.
constexpr double HUD_REFRESH_SECONDS = 0.35;     // Cada cuanto se refresca el HUD.
constexpr double NOTICE_SECONDS = 3.0;           // Duracion de los avisos en pantalla.

// Estado mutable del ciclo principal, agrupado para no usar variables globales.
struct RuntimeState {
    ExecutionMode mode = ExecutionMode::OpenMpTuned; // Modo activo.
    int threadCount = 0;          // 0 = todos los nucleos disponibles.
    int maxThreads = 1;           // Tope reportado por OpenMP.
    bool running = true;          // Falso termina el ciclo principal.
    bool resetRequested = false;  // Solicita regenerar la escena.
    std::string notice;           // Aviso temporal mostrado en el HUD.
    double noticeTimer = 0.0;     // Segundos restantes del aviso.
    // Tiempo de referencia del modo secuencial, usado para estimar el speedup
    // en vivo. Se actualiza cada vez que se ejecuta el modo secuencial.
    double sequentialReferenceMs = 0.0;
};

// Rota entre los cuatro modos disponibles.
ExecutionMode nextMode(ExecutionMode mode) {
    switch (mode) {
        case ExecutionMode::Sequential:   return ExecutionMode::StdThreads;
        case ExecutionMode::StdThreads:   return ExecutionMode::OpenMpStatic;
        case ExecutionMode::OpenMpStatic: return ExecutionMode::OpenMpTuned;
        case ExecutionMode::OpenMpTuned:  return ExecutionMode::Sequential;
    }
    return ExecutionMode::Sequential;
}

// Procesa la cola de eventos de SDL.
// Entradas/Salidas: "state" se modifica segun las teclas pulsadas.
void processEvents(RuntimeState& state) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:
                state.running = false;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    resizeRenderer(event.window.data1, event.window.data2);
                }
                break;

            case SDL_KEYDOWN: {
                if (event.key.repeat != 0) {
                    break;
                }
                const SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE || key == SDLK_q) {
                    state.running = false;
                } else if (key == SDLK_0) {
                    state.mode = ExecutionMode::Sequential;
                } else if (key == SDLK_1) {
                    state.mode = ExecutionMode::StdThreads;
                } else if (key == SDLK_2) {
                    state.mode = ExecutionMode::OpenMpStatic;
                } else if (key == SDLK_3) {
                    state.mode = ExecutionMode::OpenMpTuned;
                } else if (key == SDLK_SPACE) {
                    state.mode = nextMode(state.mode);
                } else if (key == SDLK_PLUS || key == SDLK_EQUALS || key == SDLK_KP_PLUS) {
                    const int current = state.threadCount > 0 ? state.threadCount : state.maxThreads;
                    state.threadCount = std::min(current + 1, state.maxThreads);
                } else if (key == SDLK_MINUS || key == SDLK_KP_MINUS) {
                    const int current = state.threadCount > 0 ? state.threadCount : state.maxThreads;
                    state.threadCount = std::max(current - 1, 1);
                } else if (key == SDLK_r) {
                    state.resetRequested = true;
                }
                break;
            }

            default:
                break;
        }
    }
}

// Guarda el contenido del framebuffer en un archivo BMP.
// Entradas: "path" ruta destino; "width"/"height" tamano del lienzo.
// Devuelve true si el archivo se escribio correctamente.
bool saveScreenshot(const std::string& path, int width, int height) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    SDL_Surface* surface = SDL_CreateRGBSurface(0, width, height, 24,
                                                0x0000FF, 0x00FF00, 0xFF0000, 0);
    if (surface == nullptr) {
        std::cerr << "No se pudo crear la superficie de captura: " << SDL_GetError() << '\n';
        return false;
    }

    // OpenGL entrega las filas de abajo hacia arriba; se invierten al copiar.
    for (int row = 0; row < height; ++row) {
        std::uint8_t* destination =
            static_cast<std::uint8_t*>(surface->pixels) + static_cast<std::size_t>(row) * surface->pitch;
        const std::uint8_t* source =
            pixels.data() + static_cast<std::size_t>(height - 1 - row) * width * 3;
        std::copy(source, source + static_cast<std::size_t>(width) * 3, destination);
    }

    const bool saved = SDL_SaveBMP(surface, path.c_str()) == 0;
    if (!saved) {
        std::cerr << "No se pudo guardar la captura: " << SDL_GetError() << '\n';
    }
    SDL_FreeSurface(surface);
    return saved;
}

}  // namespace

int main(int argc, char** argv) {
    // --- 1. Captura y validacion de argumentos -----------------------------
    AppConfig config;
    std::string errorMessage;
    if (!parseCommandLine(argc, argv, config, errorMessage)) {
        std::cerr << "Error en los argumentos: " << errorMessage << "\n\n"
                  << usageText(argv[0]);
        return 1;
    }
    if (config.showHelp) {
        std::cout << usageText(argv[0]);
        return 0;
    }

    // --- 2. Modo banco de pruebas: no requiere ventana ---------------------
    if (config.runBenchmark) {
        return runBenchmark(config);
    }

    // --- 3. Inicializacion de SDL2 y OpenGL --------------------------------
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "No se pudo iniciar SDL2: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow(
        "Plinko 3D Paralelo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        config.windowWidth, config.windowHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window == nullptr) {
        std::cerr << "No se pudo crear la ventana: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (context == nullptr) {
        std::cerr << "No se pudo crear el contexto de OpenGL: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_GL_SetSwapInterval(config.vsync ? 1 : 0);

    // El lienzo real puede ser mayor que el solicitado en pantallas HiDPI.
    int drawableWidth = config.windowWidth;
    int drawableHeight = config.windowHeight;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    initializeRenderer(drawableWidth, drawableHeight);

    // --- 4. Construccion de la escena --------------------------------------
    Simulation simulation(config.simulation);

    RuntimeState state;
    state.mode = config.mode;
    state.threadCount = config.threadCount;
#ifdef _OPENMP
    state.maxThreads = std::max(1, omp_get_max_threads());
#else
    state.maxThreads = 1;
    state.notice = "COMPILADO SIN OPENMP: LOS MODOS OMP CORREN EN 1 HILO";
    state.noticeTimer = 8.0;
#endif
    if (state.threadCount > state.maxThreads) {
        state.threadCount = state.maxThreads;
    }

    std::cout << "Plinko 3D Paralelo\n"
              << "  N = " << config.simulation.ballCount
              << " | clavijas = " << simulation.pegs().size()
              << " | modificadores = " << simulation.modifiers().size()
              << " | casillas = " << config.simulation.binCount << '\n'
              << "  lienzo = " << config.windowWidth << "x" << config.windowHeight
              << " | modo inicial = " << executionModeName(state.mode)
              << " | hilos maximos = " << state.maxThreads << '\n';

    // --- 5. Ciclo principal -------------------------------------------------
    HudInfo hud;
    hud.ballCount = config.simulation.ballCount;

    auto previousTime = std::chrono::steady_clock::now();
    auto hudTimer = previousTime;
    int framesInWindow = 0;
    double physicsAccumulatorMs = 0.0;
    double frameAccumulatorMs = 0.0;
    int screenshotCountdown = config.screenshotPath.empty() ? -1 : config.screenshotWarmupFrames;

    while (state.running) {
        const auto frameStart = std::chrono::steady_clock::now();
        processEvents(state);

        if (state.resetRequested) {
            state.resetRequested = false;
            simulation.reset(static_cast<std::uint32_t>(
                std::chrono::steady_clock::now().time_since_epoch().count()));
            state.notice = "ESCENA REINICIADA";
            state.noticeTimer = NOTICE_SECONDS;
        }

        // Delta time acotado: si el sistema operativo suspende el proceso, un
        // delta enorme haria atravesar las paredes a todas las pelotas.
        float deltaTime = std::chrono::duration<float>(frameStart - previousTime).count();
        deltaTime = std::clamp(deltaTime, 0.0F, MAX_DELTA_TIME);
        previousTime = frameStart;
        // Durante la captura se usa un paso fijo para que el resultado sea
        // reproducible entre ejecuciones.
        if (screenshotCountdown >= 0) {
            deltaTime = 1.0F / 60.0F;
        }

        const auto physicsStart = std::chrono::steady_clock::now();
        simulation.step(deltaTime, state.mode, state.threadCount);
        const auto physicsEnd = std::chrono::steady_clock::now();
        const double physicsMs =
            std::chrono::duration<double, std::milli>(physicsEnd - physicsStart).count();
        physicsAccumulatorMs += physicsMs;

        // Aviso cuando el modo de un hilo por pelota no pudo activarse.
        if (state.mode == ExecutionMode::StdThreads && !simulation.stdThreadsAvailable()) {
            state.notice = "UN HILO POR PELOTA NO DISPONIBLE CON ESTE N";
            state.noticeTimer = NOTICE_SECONDS;
            state.mode = ExecutionMode::OpenMpTuned;
        }

        // OpenGL solo lee el estado despues de que la fisica termino y la
        // barrera del modo paralelo libero a todos los hilos.
        renderScene(simulation, hud);
        SDL_GL_SwapWindow(window);

        ++framesInWindow;
        const auto frameEnd = std::chrono::steady_clock::now();
        frameAccumulatorMs +=
            std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

        // Captura diferida: se espera a que la escena este poblada.
        if (screenshotCountdown >= 0 && --screenshotCountdown < 0) {
            SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
            const bool saved = saveScreenshot(config.screenshotPath, drawableWidth, drawableHeight);
            std::cout << (saved ? "Captura guardada en " : "Fallo la captura en ")
                      << config.screenshotPath << '\n';
            state.running = false;
        }

        const double sinceHudUpdate =
            std::chrono::duration<double>(frameEnd - hudTimer).count();
        if (sinceHudUpdate >= HUD_REFRESH_SECONDS) {
            hud.framesPerSecond = framesInWindow / sinceHudUpdate;
            hud.physicsMilliseconds = physicsAccumulatorMs / framesInWindow;
            hud.frameMilliseconds = frameAccumulatorMs / framesInWindow;
            hud.modeName = executionModeName(state.mode);
            hud.ballCount = static_cast<int>(simulation.balls().size());
            hud.recycled = simulation.recycledBalls();
            hud.loadImbalance = simulation.lastLoadImbalance();
            hud.threads = (state.mode == ExecutionMode::Sequential)
                              ? 1
                              : simulation.lastOpenMpThreads();

            if (state.mode == ExecutionMode::Sequential) {
                state.sequentialReferenceMs = hud.physicsMilliseconds;
                hud.speedupEstimate = 1.0;
            } else if (state.sequentialReferenceMs > 0.0 && hud.physicsMilliseconds > 0.0) {
                hud.speedupEstimate = state.sequentialReferenceMs / hud.physicsMilliseconds;
            } else {
                hud.speedupEstimate = 0.0;
            }

            state.noticeTimer = std::max(0.0, state.noticeTimer - sinceHudUpdate);
            hud.notice = state.noticeTimer > 0.0 ? state.notice : std::string();

            // El titulo repite las cifras principales para quien ejecute el
            // programa con la ventana minimizada o grabando la pantalla.
            char title[256];
            std::snprintf(title, sizeof(title),
                          "Plinko 3D Paralelo | %s | N=%d | hilos=%d | %.1f FPS | fisica %.2f ms",
                          hud.modeName.c_str(), hud.ballCount, hud.threads,
                          hud.framesPerSecond, hud.physicsMilliseconds);
            SDL_SetWindowTitle(window, title);

            framesInWindow = 0;
            physicsAccumulatorMs = 0.0;
            frameAccumulatorMs = 0.0;
            hudTimer = frameEnd;
        }
    }

    // --- 6. Liberacion ordenada de recursos --------------------------------
    std::cout << "Pelotas recicladas durante la sesion: "
              << simulation.recycledBalls() << '\n';

    shutdownRenderer();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
