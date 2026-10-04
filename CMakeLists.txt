# Created by Jacob Hodgkins
cmake_minimum_required(VERSION 3.16)
project(MsPacmanRipper VERSION 1.0 LANGUAGES CXX)

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

# GCC 8 and older expose C++17 filesystem from the separate stdc++fs library.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(MsPacmanRipper PRIVATE stdc++fs)
endif()

set_target_properties(MsPacmanRipper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
foreach(config Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${config}" config_upper)
    set_target_properties(MsPacmanRipper PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${config_upper}" "${CMAKE_BINARY_DIR}/bin"
    )
endforeach()
