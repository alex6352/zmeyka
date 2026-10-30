cmake_minimum_required(VERSION 3.16)
project(zmeyka LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(snake src/main.cpp src/snake.cpp)
target_include_directories(snake PRIVATE include)
target_compile_options(snake PRIVATE -Wall -Wextra -Wpedantic)

# Отдельная тестовая сборка использует встроенный автотест игрового алгоритма.
add_executable(snake_autotest src/main.cpp src/snake.cpp)
target_include_directories(snake_autotest PRIVATE include)
target_compile_definitions(snake_autotest PRIVATE SNAKE_TESTING)
target_compile_options(snake_autotest PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME snake_autotest_seed_1 COMMAND snake_autotest --autotest 1)
add_test(NAME snake_autotest_seed_7 COMMAND snake_autotest --autotest 7)
add_test(NAME snake_autotest_seed_42 COMMAND snake_autotest --autotest 42)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
endif()
if(SDL2_FOUND)
    add_executable(snake_sdl src/main_sdl.cpp src/snake_sdl.cpp)
    target_include_directories(snake_sdl PRIVATE include)
    target_link_libraries(snake_sdl PRIVATE PkgConfig::SDL2)
    target_compile_options(snake_sdl PRIVATE -Wall -Wextra -Wpedantic)
else()
    message(STATUS "SDL2 not found: graphical target snake_sdl will not be built")
endif()
