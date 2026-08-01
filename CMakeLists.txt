cmake_minimum_required(VERSION 3.22)

project(quadnoncortex VERSION 1.1.0)

# Multi-config (VS): add Dev = RelWithDebInfo + QUADNONCORTEX_DEV
if (CMAKE_CONFIGURATION_TYPES)
    set (CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Dev" CACHE STRING "" FORCE)
endif()
set (CMAKE_CXX_FLAGS_DEV             "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
set (CMAKE_C_FLAGS_DEV               "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
set (CMAKE_EXE_LINKER_FLAGS_DEV      "${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO}")
set (CMAKE_SHARED_LINKER_FLAGS_DEV   "${CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO}")

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_PATH}. Set -DJUCE_PATH=/path/to/JUCE")
endif()

add_subdirectory(${JUCE_PATH} JUCE)

juce_add_gui_app(quadnoncortex
    PRODUCT_NAME "quadnoncortex"
    COMPANY_NAME "quadnoncortex"
    BUNDLE_ID    "com.quadnoncortex.app"
    VERSION      "1.1.0"
    NEEDS_CURL   TRUE
    NEEDS_WEB_BROWSER FALSE
)

juce_generate_juce_header(quadnoncortex)

target_sources(quadnoncortex
    PRIVATE
        Source/Main.cpp
        Source/MainComponent.h
        Source/MainComponent.cpp
        Source/AudioEngine.h
        Source/AudioEngine.cpp
        Source/PluginChain.h
        Source/PluginChain.cpp
        Source/PluginBlockComponent.h
        Source/PluginBlockComponent.cpp
        Source/ParameterPanel.h
        Source/ParameterPanel.cpp
        Source/MidiLearnManager.h
        Source/MidiLearnManager.cpp
        Source/PluginBrowser.h
        Source/PluginBrowser.cpp
        Source/PluginScanOverlay.h
        Source/PluginScanOverlay.cpp
        Source/TunerComponent.h
        Source/TunerComponent.cpp
        Source/AppSettings.h
        Source/Theme.h
        Source/OnScreenKeyboard.h
        Source/SettingsComponent.h
        Source/SettingsComponent.cpp
        Source/NativeNam/NativeNamProcessor.cpp
        Source/NativeNam/NativeNamPanel.cpp
        Source/NativeNam/Tone3000Client.cpp
)

target_compile_definitions(quadnoncortex
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:quadnoncortex,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:quadnoncortex,JUCE_VERSION>"
        QUADNONCORTEX_VERSION="$<TARGET_PROPERTY:quadnoncortex,JUCE_VERSION>"
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_VST=0
        JUCE_PLUGINHOST_AU=0
        JUCE_PLUGINHOST_LADSPA=0
        JUCE_PLUGINHOST_LV2=0
        JUCE_ASIO=1
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=0
        JUCE_ALSA=1
        JUCE_JACK=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        $<$<CONFIG:Dev>:QUADNONCORTEX_DEV=1>
        $<$<CONFIG:Debug>:QUADNONCORTEX_DEV=1>
)


# ---- Native NAM (NeuralAmpModelerCore) ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(QUADNONCORTEX_FETCH_NAM "Fetch NeuralAmpModelerCore via FetchContent" ON)
set(QUADNONCORTEX_HAS_NAM 0)

if (QUADNONCORTEX_FETCH_NAM)
    include(FetchContent)
    FetchContent_Declare(
        NeuralAmpModelerCore
        GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES "Dependencies/eigen;Dependencies/nlohmann"
    )
    # Populate only (do not add_subdirectory NAM's own CMake project)
    set(CMAKE_POLICY_DEFAULT_CMP0169 OLD)
    FetchContent_GetProperties(NeuralAmpModelerCore)
    if (NOT neuralampmodelercore_POPULATED)
        FetchContent_Populate(NeuralAmpModelerCore)
    endif()
    set(NAM_ROOT "${neuralampmodelercore_SOURCE_DIR}")
    if (EXISTS "${NAM_ROOT}/NAM/get_dsp.h")
        set(QUADNONCORTEX_HAS_NAM 1)
        file(GLOB NAM_SOURCES
            "${NAM_ROOT}/NAM/*.cpp"
            "${NAM_ROOT}/NAM/wavenet/*.cpp"
        )
        # Exclude tools if any leaked into GLOB
        target_sources(quadnoncortex PRIVATE ${NAM_SOURCES})
        target_include_directories(quadnoncortex PRIVATE
            "${NAM_ROOT}/NAM"
            "${NAM_ROOT}/Dependencies/eigen"
            "${NAM_ROOT}/Dependencies/nlohmann"
            "${NAM_ROOT}/Dependencies/nlohmann/single_include"
            "${NAM_ROOT}/Dependencies/nlohmann/single_include/nlohmann"
        )
        target_compile_definitions(quadnoncortex PRIVATE
            NAM_ENABLE_A2_FAST
            EIGEN_MAX_ALIGN_BYTES=0
        )
        message(STATUS "NeuralAmpModelerCore found — Native NAM DSP enabled")
    else()
        message(WARNING "NeuralAmpModelerCore missing — Native NAM UI only (no DSP)")
    endif()
endif()

target_compile_definitions(quadnoncortex PRIVATE QUADNONCORTEX_HAS_NAM=${QUADNONCORTEX_HAS_NAM})
target_include_directories(quadnoncortex PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/Source"
)

target_link_libraries(quadnoncortex
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_processors
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_graphics
        juce::juce_events
        juce::juce_core
        juce::juce_data_structures
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
