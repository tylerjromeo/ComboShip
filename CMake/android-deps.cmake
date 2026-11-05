# ComboShip Android: third-party libraries the NDK does not ship.
#
# Included from the root CMakeLists after libultraship (whose cmake/dependencies/android.cmake
# fetches SDL2, nlohmann_json, tinyxml2, spdlog and libzip) and before ZAPD/soh/mm/combo. Every
# library is fetched once here and shared by both games; the stub Find modules in CMake/android/
# then let the vendored soh/mm/ZAPD CMakeLists keep their plain find_package() calls unchanged.
#
# Versions match linkzenic/Shipwright-Ports CMake/AndroidAudio.cmake (SoH 9.3.1-android.3).
include(FetchContent)

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/android")

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)

#=================== Audio codecs ===================
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libogg GIT_REPOSITORY https://github.com/xiph/ogg.git GIT_TAG v1.3.5)
FetchContent_MakeAvailable(libogg)
FetchContent_Declare(opus GIT_REPOSITORY https://github.com/xiph/opus.git GIT_TAG v1.5.2)
FetchContent_MakeAvailable(opus)
set(OGG_INCLUDE_DIR ${libogg_SOURCE_DIR}/include ${libogg_BINARY_DIR}/include)
set(OGG_LIBRARY ogg)
FetchContent_Declare(libvorbis GIT_REPOSITORY https://github.com/xiph/vorbis.git GIT_TAG v1.3.7)
FetchContent_MakeAvailable(libvorbis)
FetchContent_Declare(opusfile GIT_REPOSITORY https://github.com/xiph/opusfile.git GIT_TAG v0.12)
FetchContent_Populate(opusfile)
add_library(opusfile STATIC
    ${opusfile_SOURCE_DIR}/src/info.c
    ${opusfile_SOURCE_DIR}/src/internal.c
    ${opusfile_SOURCE_DIR}/src/opusfile.c
    ${opusfile_SOURCE_DIR}/src/stream.c)
target_include_directories(opusfile PUBLIC ${opusfile_SOURCE_DIR}/include)
target_link_libraries(opusfile PUBLIC opus ogg)

# The games include the packaged <opus/opus.h> and <opusfile.h> layouts.
set(COMBO_ANDROID_COMPAT_INCLUDE "${CMAKE_BINARY_DIR}/android_compat_include")
file(MAKE_DIRECTORY "${COMBO_ANDROID_COMPAT_INCLUDE}/opus" "${COMBO_ANDROID_COMPAT_INCLUDE}/SDL2")
file(GLOB _combo_opus_headers "${opus_SOURCE_DIR}/include/*.h")
file(COPY ${_combo_opus_headers} DESTINATION "${COMBO_ANDROID_COMPAT_INCLUDE}/opus")
file(COPY "${opusfile_SOURCE_DIR}/include/opusfile.h" DESTINATION "${COMBO_ANDROID_COMPAT_INCLUDE}/opus")

#=================== SDL2_net ===================
set(SDL2NET_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2NET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL2_net GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git GIT_TAG release-2.2.0)
FetchContent_MakeAvailable(SDL2_net)
file(COPY "${sdl2_net_SOURCE_DIR}/SDL_net.h" DESTINATION "${COMBO_ANDROID_COMPAT_INCLUDE}/SDL2")

#=================== libpng (ZAPD's in-process MM extractor) ===================
set(PNG_SHARED OFF CACHE BOOL "" FORCE)
set(PNG_STATIC ON CACHE BOOL "" FORCE)
set(PNG_TESTS OFF CACHE BOOL "" FORCE)
set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON)
FetchContent_Declare(libpng GIT_REPOSITORY https://github.com/pnggroup/libpng.git GIT_TAG v1.6.44)
FetchContent_MakeAvailable(libpng)
target_include_directories(png_static INTERFACE ${libpng_SOURCE_DIR} ${libpng_BINARY_DIR})

#=================== Namespaced names the vendored CMakeLists link ===================
function(combo_android_alias alias real)
    if(NOT TARGET ${alias})
        add_library(${alias} INTERFACE IMPORTED GLOBAL)
        target_link_libraries(${alias} INTERFACE ${real})
    endif()
endfunction()
combo_android_alias(Ogg::ogg ogg)
combo_android_alias(Opus::opus opus)
combo_android_alias(Vorbis::vorbis vorbis)
combo_android_alias(Vorbis::vorbisenc vorbisenc)
combo_android_alias(Vorbis::vorbisfile vorbisfile)
combo_android_alias(OpusFile::opusfile opusfile)
combo_android_alias(Opusfile::Opusfile opusfile)
combo_android_alias(SDL2_net::SDL2_net SDL2_net)
combo_android_alias(PNG::PNG png_static)
# On the wrappers, not opus itself: opus install(EXPORT)s, which rejects build-tree include dirs.
# (opus' own Opus::opus is an ALIAS and can't take it; both games also link opusfile.)
foreach(_combo_tgt OpusFile::opusfile Opusfile::Opusfile)
    target_include_directories(${_combo_tgt} INTERFACE "${COMBO_ANDROID_COMPAT_INCLUDE}")
endforeach()
# soh/mm add SDL2_INCLUDE_DIRS / SDL_NET_INCLUDE_DIRS to their include path directly.
get_target_property(_combo_sdl_inc SDL2::SDL2 INTERFACE_INCLUDE_DIRECTORIES)
set(SDL2_INCLUDE_DIRS ${_combo_sdl_inc} "${COMBO_ANDROID_COMPAT_INCLUDE}" CACHE INTERNAL "")
set(SDL_NET_INCLUDE_DIRS "${COMBO_ANDROID_COMPAT_INCLUDE}" "${sdl2_net_SOURCE_DIR}" CACHE INTERNAL "")
# Every later subdirectory (ZAPD, OTRExporter, soh, mm, combo) sees <SDL2/SDL_net.h> and
# <opus/opus.h> where the desktop builds find them in the system include path.
include_directories(SYSTEM "${COMBO_ANDROID_COMPAT_INCLUDE}")
