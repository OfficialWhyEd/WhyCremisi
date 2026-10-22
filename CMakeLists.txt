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
elseif(EXISTS "E:/CARTELLE/JUCE")
    set(JUCE_ROOT "E:/CARTELLE/JUCE")  # Windows (Edo) — spostata qui dal Desktop
elseif(EXISTS "C:/Users/Whyed/Desktop/CARTELLE/JUCE")
    set(JUCE_ROOT "C:/Users/Whyed/Desktop/CARTELLE/JUCE")  # Windows (Edo), posizione precedente
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
# WebView2 (Windows) — required for WebBrowserComponent's resource provider
# ==============================================================================
# On Windows the WebView is backed by Microsoft Edge WebView2, which JUCE links
# from the Microsoft.Web.WebView2 NuGet package. Without it the whole
# Options::withResourceProvider API is compiled out and the React UI cannot load.
#
# Install with:
#   Register-PackageSource -provider NuGet -name nugetRepository -location https://www.nuget.org/api/v2
#   Install-Package Microsoft.Web.WebView2 -Scope CurrentUser -RequiredVersion 1.0.1901.177 -Source nugetRepository
# or unpack the .nupkg anywhere and point JUCE_WEBVIEW2_PACKAGE_LOCATION at it.

if(WIN32 AND NOT DEFINED JUCE_WEBVIEW2_PACKAGE_LOCATION)
    if(DEFINED ENV{JUCE_WEBVIEW2_PACKAGE_LOCATION})
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "$ENV{JUCE_WEBVIEW2_PACKAGE_LOCATION}")
    elseif(EXISTS "E:/CARTELLE/NuGet")
        set(JUCE_WEBVIEW2_PACKAGE_LOCATION "E:/CARTELLE/NuGet")  # Windows (Edo)
    endif()
endif()

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
    NEEDS_WEBVIEW2 TRUE
    PRODUCT_NAME "WhyCremisi"
    BUNDLE_ID "com.whycremisi.plugin"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
    ICON_BIG   "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/macos/icon_512x512@2x.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/macos/icon_32x32@2x.png"
)


# Disabilita VST2 fallback
target_compile_definitions(WhyCremisiVSTPlugin PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# Windows: attiva il backend WebView2 (linkato staticamente dal pacchetto NuGet).
# Senza questo JUCE_WEB_BROWSER_RESOURCE_PROVIDER_AVAILABLE resta 0 e la UI React
# non è raggiungibile dal plugin.
if(WIN32)
    target_compile_definitions(WhyCremisiVSTPlugin PRIVATE
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    )
endif()

# I sorgenti sono UTF-8 e contengono caratteri accentati e simboli (©, ™, —) nei
# commenti e nel watermark. Clang lo assume, MSVC no: senza /utf-8 interpreta i
# byte con la codepage locale e va in errore di sintassi. Va detto sia per il
# sorgente sia per l'esecuzione.
if(MSVC)
    target_compile_options(WhyCremisiVSTPlugin PRIVATE /utf-8)
endif()

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
        src/ai/ChainProvider.cpp
        src/ai/ToolRegistry.cpp
        src/samples/SampleLibrary.cpp
        src/samples/BeatBuilder.cpp
        src/daw/MixSnapshots.cpp
        src/daw/LiveDevices.cpp
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
        src/dsp/StereoStage.cpp
        src/dsp/Analyzer.cpp
        third_party/libebur128/ebur128.c
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
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_events
)

# ==============================================================================
# Include Directories
# ==============================================================================

# libebur128 usa M_PI: su MSVC serve _USE_MATH_DEFINES (file di terzi, non toccato).
set_source_files_properties(third_party/libebur128/ebur128.c PROPERTIES COMPILE_DEFINITIONS _USE_MATH_DEFINES)

target_include_directories(WhyCremisiVSTPlugin PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/osc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/midi
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libebur128/queue
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/src/daw
)

