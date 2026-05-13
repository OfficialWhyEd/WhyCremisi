cmake_minimum_required(VERSION 3.20)
project(WhyCremisi-VST-Plugin VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ==============================================================================
# JUCE Configuration
# ==============================================================================

# Try environment variable first, then common paths
if(DEFINED ENV{JUCE_ROOT})
    set(JUCE_ROOT "$ENV{JUCE_ROOT}")
elseif(EXISTS "/home/carlo/SDKs/JUCE")
    set(JUCE_ROOT "/home/carlo/SDKs/JUCE")  # Linux (Carlo)
elseif(EXISTS "C:/Users/Whyed/Desktop/CARTELLE/JUCE")
    set(JUCE_ROOT "C:/Users/Whyed/Desktop/CARTELLE/JUCE")  # Windows (Edo)
elseif(EXISTS "/Users/whyed/Documents/Dev/SDKs/JUCE")
    set(JUCE_ROOT "/Users/whyed/Documents/Dev/SDKs/JUCE")  # macOS (whyed)
elseif(EXISTS "/Users/edo/SDKs/JUCE")
    set(JUCE_ROOT "/Users/edo/SDKs/JUCE")  # macOS (edo)
else()
    message(FATAL_ERROR "JUCE not found. Set JUCE_ROOT environment variable or install JUCE.")
endif()

message(STATUS "JUCE_ROOT: ${JUCE_ROOT}")
add_subdirectory(${JUCE_ROOT} JUCE)

# ==============================================================================
# cURL - Not required (AI uses JUCE native HTTP)
# ==============================================================================

# ==============================================================================
# OpenSSL - Required for WebSocket SHA1 handshake (RFC 6455)
# ==============================================================================
# SHA1 implemented inline (no external OpenSSL dependency needed)
# This keeps the build portable across Linux, Windows, macOS

# ==============================================================================
# JUCE Plugin
# ==============================================================================

juce_add_plugin(WhyCremisiVSTPlugin
    COMPANY_NAME "WhyCremisi"
    PLUGIN_MANUFACTURER_CODE WyCr
    PLUGIN_CODE WyCr
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    # VST3: Ableton, REAPER, FL Studio, Cubase, Studio One, Bitwig, Pro Tools (via wrapper)
    # AU:   Logic Pro, GarageBand (macOS only — ignored on Windows/Linux)
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "WhyCremisi"
    BUNDLE_ID "com.whycremisi.plugin"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/macos/icon_512x512.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/macos/icon_32x32.png"
)

# Disabilita VST2 fallback
target_compile_definitions(WhyCremisiVSTPlugin PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ==============================================================================
# Source Files
# ==============================================================================

target_sources(WhyCremisiVSTPlugin
    PRIVATE
        src/core/PluginProcessor.cpp
        src/core/PluginEditor.cpp
        src/core/SessionManager.cpp
        src/agent/AgentIdentity.cpp
        src/agent/AgentSoul.cpp
        src/agent/AgentUser.cpp
        src/agent/AgentWorkspace.cpp
        src/osc/OscHandler.cpp
        src/ai/AiEngine.cpp
        src/ai/AIProvider.cpp
        src/ai/ToolRegistry.cpp
        src/ui/WebViewBridge.cpp
        src/bridge/WebSocketServer.cpp
        src/bridge/OscBridge.cpp
        src/midi/MidiHandler.cpp
        src/core/ParameterMapper.cpp
        src/core/PluginChain.cpp
        src/agent/PersonalityCore.cpp
        src/dsp/EQBand.cpp
        src/dsp/Compressor.cpp
        src/dsp/Limiter.cpp
        src/dsp/Analyzer.cpp
        src/dsp/DSPEngine.cpp
        src/daw/ReaperDawHandler.cpp
        src/daw/AbletonDawHandler.cpp
        src/daw/DawDetector.cpp
)

# ==============================================================================
# Link Libraries
# ==============================================================================

target_link_libraries(WhyCremisiVSTPlugin
    PRIVATE
        # Audio modules (required for Standalone)
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_audio_basics
        # GUI - ordine importante: basics prima di extra
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_graphics
        # Core
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
)

# ==============================================================================
# Include Directories
# ==============================================================================

target_include_directories(WhyCremisiVSTPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/src/daw
)

# ==============================================================================
# Unit Tests
# ==============================================================================
option(BUILD_UNIT_TESTS "Build C++ unit tests" OFF)
if(BUILD_UNIT_TESTS)
    # Unit test for PluginProcessor
    juce_add_unit_test(PluginProcessorTest
        SOURCES
            src/core/tests/PluginProcessorTest.cpp
    )
    
    # Unit test for OscBridge
    juce_add_unit_test(OscBridgeTest
        SOURCES
            src/bridge/tests/OscBridgeTest.cpp
    )
    
    # Link the unit tests with the main plugin target so they can access its classes
    target_link_libraries(PluginProcessorTest PRIVATE WhyCremisiVSTPlugin)
    target_link_libraries(OscBridgeTest PRIVATE WhyCremisiVSTPlugin)
endif()

# ==============================================================================
# Linux-specific (GTK, WebKit, install)
# ==============================================================================

# ==============================================================================
# React UI Bundle — copia dist/ nel Standalone .app (solo macOS Release)
# ==============================================================================

if(APPLE)
    set(REACT_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webview-ui/dist")
    set(STANDALONE_RESOURCES "${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/$<CONFIG>/Standalone/WhyCremisi.app/Contents/Resources/webview-ui")

    add_custom_command(TARGET WhyCremisiVSTPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying React bundle to Standalone..."
        COMMAND ${CMAKE_COMMAND} -E $<IF:$<CONFIG:Release>,copy_directory,true>
            "${REACT_DIST_DIR}"
            "${STANDALONE_RESOURCES}"
        COMMENT "Embedding React UI in Standalone app bundle (Release only)"
    )
endif()

if(NOT WIN32 AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    
    target_include_directories(WhyCremisiVSTPlugin PRIVATE ${GTK_INCLUDE_DIRS})
    target_include_directories(WhyCremisiVSTPlugin PRIVATE ${WEBKIT_INCLUDE_DIRS})
    
    target_link_libraries(WhyCremisiVSTPlugin PRIVATE ${GTK_LIBRARIES})
    target_link_libraries(WhyCremisiVSTPlugin PRIVATE ${WEBKIT_LIBRARIES})
    
    install(DIRECTORY ${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/Release/VST3/
            DESTINATION ~/.vst3
    )
endif()