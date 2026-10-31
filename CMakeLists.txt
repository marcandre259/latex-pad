cmake_minimum_required(VERSION 3.22)
project(LatexPad VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_library(latexpad-core src/renderer.cpp src/window.cpp)
target_include_directories(latexpad-core PUBLIC src)
target_link_libraries(latexpad-core PUBLIC Qt6::Widgets Qt6::Svg)
target_compile_options(latexpad-core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(latex-pad src/main.cpp)
target_link_libraries(latex-pad PRIVATE latexpad-core)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(latex-pad-tests tests/app_test.cpp)
    target_link_libraries(latex-pad-tests PRIVATE latexpad-core Qt6::Test)
    add_test(NAME app COMMAND latex-pad-tests)
    add_executable(latex-pad-export-check tests/export_check.cpp)
    target_link_libraries(latex-pad-export-check PRIVATE latexpad-core)
endif()
