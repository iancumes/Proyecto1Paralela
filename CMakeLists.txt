cmake_minimum_required(VERSION 3.16)

project(Plinko3DParalelo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(plinko3d
    src/main.cpp
    src/Ball.cpp
    src/Renderer.cpp
)

target_include_directories(plinko3d PRIVATE include)
target_link_libraries(plinko3d PRIVATE SDL2::SDL2 OpenGL::GL)

if(TARGET SDL2::SDL2main)
    target_link_libraries(plinko3d PRIVATE SDL2::SDL2main)
endif()

if(MSVC)
    target_compile_options(plinko3d PRIVATE /W4)
else()
    target_compile_options(plinko3d PRIVATE -Wall -Wextra -Wpedantic)
endif()
