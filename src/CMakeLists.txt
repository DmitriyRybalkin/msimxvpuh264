cmake_minimum_required(VERSION 2.8)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

project(MSIMXVPULIB C CXX)

include(CMakeMacroLibtoolFile)

find_package(PkgConfig REQUIRED)
# Find mediastreamer2 with pkg-config
pkg_check_modules(MEDIASTREAMER2 REQUIRED mediastreamer)
# Find ortp with pkg-config
pkg_check_modules(ORTP REQUIRED ortp)

# NOTE: make sure that variables CMAKE_LIBRARY_PATH and CMAKE_INCLUDE_PATH are properly set in toolchain.cmake
find_path(LIBIMXVPUAPI_INCLUDE_DIRS imxvpuapi/imxvpuapi.h)
find_library(LIBIMXVPUAPI_LIBRARIES NAMES libimxvpuapi imxvpuapi)

message("LIBIMXVPUAPI_INCLUDE_DIRS include dir = ${LIBIMXVPUAPI_INCLUDE_DIRS}")
message("LIBIMXVPUAPI_LIBRARIES lib = ${LIBIMXVPUAPI_LIBRARIES}")

include(FindPackageHandleStandardArgs)

# Handle the QUIETLY and REQUIRED arguments and set the LIBIMXVPUAPI_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LIBIMXVPUAPI DEFAULT_MSG
                                  LIBIMXVPUAPI_LIBRARIES LIBIMXVPUAPI_INCLUDE_DIRS)

mark_as_advanced(LIBIMXVPUAPI_INCLUDE_DIRS LIBIMXVPUAPI_LIBRARIES)

include_directories(${LIBIMXVPUAPI_INCLUDE_DIRS})
include_directories(${MEDIASTREAMER2_INCLUDE_DIRS})
include_directories(${ORTP_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${LIBIMXVPUAPI_CFLAGS_OTHER} ${MEDIASTREAMER2_CFLAGS_OTHER} ${ORTP_CFLAGS_OTHER} -std=c++11")

set(MSIMXVPULIB_SOURCES msimxvpuh264.cpp msimxvpuh264dec.cpp msimxvpuh264dec.cpp)
set(MSIMXVPULIB_HEADERS msimxvpuh264dec.h msimxvpuh264enc.h)

add_library(msimxvpuh264 SHARED ${MSIMXVPULIB_SOURCES})

set(MSIMXVPUH264LIB_MAJOR_VERSION 0)
set(MSIMXVPUH264LIB_MINOR_VERSION 0)
set(MSIMXVPUH264LIB_PATCH_VERSION 0)
set(MSIMXVPUH264LIB_VERSION ${MSIMXVPUH264LIB_MAJOR_VERSION}.${MSIMXVPUH264LIB_MINOR_VERSION}.${MSIMXVPUH264LIB_PATCH_VERSION}) 

set_property(TARGET msimxvpuh264 PROPERTY VERSION ${MSIMXVPUH264LIB_VERSION})
set_property(TARGET msimxvpuh264 PROPERTY SOVERSION ${MSIMXVPUH264LIB_MAJOR_VERSION})

target_link_libraries(msimxvpuh264 ${LIBIMXVPUAPI_LIBRARIES} ${MEDIASTREAMER2_LIBRARIES} ${ORTP_LIBRARIES} pthread)

set(LIB_INSTALL_PATH /usr/lib/mediastreamer/plugins)
set(INCLUDE_INSTALL_PATH /usr/include)

# Generate libtool *.la file
# FIXME: need to add dependency libs
create_libtool_file(msimxvpuh264 ${LIB_INSTALL_PATH})

install(FILES ${MSIMXVPULIB_HEADERS} DESTINATION ${INCLUDE_INSTALL_PATH})
install(TARGETS msimxvpuh264 LIBRARY DESTINATION ${LIB_INSTALL_PATH})
