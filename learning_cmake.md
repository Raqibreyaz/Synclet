<!-- defining minimum version requirement of cmake -->
cmake_minimum_required(VERSION 3.28.0)

<!-- defining project name(doesn't depend on project dir name) -->
project(MyApp)

<!-- setting sources recursively -->
file(GLOB_RECURSE SOURCES "engine/\*.cpp" "main.cpp")

<!-- setting sources individually -->
set(SOURCES
main.cpp
engine/player.cpp
engine/enemy.cpp
)

<!-- creating executable from the source files -->
add_executable(output ${SOURCES})

<!--
running this in terminal, will allow cmake find libraries which vcpkg install
 cmake .. DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake -->

<!-- finds fmt package from vcpkg, REQUIRED means if not found will throw error -->
find_package(fmt CONFIG REQUIRED)

<!-- while building this 'output' exec link the fmt lib in PRIVATE visibility me jisse MyApp ke dependents me fmt na di jaaye, fmt::fmt is an object which gives all things like include-path, lib etc-->
target_link_libraries(output PRIVATE fmt::fmt)

<!-- look in this directries for headers for making the executable -->
target_link_directories(MyApp PRIVATE include)

<!-- if project is distributed in individual directories-->
add_subdirectory(src/player)
add_subdirectory(src/enemy)