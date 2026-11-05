# ComboShip Android: satisfied by CMake/android-deps.cmake (FetchContent), not a system lookup.
set(SDL2_FOUND TRUE)
if(NOT TARGET SDL2::SDL2)
    set(SDL2_FOUND FALSE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2 REQUIRED_VARS SDL2_FOUND)
