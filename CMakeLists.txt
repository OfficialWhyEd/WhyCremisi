cmake_minimum_required(VERSION 3.20)
project(OpenClaw-VST-Plugin VERSION 0.1.0 LANGUAGES C CXX)

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
# cURL - Required for AI HTTP requests
# ==============================================================================
# cURL is used for HTTP POST to Ollama, Gemini, OpenAI, etc.
# Linux: sudo apt install libcurl4-openssl-dev
# Windows: vcpkg install curl (or use prebuilt binaries)
# macOS: brew install curl (or use system framework)
find_package(CURL REQUIRED)
message(STATUS "CURL found: ${CURL_LIBRARIES}")

# ==============================================================================
# OpenSSL - Required for WebSocket SHA1 handshake (RFC 6455)
# ==============================================================================
# SHA1 implemented inline (no external OpenSSL dependency needed)
# This keeps the build portable across Linux, Windows, macOS

# ==============================================================================
# JUCE Plugin
# ==============================================================================

juce_add_plugin(OpenClawVSTPlugin
    COMPANY_NAME "OpenClaw"
    PLUGIN_MANUFACTURER_CODE OpCl
    PLUGIN_CODE OcAI
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    FORMATS VST3 Standalone
    PRODUCT_NAME "OpenClawVSTBridgeAI"
)

# Disabilita VST2 fallback
target_compile_definitions(OpenClawVSTPlugin PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ==============================================================================
# Source Files
# ==============================================================================

target_sources(OpenClawVSTPlugin
    PRIVATE
        src/core/PluginProcessor.cpp
        src/core/PluginEditor.cpp
        src/osc/OscHandler.cpp
        src/ai/AiEngine.cpp
        src/ui/WebViewBridge.cpp
        src/bridge/WebSocketServer.cpp
        src/bridge/OscBridge.cpp
)

# ==============================================================================
# Link Libraries
# ==============================================================================

target_link_libraries(OpenClawVSTPlugin
    PRIVATE
        # Audio modules (required for Standalone)
        juce::juce_audio_utils
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
        # HTTP
        ${CURL_LIBRARIES}
)

# ==============================================================================
# Include Directories
# ==============================================================================

target_include_directories(OpenClawVSTPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
    ${CURL_INCLUDE_DIRS}
)

# ==============================================================================
# Linux-specific (GTK, WebKit, install)
# ==============================================================================

if(NOT WIN32 AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT REQUIRED webkit2gtk-4.1)
    
    target_include_directories(OpenClawVSTPlugin PRIVATE ${GTK_INCLUDE_DIRS})
    target_include_directories(OpenClawVSTPlugin PRIVATE ${WEBKIT_INCLUDE_DIRS})
    
    target_link_libraries(OpenClawVSTPlugin PRIVATE ${GTK_LIBRARIES})
    target_link_libraries(OpenClawVSTPlugin PRIVATE ${WEBKIT_LIBRARIES})
    
    install(DIRECTORY ${CMAKE_BINARY_DIR}/OpenClawVSTPlugin_artefacts/Release/VST3/
            DESTINATION ~/.vst3
    )
endif()