# ==============================================================================
# Unit Tests
# ==============================================================================
option(BUILD_UNIT_TESTS "Build C++ unit tests" OFF)
if(BUILD_UNIT_TESTS)
    # Un solo eseguibile che raccoglie tutti i juce::UnitTest e li esegue.
    #
    # Prima qui c'era juce_add_unit_test, una funzione che JUCE non ha mai
    # avuto: con BUILD_UNIT_TESTS=ON la configurazione falliva subito. Dato
    # che l'opzione e' spenta di default, nessuno se n'era accorto e i test
    # non erano mai stati eseguiti nemmeno una volta.
    juce_add_console_app(WhyCremisiTests
        PRODUCT_NAME "WhyCremisiTests"
    )

    target_sources(WhyCremisiTests
        PRIVATE
            src/tests/TestRunner.cpp
            src/core/tests/PluginProcessorTest.cpp
            src/bridge/tests/OscBridgeTest.cpp
            src/ai/tests/AiEngineTest.cpp
            src/ai/tests/ChainProviderTest.cpp
            src/samples/tests/SamplesTest.cpp
            src/daw/tests/MixSnapshotsTest.cpp
            src/daw/tests/LiveDevicesTest.cpp
            src/dsp/tests/AnalyzerLoudnessTest.cpp
            # I test istanziano il processore vero, quindi serve tutto cio'
            # da cui dipende: il plugin non e' una libreria separata.
            src/core/PluginProcessor.cpp
            src/core/PluginEditor.cpp
            src/core/SessionManager.cpp
            src/core/ParameterMapper.cpp
            src/core/PluginChain.cpp
            src/agent/AgentIdentity.cpp
            src/agent/AgentSoul.cpp
            src/agent/AgentUser.cpp
            src/agent/AgentWorkspace.cpp
            src/agent/PersonalityCore.cpp
            src/osc/OscHandler.cpp
            src/ai/AiEngine.cpp
            src/ai/AIProvider.cpp
            src/ai/ChainProvider.cpp
            src/ai/ToolRegistry.cpp
            src/samples/SampleLibrary.cpp
            src/samples/BeatBuilder.cpp
            src/daw/MixSnapshots.cpp
            src/daw/LiveDevices.cpp
            src/ui/WebViewBridge.cpp
            src/bridge/WebSocketServer.cpp
            src/bridge/OscBridge.cpp
            src/midi/MidiHandler.cpp
            src/dsp/EQBand.cpp
            src/dsp/Compressor.cpp
            src/dsp/Limiter.cpp
            src/dsp/StereoStage.cpp
            src/dsp/Analyzer.cpp
            third_party/libebur128/ebur128.c
            src/dsp/DSPEngine.cpp
            src/daw/ReaperDawHandler.cpp
            src/daw/AbletonDawHandler.cpp
            src/daw/DawDetector.cpp
    )

    target_include_directories(WhyCremisiTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/osc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ai
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/src/midi
        ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/libebur128/queue
        ${CMAKE_CURRENT_SOURCE_DIR}/src/bridge
        ${CMAKE_CURRENT_SOURCE_DIR}/src/daw
    )

    target_compile_definitions(WhyCremisiTests PRIVATE
        JUCE_UNIT_TESTS=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        # Il processore vuole i simboli JucePlugin_*: senza, non compila
        # fuori dal target del plugin.
        JucePlugin_Name="WhyCremisi"
        JucePlugin_IsSynth=0
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=1
        JucePlugin_IsMidiEffect=0
    )

    if(WIN32)
        target_compile_options(WhyCremisiTests PRIVATE /utf-8)
        target_compile_definitions(WhyCremisiTests PRIVATE JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
        # PluginEditor.cpp usa il resource provider della WebView, quindi
        # anche i test hanno bisogno degli header di WebView2. Il plugin li
        # riceve da NEEDS_WEBVIEW2; qui il collegamento va fatto a mano.
        if(TARGET juce::juce_webview2)
            target_link_libraries(WhyCremisiTests PRIVATE juce::juce_webview2)
        endif()
    endif()

    target_link_libraries(WhyCremisiTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_audio_devices
            juce::juce_audio_processors
            juce::juce_audio_formats
            juce::juce_audio_basics
            juce::juce_gui_basics
            juce::juce_gui_extra
            juce::juce_graphics
            juce::juce_core
            juce::juce_cryptography
            juce::juce_data_structures
            juce::juce_events
    )
endif()

# ==============================================================================
# Linux-specific (GTK, WebKit, install)
# ==============================================================================

# ==============================================================================
# React UI Bundle — copia dist/ nel Standalone .app (solo macOS Release)
# ==============================================================================

if(APPLE)
    set(REACT_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webview-ui/dist")
    set(ARTEFACTS_DIR  "${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/$<CONFIG>")
    set(STANDALONE_APP "${ARTEFACTS_DIR}/Standalone/WhyCremisi.app")
    set(VST3_BUNDLE    "${ARTEFACTS_DIR}/VST3/WhyCremisi.vst3")
    set(AU_BUNDLE      "${ARTEFACTS_DIR}/AU/WhyCremisi.component")
    set(ICNS_SRC       "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/WhyCremisi.icns")

    add_custom_command(TARGET WhyCremisiVSTPlugin POST_BUILD
        # React UI (Release only)
        COMMAND ${CMAKE_COMMAND} -E echo "Copying React bundle to Standalone..."
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${REACT_DIST_DIR}"
            "${STANDALONE_APP}/Contents/Resources/webview-ui"
        # App Transport Security — allow HTTP to localhost so WKWebView can load the React dev server
        COMMAND /usr/libexec/PlistBuddy -c
            "Add :NSAppTransportSecurity:NSAllowsLocalNetworking bool true"
            "${STANDALONE_APP}/Contents/Info.plist" || true
        COMMENT "Embedding React UI and ATS exceptions in app bundles"
    )

    # Icon copy runs on the Standalone target so it fires AFTER JUCE's own Icon.icns copy
    add_custom_command(TARGET WhyCremisiVSTPlugin_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ICNS_SRC}" "${STANDALONE_APP}/Contents/Resources/WhyCremisi.icns"
        COMMAND ${CMAKE_COMMAND} -E copy "${ICNS_SRC}" "${STANDALONE_APP}/Contents/Resources/Icon.icns"
        COMMENT "Overwriting JUCE Icon.icns with full 10-size WhyCremisi.icns"
    )

    # VST3 / AU icon copy (these don't have JUCE's own step overwriting them)
    add_custom_command(TARGET WhyCremisiVSTPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ICNS_SRC}" "${VST3_BUNDLE}/Contents/Resources/WhyCremisi.icns"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ICNS_SRC}" "${VST3_BUNDLE}/Contents/Resources/Icon.icns"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ICNS_SRC}" "${AU_BUNDLE}/Contents/Resources/WhyCremisi.icns"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ICNS_SRC}" "${AU_BUNDLE}/Contents/Resources/Icon.icns"
        COMMENT "Copying high-quality icns to VST3 and AU bundles"
    )
