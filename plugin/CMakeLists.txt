# The Tetrad plugin wrapper: what Logic sees (AU / VST3, a MIDI effect) and the panel.
# The four channels and the CC stream live in ../engine.

# The Audio Unit format exists on macOS only; elsewhere (the Linux checking computer) only VST3 is built.
if(APPLE)
    set(TETRAD_FORMATS AU VST3)
else()
    set(TETRAD_FORMATS VST3)
endif()

# A MIDI effect (AU type aumi), set up like Ogdoad and Strange Drift: no audio buses, MIDI in and MIDI out.
# These codes and the bundle id were chosen once and never change (saved projects find the plugin by them).
juce_add_plugin(Tetrad
    COMPANY_NAME                "particledots"
    BUNDLE_ID                   com.particledots.tetrad
    PLUGIN_MANUFACTURER_CODE    Pdot
    PLUGIN_CODE                 Ttrd
    FORMATS                     ${TETRAD_FORMATS}
    PRODUCT_NAME                "Tetrad"
    PLUGIN_NAME                 "Tetrad"
    DESCRIPTION                 "Four linked LFOs as MIDI CC"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx
    AU_MAIN_TYPE                kAudioUnitType_MIDIProcessor
)

set(TETRAD_PLUGIN_SOURCES
    Source/Parameters.h
    Source/Parameters.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

target_sources(Tetrad PRIVATE ${TETRAD_PLUGIN_SOURCES})

target_compile_definitions(Tetrad PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Tetrad
    PRIVATE
        tetrad_engine
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Optional, off by default: a small program that draws the window to picture files and checks the controls, the saved
# state and the MIDI going in and out, without Logic. Build with -DTETRAD_BUILD_PREVIEW=ON.
option(TETRAD_BUILD_PREVIEW "Build the offline window picture and plugin checks" OFF)
if(TETRAD_BUILD_PREVIEW)
    juce_add_console_app(TetradPreview PRODUCT_NAME "TetradPreview")
    target_sources(TetradPreview PRIVATE preview/preview.cpp ${TETRAD_PLUGIN_SOURCES})
    target_include_directories(TetradPreview PRIVATE Source)
    target_compile_definitions(TetradPreview PRIVATE
        JucePlugin_Name="Tetrad"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(TetradPreview
        PRIVATE tetrad_engine juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # What the open window costs, in a real window (the workflow builds the same file against 0.1.0 too, to compare).
    juce_add_console_app(TetradWindowCost PRODUCT_NAME "TetradWindowCost")
    target_sources(TetradWindowCost PRIVATE preview/windowcost.cpp ${TETRAD_PLUGIN_SOURCES})
    target_include_directories(TetradWindowCost PRIVATE Source)
    target_compile_definitions(TetradWindowCost PRIVATE
        JucePlugin_Name="Tetrad"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(TetradWindowCost
        PRIVATE tetrad_engine juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    target_compile_definitions(TetradWindowCost PRIVATE TETRAD_WINDOWCOST_VARIANTS=1)

    # The same, with JUCE's other macOS drawing route (each changed area drawn on its own, through Metal), to compare.
    juce_add_console_app(TetradWindowCostMetal PRODUCT_NAME "TetradWindowCostMetal")
    target_sources(TetradWindowCostMetal PRIVATE preview/windowcost.cpp ${TETRAD_PLUGIN_SOURCES})
    target_include_directories(TetradWindowCostMetal PRIVATE Source)
    target_compile_definitions(TetradWindowCostMetal PRIVATE
        JucePlugin_Name="Tetrad"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        TETRAD_WINDOWCOST_VARIANTS=1
        JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1
    )
    target_link_libraries(TetradWindowCostMetal
        PRIVATE tetrad_engine juce::juce_audio_processors juce::juce_gui_basics
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
