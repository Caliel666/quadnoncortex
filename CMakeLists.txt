cmake_minimum_required(VERSION 3.22)

project(quadnoncortex VERSION 1.0.2)

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
    VERSION      "1.0.2"
    NEEDS_CURL   FALSE
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
)

target_compile_definitions(quadnoncortex
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
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
)

target_link_libraries(quadnoncortex
    PRIVATE
        juce::juce_audio_utils
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
