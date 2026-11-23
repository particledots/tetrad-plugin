# The Tetrad engine. Plain C++17, no JUCE, no plugin code.
# Outputs.h holds the MIDI numbers and menus, Waves.h the wave shapes and random sequences, Engine.h the four channels,
# the modes, tempo following and the rect notes, CcStream the smooth 14-bit CC sender (from Strange Drift 0.1.1).

add_library(tetrad_engine STATIC
    src/Engine.cpp
    src/CcStream.cpp
)

target_include_directories(tetrad_engine PUBLIC include)
target_compile_features(tetrad_engine PUBLIC cxx_std_17)

if(TETRAD_BUILD_TESTS)
    add_executable(tetrad_engine_test tests/engine_test.cpp)
    target_link_libraries(tetrad_engine_test PRIVATE tetrad_engine)
    add_test(NAME engine COMMAND tetrad_engine_test)

    # Writes the engine's real MIDI stream to a file; the mapper test (Node.js) feeds it through the Scripter mapper.
    add_executable(tetrad_ccdump tests/ccdump.cpp)
    target_link_libraries(tetrad_ccdump PRIVATE tetrad_engine)

    find_program(NODE_EXE node)
    if(NODE_EXE)
        add_test(NAME mapper
                 COMMAND ${CMAKE_COMMAND} -E env
                         TETRAD_CCDUMP=$<TARGET_FILE:tetrad_ccdump>
                         ${NODE_EXE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/mapper_test.js
                         ${CMAKE_SOURCE_DIR}/tools/mapper/Tetrad-Mapper.js
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)
    endif()
endif()