endif()

# ==============================================================================
# React UI Bundle — Windows
# ==============================================================================
# Il VST3 su Windows è comunque una cartella-bundle, quindi la UI va in
# Contents/Resources/webview-ui come su macOS. Lo Standalone è un exe nudo:
# lì la dist finisce accanto all'eseguibile. findUIDirectory() cerca entrambe.

if(WIN32)
    set(REACT_DIST_DIR "${CMAKE_CURRENT_SOURCE_DIR}/webview-ui/dist")
    set(WIN_ARTEFACTS  "${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/$<CONFIG>")

    if(EXISTS "${REACT_DIST_DIR}")
        add_custom_command(TARGET WhyCremisiVSTPlugin POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${REACT_DIST_DIR}"
                "${WIN_ARTEFACTS}/VST3/WhyCremisi.vst3/Contents/Resources/webview-ui"
            COMMENT "Embedding React UI in the Windows VST3 bundle"
        )

        if(TARGET WhyCremisiVSTPlugin_Standalone)
            add_custom_command(TARGET WhyCremisiVSTPlugin_Standalone POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${REACT_DIST_DIR}"
                    "${WIN_ARTEFACTS}/Standalone/webview-ui"
                COMMENT "Embedding React UI next to the Windows standalone"
            )
        endif()
    else()
        message(WARNING "webview-ui/dist non trovata — compila la UI con 'npm run build' in webview-ui/, altrimenti il plugin si aprirà vuoto.")
    endif()
endif()

# DAW thumbnail — copied into VST3 bundle for plugin browser display (macOS + Windows)
if(APPLE OR WIN32)
    add_custom_command(TARGET WhyCremisiVSTPlugin POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/$<CONFIG>/VST3/WhyCremisi.vst3/Contents/Resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/daw/daw-thumb-256.png"
            "${CMAKE_BINARY_DIR}/WhyCremisiVSTPlugin_artefacts/$<CONFIG>/VST3/WhyCremisi.vst3/Contents/Resources/plugin.png"
        COMMENT "Copying DAW thumbnail to VST3 bundle"
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

    # Install Linux icons for desktop integration
    set(LINUX_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Research/logo/platform/linux")
    install(FILES ${LINUX_ICON_DIR}/linux-16.png   DESTINATION ~/.local/share/icons/hicolor/16x16/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-22.png   DESTINATION ~/.local/share/icons/hicolor/22x22/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-24.png   DESTINATION ~/.local/share/icons/hicolor/24x24/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-32.png   DESTINATION ~/.local/share/icons/hicolor/32x32/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-48.png   DESTINATION ~/.local/share/icons/hicolor/48x48/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-64.png   DESTINATION ~/.local/share/icons/hicolor/64x64/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-96.png   DESTINATION ~/.local/share/icons/hicolor/96x96/apps   RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-128.png  DESTINATION ~/.local/share/icons/hicolor/128x128/apps RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-256.png  DESTINATION ~/.local/share/icons/hicolor/256x256/apps RENAME whycremisi.png)
    install(FILES ${LINUX_ICON_DIR}/linux-512.png  DESTINATION ~/.local/share/icons/hicolor/512x512/apps RENAME whycremisi.png)
endif()