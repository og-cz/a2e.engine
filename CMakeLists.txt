cmake_minimum_required(VERSION 3.20)
project(a2e VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(a2e_engine
    engine/src/agent.cpp
    engine/src/agent_memory.cpp
    engine/src/animation.cpp
    engine/src/application.cpp
    engine/src/audio.cpp
    engine/src/basic_2d_renderer.cpp
    engine/src/camera.cpp
    engine/src/config.cpp
    engine/src/logging.cpp
    engine/src/navigation.cpp
    engine/src/perception.cpp
    engine/src/physics.cpp
    engine/src/scene.cpp
    engine/src/time.cpp
    engine/src/tilemap.cpp
    engine/src/win32_audio.cpp
    engine/src/win32_window.cpp
)
target_include_directories(a2e_engine PUBLIC engine/include)
target_compile_definitions(a2e_engine PRIVATE UNICODE _UNICODE)
if(WIN32)
    target_link_libraries(a2e_engine PRIVATE user32 gdi32 winmm)
endif()

add_executable(a2e_demo examples/basic_scene.cpp)
target_link_libraries(a2e_demo PRIVATE a2e_engine)

add_executable(a2e_tests tests/phase1_tests.cpp)
target_link_libraries(a2e_tests PRIVATE a2e_engine)

add_executable(a2e_ai_tests tests/ai_tests.cpp)
target_link_libraries(a2e_ai_tests PRIVATE a2e_engine)

enable_testing()
add_test(NAME phase1_tests COMMAND a2e_tests)
add_test(NAME ai_tests COMMAND a2e_ai_tests)
