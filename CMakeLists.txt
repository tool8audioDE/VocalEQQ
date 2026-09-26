cmake_minimum_required(VERSION 3.22)

# C wird mitaktiviert, weil JUCE C-Quellen mitbringt (u.a. die Bibliotheken
# fuer Schriftdarstellung und Bildformate). Ohne das bricht die Konfiguration
# des Plugin-Ziels ab.
project(VocalEQQ VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Die Quelltexte sind UTF-8 - Umlaute stehen in Kommentaren, Sonderzeichen
# in den Texten der Oberflaeche. MSVC liest sie sonst in der Codepage des
# Systems.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Wie bei Keyy und Voxx: Das Plugin braucht JUCE (grosser Download beim
# ersten Konfigurieren), DSP-Kern, Tests und Offline-Werkzeug kommen ohne aus.
option(VOCALEQQ_BUILD_PLUGIN "VST3- und Standalone-Ziele bauen (benoetigt JUCE)" ON)
option(VOCALEQQ_BUILD_TESTS  "Unit-Tests bauen (benoetigt Catch2)"               ON)
option(VOCALEQQ_BUILD_CLI    "Offline-Kommandozeilenwerkzeug bauen"              ON)

# Nach dem Bauen wird das VST3 in den System-Ordner kopiert. Das braucht
# unter Windows Adminrechte; ohne sie scheitert der Bau an dieser Stelle,
# obwohl das Plugin laengst fertig ist.
option(VOCALEQQ_COPY_PLUGIN  "Gebautes Plugin in den System-Ordner kopieren"     OFF)

include(FetchContent)

# ---------------------------------------------------------------------------
# DSP-Kern: reines C++17, keine Abhaengigkeiten.
# Genau deshalb ist er in Tests, im CLI und im Plugin derselbe Code.
# ---------------------------------------------------------------------------
add_library(vocaleqq_dsp STATIC
    source/dsp/AutoCompressor.cpp
    source/dsp/DeEsser.cpp
    source/dsp/Fft.cpp
    source/dsp/ResonanceEq.cpp
    source/dsp/VocalChain.cpp)

target_include_directories(vocaleqq_dsp PUBLIC source)

# Der DSP-Kern landet auch in der VST3-Shared-Library.
set_target_properties(vocaleqq_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_options(vocaleqq_dsp PRIVATE /W4)
else()
    target_compile_options(vocaleqq_dsp PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# Offline-Werkzeug
# ---------------------------------------------------------------------------
if(VOCALEQQ_BUILD_CLI)
    add_executable(vocaleqq-cli tools/VocalEqqCli.cpp)
    target_link_libraries(vocaleqq-cli PRIVATE vocaleqq_dsp)
    target_include_directories(vocaleqq-cli PRIVATE tools)

    if(MSVC)
        target_compile_definitions(vocaleqq-cli PRIVATE _CRT_SECURE_NO_WARNINGS)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(VOCALEQQ_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(vocaleqq-tests
        tests/ChainTests.cpp
        tests/DynamicsTests.cpp
        tests/EqTests.cpp)

    target_link_libraries(vocaleqq-tests PRIVATE vocaleqq_dsp Catch2::Catch2WithMain)
    target_include_directories(vocaleqq-tests PRIVATE tests)

    enable_testing()
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(vocaleqq-tests)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(VOCALEQQ_BUILD_PLUGIN)
    # Feste Version, dieselbe wie in Keyy und Voxx.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    # AU fehlt bewusst, wie bei Keyy: nur macOS, nie gebaut, nie geprueft.
    juce_add_plugin(VocalEQQ
        COMPANY_NAME              "tooL8"
        COMPANY_WEBSITE           "https://tool8.online"
        PLUGIN_MANUFACTURER_CODE  TooL
        PLUGIN_CODE               Veqq
        FORMATS                   VST3 Standalone
        PRODUCT_NAME              "Vocal EQQ"
        VST3_CATEGORIES           Fx EQ Dynamics
        IS_SYNTH                  FALSE
        NEEDS_MIDI_INPUT          FALSE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD   ${VOCALEQQ_COPY_PLUGIN})

    target_sources(VocalEQQ PRIVATE
        source/plugin/PluginEditor.cpp
        source/plugin/PluginProcessor.cpp
        source/plugin/SpectrumView.cpp
        source/plugin/Tool8LookAndFeel.cpp)

    target_compile_definitions(VocalEQQ PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(VocalEQQ PRIVATE
        vocaleqq_dsp
        juce::juce_audio_utils
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

    # Rendert die Oberflaeche als PNG (Screenshots fuer Website und README,
    # Layout pruefen ohne DAW). Nutzt denselben Code wie das Plugin.
    option(VOCALEQQ_BUILD_SNAPSHOT "Werkzeug fuer Oberflaechen-Screenshots bauen" OFF)
    if(VOCALEQQ_BUILD_SNAPSHOT)
        juce_add_console_app(vocaleqq-snapshot PRODUCT_NAME "vocaleqq-snapshot")
        target_sources(vocaleqq-snapshot PRIVATE
            tools/Snapshot.cpp
            source/plugin/PluginEditor.cpp
            source/plugin/PluginProcessor.cpp
            source/plugin/SpectrumView.cpp
            source/plugin/Tool8LookAndFeel.cpp)
        target_compile_definitions(vocaleqq-snapshot PRIVATE
            JucePlugin_Name="Vocal EQQ"
            JucePlugin_VersionString="${PROJECT_VERSION}"
            JUCE_MODAL_LOOPS_PERMITTED=1
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)
        target_link_libraries(vocaleqq-snapshot PRIVATE
            vocaleqq_dsp
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
    endif()
endif()
