# Created by Jacob Hodgkins
cmake_minimum_required(VERSION 3.16)
project(MsPacmanRipper LANGUAGES CXX)

add_executable(MsPacmanRipper
    src/main.cpp
    src/crypto/Hash.cpp
    src/rom/RomSet.cpp
    src/mspacman/Daughterboard.cpp
    src/disasm/Z80Disassembler.cpp
    src/analysis/StatefulAnalyzer.cpp
    src/semantic/SemanticCatalog.cpp
    src/semantic/PacmanSemanticFamilies.cpp
)

target_include_directories(MsPacmanRipper PRIVATE src)
target_compile_features(MsPacmanRipper PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(MsPacmanRipper PRIVATE /W4)
else()
    target_compile_options(MsPacmanRipper PRIVATE -Wall -Wextra -Wpedantic)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set("CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/bin")
endforeach()
