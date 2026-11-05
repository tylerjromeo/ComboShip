# ComboShip Android: satisfied by CMake/android-deps.cmake (FetchContent), not a system lookup.
set(Opus_FOUND TRUE)
if(NOT TARGET Opus::opus)
    set(Opus_FOUND FALSE)
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus REQUIRED_VARS Opus_FOUND)
