cmake_minimum_required(VERSION 3.21)
project(robrun_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_executable(rr_probe
        src/main.cpp
        src/probe.cpp
        src/process.cpp
    )
    target_include_directories(rr_probe PRIVATE src)

    add_executable(rr_overlay
        src/cheat_main.cpp
        src/aim.cpp
        src/esp.cpp
        src/input.cpp
        src/menu.cpp
        src/overlay.cpp
        src/process.cpp
        src/settings.cpp
        src/world.cpp
    )
    target_include_directories(rr_overlay PRIVATE src)
    target_link_libraries(rr_overlay PRIVATE user32 gdi32)

    foreach(target rr_probe rr_overlay)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive-)
            target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        endif()
    endforeach()
endif()

add_executable(rr_tests tests/test_pure.cpp)
target_include_directories(rr_tests PRIVATE src)

add_executable(rr_math_tests tests/test_math.cpp)
target_include_directories(rr_math_tests PRIVATE src)

add_executable(rr_settings_tests tests/test_settings.cpp src/settings.cpp)
target_include_directories(rr_settings_tests PRIVATE src)

add_executable(rr_target_tests tests/test_target.cpp)
target_include_directories(rr_target_tests PRIVATE src)

if(MSVC)
    target_compile_options(rr_tests PRIVATE /W4 /permissive-)
    target_compile_options(rr_math_tests PRIVATE /W4 /permissive-)
    target_compile_options(rr_settings_tests PRIVATE /W4 /permissive-)
    target_compile_options(rr_target_tests PRIVATE /W4 /permissive-)
    target_compile_definitions(rr_settings_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

enable_testing()
add_test(NAME pure COMMAND rr_tests)
add_test(NAME math COMMAND rr_math_tests)
add_test(NAME settings COMMAND rr_settings_tests)
add_test(NAME target COMMAND rr_target_tests)
