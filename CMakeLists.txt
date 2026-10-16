cmake_minimum_required(VERSION 3.22)

project(MiDJ VERSION 0.5.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Las dependencias se descargan automáticamente la primera vez que se configura el proyecto
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)

# SoundTouch: time-stretch para el key lock (cambiar tempo sin cambiar el tono). Licencia LGPL.
# SOURCE_SUBDIR apunta a una carpeta inexistente para usar solo el código y compilarlo a nuestra manera.
FetchContent_Declare(
    soundtouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG        2.3.3
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  no-cmake
)

# SQLite: base de datos de la biblioteca (dominio público). Se usa el "amalgamation" (un solo .c)
FetchContent_Declare(
    sqlite
    URL      https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip
    URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(JUCE soundtouch sqlite)

add_library(sqlite3 STATIC ${sqlite_SOURCE_DIR}/sqlite3.c)
target_include_directories(sqlite3 PUBLIC ${sqlite_SOURCE_DIR})
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1 SQLITE_DEFAULT_WAL_SYNCHRONOUS=1 SQLITE_OMIT_LOAD_EXTENSION)
if (MSVC)
    target_compile_options(sqlite3 PRIVATE /W0)
endif()

file(GLOB SOUNDTOUCH_SOURCES ${soundtouch_SOURCE_DIR}/source/SoundTouch/*.cpp)
list(FILTER SOUNDTOUCH_SOURCES EXCLUDE REGEX "BPMDetect|PeakFinder")
add_library(soundtouch STATIC ${SOUNDTOUCH_SOURCES})
target_include_directories(soundtouch PUBLIC ${soundtouch_SOURCE_DIR}/include)
target_compile_definitions(soundtouch PUBLIC SOUNDTOUCH_FLOAT_SAMPLES=1)
if (MSVC)
    target_compile_options(soundtouch PRIVATE /W0)
endif()

juce_add_gui_app(MiDJ
    PRODUCT_NAME "MiDJ"
    COMPANY_NAME "moybm")

target_sources(MiDJ PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/DJLookAndFeel.cpp
    Source/TrackAnalyser.cpp
    Source/DeckEngine.cpp
    Source/DeckFx.cpp
    Source/MixerEngine.cpp
    Source/JogWheel.cpp
    Source/OverviewWaveform.cpp
    Source/ScrollingWaveforms.cpp
    Source/DeckComponent.cpp
    Source/MixerComponent.cpp
    Source/BrowserComponent.cpp
    Source/LibraryTree.cpp
    Source/TagReader.cpp
    Source/Library.cpp
    Source/SamplerEngine.cpp
    Source/Recorder.cpp
    Source/MidiController.cpp)

target_compile_definitions(MiDJ PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:MiDJ,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:MiDJ,JUCE_VERSION>")

if (MSVC)
    target_compile_options(MiDJ PRIVATE /utf-8)   # los textos con acentos del código son UTF-8
endif()

target_link_libraries(MiDJ
    PRIVATE
        soundtouch
        sqlite3
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
