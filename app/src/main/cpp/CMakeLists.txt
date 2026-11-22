# GENRA BOX for Android: the native part (the engine, the DRUM-X core, the bridge and the audio output).
cmake_minimum_required(VERSION 3.18.1)
project(genrabox C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The DSP must be optimized in every build type (a -O0 debug build cannot play in real time).
set(OPT -O2)

# ---- DRUM-X: the JIANT FM firmware's DRUM engine as its browser build compiles it (plain C, its state in statics)
add_library(drumx STATIC drumx/jiantcore/drumcore.c)
set_target_properties(drumx PROPERTIES C_STANDARD 11 C_EXTENSIONS ON)
target_include_directories(drumx PRIVATE drumx/jiant/build/gen drumx/jiant/firmware/src)
target_compile_definitions(drumx PRIVATE "FELUCCA_VERSION=\"ANDROID\"")
target_compile_options(drumx PRIVATE ${OPT} -w -ffp-contract=off -fvisibility=hidden)

# ---- the engine and the bridge
add_library(genrabox SHARED
	box/jni.cpp
	box/BoxEngine.cpp
	genra-src/genra/Engine.cpp
	genra-src/genra/Genres.cpp
	genra-src/genra/Arranger.cpp
	genra-src/genra/Edit.cpp
	genra-src/genra/Midi.cpp
	genra-src/genra/Store.cpp
)
target_compile_options(genrabox PRIVATE ${OPT} -fexceptions -frtti -Wall -Wno-unused-function -Wno-unused-variable
	-Wno-unused-but-set-variable -Wno-multichar)
target_link_libraries(genrabox drumx aaudio log m)

# ---- Ableton Link (GPLv2+, github.com/Ableton/link): tempo, beat and start / stop with other apps on the Wi-Fi.
# Fetched when the build is configured (with its asio submodule); the Linux platform code runs on Android.
include(FetchContent)
FetchContent_Declare(ableton_link
	GIT_REPOSITORY https://github.com/Ableton/link.git
	GIT_TAG Link-3.1.5
	GIT_SHALLOW TRUE
	GIT_SUBMODULES modules/asio-standalone
)
FetchContent_GetProperties(ableton_link)
if(NOT ableton_link_POPULATED)
	FetchContent_Populate(ableton_link)
endif()
target_include_directories(genrabox PRIVATE
	${ableton_link_SOURCE_DIR}/include
	${ableton_link_SOURCE_DIR}/modules/asio-standalone/asio/include
)
target_compile_definitions(genrabox PRIVATE GENRA_LINK=1 LINK_PLATFORM_UNIX=1 LINK_PLATFORM_LINUX=1)
