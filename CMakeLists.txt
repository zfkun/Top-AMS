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

# idf_component_register(
#     SRCS 
#         ${CMAKE_CURRENT_SOURCE_DIR}/managed_components/s00500_ESPUI/*.cpp          # ESPUI 主源文件
#         # "${espui_SOURCE_DIR}/utility/...其他源文件"  # 补充其他文件路径
#     INCLUDE_DIRS 
#         $${CMAKE_CURRENT_SOURCE_DIR}/managed_components/s00500_ESPUI/*.h                    # 主头文件目录
#         # "${espui_SOURCE_DIR}/utility"            # 附加头文件目录
#     # REQUIRES 
#     #     arduino                                  # 依赖的 ESP-IDF 组件
#     #     webserver
# )


project(Top-AMS)

# list(APPEND EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/managed_components/s00500_ESPUI)