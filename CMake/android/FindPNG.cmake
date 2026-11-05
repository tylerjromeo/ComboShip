# ComboShip Android: satisfied by CMake/android-deps.cmake (FetchContent), not a system lookup.
set(PNG_FOUND TRUE)
if(NOT TARGET PNG::PNG)
    set(PNG_FOUND FALSE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PNG REQUIRED_VARS PNG_FOUND)
