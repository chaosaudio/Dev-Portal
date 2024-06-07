cmake_minimum_required(VERSION 3.13)
project(JuceEffect)
set(EFFECT_NAME "JuceEffect")
set(EFFECT_VERSION "0.0.1")
set(EFFECT_GUID "4ec868ff-1072-4822-9555-28a5d774b982")
set(EFFECT_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/effect")

# JUCE config - begin

set(JUCE_MODULES_ONLY ON)
set(JUCE_BUILD_EXTRAS OFF)
set(JUCE_BUILD_EXAMPLES OFF)
set(JUCE_ENABLE_MODULE_SOURCE_GROUPS OFF)
set(JUCE_COPY_PLUGIN_AFTER_BUILD OFF)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/JUCE JUCE)
add_library(${EFFECT_NAME} SHARED)

# Don't forget to edit <JUCE folder>/modules/CMakeLists.txt and JuceHeader.h to match those changes!
target_link_libraries(${EFFECT_NAME}
    PRIVATE
        juce::juce_audio_basics
        # juce::juce_audio_devices
        juce::juce_audio_formats
        # juce::juce_audio_plugin_client
        # juce::juce_audio_processors
        # juce::juce_audio_utils
        # juce::juce_box2d
        juce::juce_core
        # juce::juce_cryptography
        # juce::juce_data_structures
        juce::juce_dsp
        # juce::juce_events
        # juce::juce_graphics
        # juce::juce_gui_basics
        # juce::juce_gui_extra
        # juce::juce_opengl
        # juce::juce_osc
        # juce::juce_video

    PUBLIC
        # juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

# We're linking the modules privately, but we need to export
# their compile flags
target_compile_definitions(${EFFECT_NAME}
    PUBLIC
        JucePlugin_Build_Standalone=1
        JUCE_STANDALONE_APPLICATION=JucePlugin_Build_Standalone
        JUCE_USE_CURL=0
        JUCE_GUI_BASICS_INCLUDE_XHEADERS=0
        JUCE_USE_XRANDR=0
        JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0
        JUCE_JACK=0
        JUCE_USE_FREETYPE=0
        JUCE_WEB_BROWSER=0
        JUCE_INCLUDE_ZLIB_CODE=1
        JUCE_LOAD_CURL_SYMBOLS_LAZILY=1
    INTERFACE
        $<TARGET_PROPERTY:${EFFECT_NAME},COMPILE_DEFINITIONS>
    )

# JUCE config - end

# Stratus effect config - begin

target_include_directories(${EFFECT_NAME}
    PRIVATE
        src
    INTERFACE
        $<TARGET_PROPERTY:${EFFECT_NAME},INCLUDE_DIRECTORIES>
    )

# Your effect's cpp file(s)
target_sources(${EFFECT_NAME}
    PRIVATE
        src/juce_effect.cpp
    )

add_compile_definitions(${EFFECT_NAME}
    PUBLIC
        STRATUS_EFFECT_VERSION="${EFFECT_VERSION}"
        STRATUS_ENABLE_LOGGER=false
    )

# Target file
set_target_properties(${EFFECT_NAME}
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${EFFECT_OUTPUT_DIRECTORY}
        PREFIX ""
        OUTPUT_NAME ${EFFECT_GUID}
    )

# This file should go on Stratus along with the .so file
file(WRITE ${EFFECT_OUTPUT_DIRECTORY}/${EFFECT_GUID}.txt ${EFFECT_VERSION})

# Stratus effect config - end

# Uncomment if you're building on Stratus
# add_compile_options(-fPIC -shared -O2 -march=armv7-a -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math)
