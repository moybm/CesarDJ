cmake_minimum_required(VERSION 3.22)

project(CesarDJ VERSION 0.12.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime de Visual C++ dentro del .exe: CesarDJ.exe funciona en otro PC sin instalar nada
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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

# demucs.cpp: separación de stems con IA (Demucs v4 de Meta portado a C++, licencia MIT).
# Solo se usa su código de inferencia (src/) y Eigen; se fija el commit exacto.
FetchContent_Declare(
    demucs
    GIT_REPOSITORY https://github.com/sevagh/demucs.cpp.git
    GIT_TAG        f1206e9adeea103aef4a636b9e62297cf1f8e34e
    GIT_SUBMODULES "vendor/eigen"
    SOURCE_SUBDIR  no-cmake
)

FetchContent_MakeAvailable(JUCE soundtouch sqlite demucs)

file(GLOB DEMUCS_SOURCES ${demucs_SOURCE_DIR}/src/*.cpp)
add_library(demucs STATIC ${DEMUCS_SOURCES})
target_include_directories(demucs PUBLIC ${demucs_SOURCE_DIR}/src ${demucs_SOURCE_DIR}/vendor/eigen)
target_compile_definitions(demucs PUBLIC _USE_MATH_DEFINES NOMINMAX)
if (MSVC)
    # Optimizado también al depurar: sin esto la IA sería decenas de veces más lenta
    target_compile_options(demucs PRIVATE /W0 /O2 /Ob2 /bigobj /fp:fast)
    target_compile_options(demucs PUBLIC /bigobj)
endif()

# Modelo de IA (htdemucs 4 fuentes, ~84 MB): se descarga una vez y se verifica su huella
set(DEMUCS_MODEL "${CMAKE_BINARY_DIR}/models/ggml-model-htdemucs-4s-f16.bin")
if (NOT EXISTS "${DEMUCS_MODEL}")
    message(STATUS "Descargando el modelo de stems (84 MB)...")
    file(DOWNLOAD
        "https://huggingface.co/datasets/Retrobear/demucs.cpp/resolve/main/ggml-model-htdemucs-4s-f16.bin"
        "${DEMUCS_MODEL}"
        EXPECTED_HASH SHA256=72b17c42d308982ddb5069bc3bf48b81a5aac4cb6516e4366c0fa7cef6df0064
        SHOW_PROGRESS)
endif()

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

# WebView2 (Microsoft Edge): ventana de inicio de sesión de Spotify dentro de CesarDJ.
# Se descarga el SDK de NuGet (verificado) y se enlaza estático: no añade DLLs al programa.
set(WEBVIEW2_VERSION 1.0.3967.48)
set(WEBVIEW2_DIR "${CMAKE_BINARY_DIR}/webview2/Microsoft.Web.WebView2.${WEBVIEW2_VERSION}")
if (NOT EXISTS "${WEBVIEW2_DIR}/build/native/include/WebView2.h")
    message(STATUS "Descargando el SDK de WebView2...")
    file(DOWNLOAD
        "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${WEBVIEW2_VERSION}/microsoft.web.webview2.${WEBVIEW2_VERSION}.nupkg"
        "${CMAKE_BINARY_DIR}/webview2.nupkg"
        EXPECTED_HASH SHA256=c66357ac7f324ec9bcafe5241706a023b4122d8c22300c31de4b0eb220db689e)
    file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/webview2.nupkg" DESTINATION "${WEBVIEW2_DIR}")
endif()
set(JUCE_WEBVIEW2_PACKAGE_LOCATION "${CMAKE_BINARY_DIR}/webview2")

juce_add_gui_app(CesarDJ
    PRODUCT_NAME "CesarDJ"
    COMPANY_NAME "CesarDJ"
    DESCRIPTION  "CesarDJ - software DJ"
    ICON_BIG     Resources/vespa.svg
    ICON_SMALL   Resources/vespa.svg
    NEEDS_WEBVIEW2 TRUE)

# Si cambia el logo, regenerar el icono del .exe (JUCE no lo detecta por sí solo)
set(CESARDJ_ICON "${CMAKE_BINARY_DIR}/CesarDJ_artefacts/JuceLibraryCode/icon.ico")
if (EXISTS "${CESARDJ_ICON}" AND "${CMAKE_SOURCE_DIR}/Resources/vespa.svg" IS_NEWER_THAN "${CESARDJ_ICON}")
    file(REMOVE "${CESARDJ_ICON}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS Resources/vespa.svg)

# Logo incrustado en el programa (bienvenida, barra superior, platos)
juce_add_binary_data(CesarDJData SOURCES
    Resources/vespa.svg
    Resources/vespa_mark.svg)

target_sources(CesarDJ PRIVATE
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
    Source/MidiController.cpp
    Source/Branding.cpp
    Source/Automix.cpp
    Source/StemSeparator.cpp
    Source/PlaylistImport.cpp
        Source/SettingsWindow.cpp
        Source/Help.cpp
        Source/HelpContent.cpp)

target_compile_definitions(CesarDJ PRIVATE
    JUCE_WEB_BROWSER=1
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:CesarDJ,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:CesarDJ,JUCE_VERSION>")

if (MSVC)
    target_compile_options(CesarDJ PRIVATE /utf-8)   # los textos con acentos del código son UTF-8
endif()

target_link_libraries(CesarDJ
    PRIVATE
        CesarDJData
        soundtouch
        sqlite3
        demucs
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_gui_extra
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# El modelo de stems va junto al .exe (carpeta models\)
add_custom_command(TARGET CesarDJ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:CesarDJ>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${DEMUCS_MODEL}" "$<TARGET_FILE_DIR:CesarDJ>/models/"
    VERBATIM)
