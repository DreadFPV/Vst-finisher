cmake_minimum_required(VERSION 3.22)

project(Finisher VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(FINISHER_BUILD_CLI "Offline-CLI (Messen/Mastern von WAV-Dateien) bauen" ON)

# --- JUCE --------------------------------------------------------------------
# Lokal schneller: -DFETCHCONTENT_SOURCE_DIR_JUCE=/pfad/zu/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(FINISHER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FINISHER_FORMATS AU)
endif()

juce_add_plugin(Finisher
    COMPANY_NAME                "JustCreateit"
    COMPANY_WEBSITE             "https://justcreateit.ch"
    BUNDLE_ID                   "ch.justcreateit.finisher"
    PLUGIN_MANUFACTURER_CODE    Jcit
    PLUGIN_CODE                 Fnsh
    FORMATS                     ${FINISHER_FORMATS}
    PRODUCT_NAME                "Finisher"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Mastering Dynamics EQ
    AU_MAIN_TYPE                kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Finisher PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/gui/Components.cpp)

target_include_directories(Finisher PRIVATE Source)

target_compile_definitions(Finisher PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Finisher
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# --- CLI ----------------------------------------------------------------------
if(FINISHER_BUILD_CLI)
    add_executable(finisher-cli tools/finisher_cli.cpp)
    target_include_directories(finisher-cli PRIVATE Source)

    # DSP-Selbsttest (Null-Tests, Latenz, True Peak, Dyn-EQ ...) -> ctest -C Release
    enable_testing()
    add_executable(finisher-dsptest tools/dsp_test.cpp)
    target_include_directories(finisher-dsptest PRIVATE Source)
    add_test(NAME dsp COMMAND finisher-dsptest)
endif()

# --- Host-Test (lädt das gebaute VST3, prüft Verarbeitung/State, rendert GUI) ---
option(FINISHER_BUILD_HOSTTEST "Host-Test bauen" OFF)
if(FINISHER_BUILD_HOSTTEST)
    juce_add_console_app(finisher-hosttest PRODUCT_NAME "finisher-hosttest")
    target_sources(finisher-hosttest PRIVATE tools/host_test.cpp)
    target_compile_definitions(finisher-hosttest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(finisher-hosttest PRIVATE
        juce::juce_audio_processors juce::juce_audio_formats juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()

if(FINISHER_BUILD_HOSTTEST)
    juce_add_console_app(finisher-snapshot PRODUCT_NAME "finisher-snapshot")
    target_sources(finisher-snapshot PRIVATE
        tools/snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/gui/Components.cpp)
    target_include_directories(finisher-snapshot PRIVATE Source)
    target_compile_definitions(finisher-snapshot PRIVATE
        JucePlugin_Name="Finisher" JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(finisher-snapshot PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

if(FINISHER_BUILD_HOSTTEST)
    juce_add_console_app(finisher-presettest PRODUCT_NAME "finisher-presettest")
    target_sources(finisher-presettest PRIVATE
        tools/preset_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/gui/Components.cpp)
    target_include_directories(finisher-presettest PRIVATE Source)
    target_compile_definitions(finisher-presettest PRIVATE
        JucePlugin_Name="Finisher" JucePlugin_VersionString="${PROJECT_VERSION}"
        JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(finisher-presettest PRIVATE
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
