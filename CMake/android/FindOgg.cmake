# ComboShip Android: satisfied by CMake/android-deps.cmake (FetchContent), not a system lookup.
set(Ogg_FOUND TRUE)
if(NOT TARGET Ogg::ogg)
    set(Ogg_FOUND FALSE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Ogg REQUIRED_VARS Ogg_FOUND)
