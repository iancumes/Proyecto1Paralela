cmake_minimum_required(VERSION 3.16)

project(Plinko3DParalelo VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_library(plinko_physics
    src/Ball.cpp
    src/BallThreadSystem.cpp
)

target_include_directories(plinko_physics PUBLIC include)
target_link_libraries(plinko_physics PUBLIC Threads::Threads)

add_executable(plinko3d
    src/main.cpp
    src/Renderer.cpp
)

target_include_directories(plinko3d PRIVATE include)
target_link_libraries(plinko3d PRIVATE plinko_physics SDL2::SDL2 OpenGL::GL)

if(TARGET SDL2::SDL2main)
    target_link_libraries(plinko3d PRIVATE SDL2::SDL2main)
endif()

if(MSVC)
    target_compile_options(plinko_physics PRIVATE /W4)
    target_compile_options(plinko3d PRIVATE /W4)
else()
    target_compile_options(plinko_physics PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(plinko3d PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(ball_thread_system_tests tests/BallThreadSystemTests.cpp)
    target_link_libraries(ball_thread_system_tests PRIVATE plinko_physics)
    add_test(NAME ball_thread_system_equivalence COMMAND ball_thread_system_tests)
endif()
