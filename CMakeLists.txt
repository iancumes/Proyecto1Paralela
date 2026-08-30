cmake_minimum_required(VERSION 3.16)

project(Plinko3DParalelo VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sin un tipo de compilacion explicito se usa Release: medir speedup sobre un
# binario sin optimizar daria numeros que no representan nada.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilacion" FORCE)
endif()

# ---------------------------------------------------------------------------
# OpenMP
#
# AppleClang no trae el runtime de OpenMP, asi que cuando la busqueda estandar
# falla se intenta con la instalacion de Homebrew (libomp) antes de rendirse.
# ---------------------------------------------------------------------------
find_package(OpenMP QUIET)

if(NOT OpenMP_CXX_FOUND AND APPLE)
    find_program(HOMEBREW_EXECUTABLE brew)
    if(HOMEBREW_EXECUTABLE)
        execute_process(
            COMMAND ${HOMEBREW_EXECUTABLE} --prefix libomp
            OUTPUT_VARIABLE LIBOMP_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
    if(NOT LIBOMP_PREFIX)
        foreach(candidate "/opt/homebrew/opt/libomp" "/usr/local/opt/libomp")
            if(EXISTS "${candidate}/include/omp.h")
                set(LIBOMP_PREFIX "${candidate}")
                break()
            endif()
        endforeach()
    endif()
    if(LIBOMP_PREFIX AND EXISTS "${LIBOMP_PREFIX}/include/omp.h")
        message(STATUS "OpenMP: usando libomp de Homebrew en ${LIBOMP_PREFIX}")
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -I${LIBOMP_PREFIX}/include")
        set(OpenMP_CXX_LIB_NAMES "omp")
        find_library(OpenMP_omp_LIBRARY omp HINTS "${LIBOMP_PREFIX}/lib" REQUIRED)
        find_package(OpenMP)
    endif()
endif()

if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP habilitado (${OpenMP_CXX_FLAGS})")
else()
    message(WARNING
        "OpenMP no esta disponible: el programa compilara, pero los modos "
        "'omp-static' y 'omp-tuned' se ejecutaran en un solo hilo. "
        "En macOS instale libomp con 'brew install libomp'.")
endif()

find_package(SDL2 QUIET CONFIG)
if(NOT SDL2_FOUND)
    find_package(SDL2 QUIET MODULE)
endif()
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Biblioteca de fisica: no depende de SDL ni de OpenGL, de modo que el banco de
# pruebas y las pruebas automatizadas pueden enlazarla sin entorno grafico.
# ---------------------------------------------------------------------------
add_library(plinko_physics
    src/Simulation.cpp
    src/BallThreadSystem.cpp
    src/SimulationConfig.cpp
    src/Benchmark.cpp
)

target_include_directories(plinko_physics PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(plinko_physics PUBLIC Threads::Threads)

if(OpenMP_CXX_FOUND)
    target_link_libraries(plinko_physics PUBLIC OpenMP::OpenMP_CXX)
endif()

# ---------------------------------------------------------------------------
# Ejecutable del screensaver.
# ---------------------------------------------------------------------------
add_executable(plinko3d
    src/main.cpp
    src/Renderer.cpp
)

target_include_directories(plinko3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(plinko3d PRIVATE plinko_physics OpenGL::GL)

if(TARGET SDL2::SDL2)
    target_link_libraries(plinko3d PRIVATE SDL2::SDL2)
elseif(SDL2_LIBRARIES)
    target_include_directories(plinko3d PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(plinko3d PRIVATE ${SDL2_LIBRARIES})
else()
    message(FATAL_ERROR
        "No se encontro SDL2. Instalelo con 'brew install sdl2', "
        "'sudo apt install libsdl2-dev' o el gestor de paquetes de su sistema.")
endif()

if(TARGET SDL2::SDL2main)
    target_link_libraries(plinko3d PRIVATE SDL2::SDL2main)
endif()

# ---------------------------------------------------------------------------
# Avisos del compilador: se activan al maximo razonable para detectar errores
# de conversion y variables sin usar antes de la entrega.
# ---------------------------------------------------------------------------
foreach(target plinko_physics plinko3d)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

# En macOS los encabezados de OpenGL en modo compatibilidad estan marcados como
# obsoletos; el aviso se silencia para no ocultar los avisos que si importan.
if(APPLE)
    target_compile_definitions(plinko3d PRIVATE GL_SILENCE_DEPRECATION)
endif()

# ---------------------------------------------------------------------------
# Pruebas automatizadas.
# ---------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    add_executable(simulation_tests tests/SimulationTests.cpp)
    target_link_libraries(simulation_tests PRIVATE plinko_physics)
    target_include_directories(simulation_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

    add_test(NAME equivalencia_modos COMMAND simulation_tests equivalencia)
    add_test(NAME determinismo_hilos COMMAND simulation_tests determinismo)
    add_test(NAME validacion_argumentos COMMAND simulation_tests argumentos)
    add_test(NAME conservacion_conteos COMMAND simulation_tests conteos)
endif()
