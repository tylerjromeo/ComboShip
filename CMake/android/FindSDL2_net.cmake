# ComboShip Android: satisfied by CMake/android-deps.cmake (FetchContent), not a system lookup.
set(SDL2_net_FOUND TRUE)
if(NOT TARGET SDL2_net::SDL2_net)
    set(SDL2_net_FOUND FALSE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SDL2_net REQUIRED_VARS SDL2_net_FOUND)
