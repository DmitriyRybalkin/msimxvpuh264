cmake_minimum_required(VERSION 2.8)

project(MSIMXVPULIB C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

add_subdirectory (src)

install(FILES cmake/Modules/CMakeMacroLibtoolFile.cmake DESTINATION share/cmake/Modules/)
install(FILES cmake/Modules/CMakeMacroLibtoolFile.cmake DESTINATION share/cmake-2.8/Modules/)