# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

include(FetchContent)

FetchContent_Declare(
    ESPUI
    GIT_REPOSITORY https://github.com/s00500/ESPUI  
    # GIT_TAG        v3.0.0
    SOURCE_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/managed_components/s00500_ESPUI  
)

FetchContent_MakeAvailable(ESPUI)


project(Top-AMS)

