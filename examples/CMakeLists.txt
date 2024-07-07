cmake_minimum_required(VERSION 3.7.2)
project(examples)

set(CMAKE_CXX_STANDARD 14)

# set optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -fPIC -shared -O3 -mcpu=cortex-a8 -mtune=cortex-a8 -mfloat-abi=hard -mfpu=neon -ftree-vectorize -ffast-math -fprefetch-loop-arrays -funroll-loops -funsafe-loop-optimizations -fno-finite-math-only")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,-Ofast -Wl,--as-needed -Wl,--strip-debug")
message("CMAKE_CXX_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_CXX_FLAGS_RELEASE}")
message("CMAKE_SHARED_LINKER_FLAGS_RELEASE in ${CMAKE_CURRENT_SOURCE_DIR} = ${CMAKE_SHARED_LINKER_FLAGS_RELEASE}")

include_directories(../resources)

include_directories(../common)

# Get all .cpp files in the current directory
file(GLOB EFFECTS_SRC
    "*.cpp"
)

# Compile Biquad.cpp into an object file
add_library(Biquad OBJECT ../../common/Biquad.cpp)

# Get all directories in the current directory
file(GLOB EFFECTS_DIRS LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")

foreach(EFFECT_DIR ${EFFECTS_DIRS})
    # Skip if not a directory
    if(NOT IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${EFFECT_DIR})
        continue()
    endif()

    # @TODO: enable when ready
    # Skip if the directory is 'juce_effect'
    if(EFFECT_DIR STREQUAL "juce_effect")
        continue()
    endif()

    # Get the filename without extension
    get_filename_component(EFFECT_NAME ${EFFECT_DIR} NAME_WE)

    # If the current file is Equalizer.cpp, link it with Biquad
    if(EFFECT_NAME STREQUAL "equalizer")
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp $<TARGET_OBJECTS:Biquad>)
    else()
        add_library(${EFFECT_NAME} SHARED ${EFFECT_DIR}/${EFFECT_NAME}.cpp)
    endif()

    # Remove the 'lib' prefix from the output library name
    set_target_properties(${EFFECT_NAME} PROPERTIES PREFIX "")

    # Install the library to the 'bins' directory
    install(TARGETS ${EFFECT_NAME} LIBRARY DESTINATION bins)
endforeach(EFFECT_DIR)
