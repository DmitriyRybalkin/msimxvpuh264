cmake_minimum_required(VERSION 2.8)

set(CMAKE_AUTOMOC TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

project(MSIMXVPULIB C CXX)

include(CMakeMacroLibtoolFile)

find_package(PkgConfig QUIET)

# -------------------
# Find MEDIASTREAMER2
# -------------------
pkg_check_modules(PKG_MEDIASTREAMER2 QUIET mediastreamer)
set(MEDIASTREAMER2_DEFINITIONS ${PKG_MEDIASTREAMER2_CFLAGS})

find_path(MEDIASTREAMER2_INCLUDE_DIRS mediastreamer2/mediastream.h)
find_library(MEDIASTREAMER2_LIBRARIES NAMES mediastreamer_voip mediastreamer_base)

message("MEDIASTREAMER2_INCLUDE_DIRS include dir = ${MEDIASTREAMER2_INCLUDE_DIRS}")
message("MEDIASTREAMER2_LIBRARIES lib = ${MEDIASTREAMER2_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(MEDIASTREAMER2 DEFAULT_MSG
                                  MEDIASTREAMER2_LIBRARIES MEDIASTREAMER2_INCLUDE_DIRS)

mark_as_advanced(MEDIASTREAMER2_INCLUDE_DIRS MEDIASTREAMER2_LIBRARIES)

# --------------
# Find ORTP
# --------------
pkg_check_modules(PKG_ORTP QUIET ortp)
set(ORTP_DEFINITIONS ${PKG_ORTP_CFLAGS})

find_path(ORTP_INCLUDE_DIRS ortp/ortp.h)
find_library(ORTP_LIBRARIES NAMES ortp)

message("ORTP_INCLUDE_DIRS include dir = ${ORTP_INCLUDE_DIRS}")
message("ORTP_LIBRARIES lib = ${ORTP_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(ORTP DEFAULT_MSG
                                  ORTP_LIBRARIES ORTP_INCLUDE_DIRS)

mark_as_advanced(ORTP_INCLUDE_DIRS ORTP_LIBRARIES)

# --------------
# Find OPENH264
# --------------
pkg_check_modules(PKG_OPENH264 QUIET openh264)
set(OPENH264_DEFINITIONS ${PKG_OPENH264_CFLAGS})

find_path(OPENH264_INCLUDE_DIRS wels/codec_api.h wels/codec_app_def.h)
find_library(OPENH264_LIBRARIES NAMES libopenh264 openh264)

message("OPENH264_INCLUDE_DIRS include dir = ${OPENH264_INCLUDE_DIRS}")
message("OPENH264_LIBRARIES lib = ${OPENH264_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(OPENH264 DEFAULT_MSG
                                  OPENH264_LIBRARIES OPENH264_INCLUDE_DIRS)

mark_as_advanced(OPENH264_INCLUDE_DIRS OPENH264_LIBRARIES)

# --------------
# Find LIBXML2
# --------------
pkg_check_modules(PKG_LIBXML2 QUIET libxml-2.0)
set(LIBXML2_DEFINITIONS ${PKG_LIBXML2_CFLAGS})

find_path(LIBXML2_INCLUDE_DIRS NAMES libxml/xpath.h PATH_SUFFIXES libxml2)

find_library(LIBXML2_LIBRARIES NAMES xml2 libxml2)

message("LIBXML2_INCLUDE_DIRS include dir = ${LIBXML2_INCLUDE_DIRS}")
message("LIBXML2_LIBRARIES lib = ${LIBXML2_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LIBXML2 DEFAULT_MSG
                                  LIBXML2_LIBRARIES LIBXML2_INCLUDE_DIRS)

mark_as_advanced(LIBXML2_INCLUDE_DIRS LIBXML2_LIBRARIES)

# --------------
# Find GOBJECT
# --------------
pkg_check_modules(PKG_GOBJECT2 QUIET gobject-2.0)
set(GOBJECT_DEFINITIONS ${PKG_GOBJECT2_CFLAGS})
   
find_path(GOBJECT_INCLUDE_DIRS glib-2.0/gobject/gobject.h)

find_library(_GObjectLibs NAMES gobject-2.0)
find_library(_GModuleLibs NAMES gmodule-2.0)
find_library(_GThreadLibs NAMES gthread-2.0)
#find_library(_GLibs NAMES glib-2.0)

set(GOBJECT_LIBRARIES ${_GObjectLibs} ${_GModuleLibs} ${_GThreadLibs})

message("GOBJECT_INCLUDE_DIRS include dir = ${GOBJECT_INCLUDE_DIRS}")
message("GOBJECT_LIBRARIES lib = ${GOBJECT_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(GOBJECT DEFAULT_MSG
                                  GOBJECT_LIBRARIES GOBJECT_INCLUDE_DIRS)

mark_as_advanced(GOBJECT_INCLUDE_DIRS GOBJECT_LIBRARIES)

# --------------
# Find GLIB
# --------------
pkg_check_modules(PKG_GLIB2 QUIET glib-2.0)
set(GLIB2_DEFINITIONS ${PKG_GLIB2_CFLAGS})

find_path(GLIB2_MAIN_INCLUDE_DIR glib-2.0/glib.h)

find_library(GLIB2_LIBRARIES NAMES glib-2.0)

# search the glibconfig.h include dir under the same root where the library is found
find_path(GLIB2_INTERNAL_INCLUDE_DIR glib-2.0/include/glibconfig.h HINTS ${CMAKE_LIBRARY_PATH})

set(GLIB2_INCLUDE_DIRS ${GLIB2_MAIN_INCLUDE_DIR})
set(GLIB2_INCLUDE_DIRS ${GLIB2_INCLUDE_DIRS} ${GLIB2_INTERNAL_INCLUDE_DIR})

message("GLIB2_INCLUDE_DIRS include dir = ${GLIB2_INCLUDE_DIRS}")
message("GLIB2_LIBRARIES lib = ${GLIB2_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(GLIB2 DEFAULT_MSG
                                  GLIB2_LIBRARIES GLIB2_INCLUDE_DIRS)

mark_as_advanced(GLIB2_INCLUDE_DIRS GLIB2_LIBRARIES)

# --------------
# Find GSTREAMER
# --------------
set(GSTREAMER_ABI_VERSION "0.10")

pkg_check_modules(PKG_GSTREAMER QUIET gstreamer-${GSTREAMER_ABI_VERSION})
set(GSTREAMER_DEFINITIONS ${PKG_GSTREAMER_CFLAGS})

pkg_check_modules(PKG_GSTREAMER_APP QUIET gstreamer-app-${GSTREAMER_ABI_VERSION})
set(GSTREAMER_APP_DEFINITIONS ${PKG_GSTREAMER_APP_CFLAGS})

pkg_check_modules(PKG_GSTREAMER_BASE QUIET gstreamer-base-${GSTREAMER_ABI_VERSION})
set(GSTREAMER_BASE_DEFINITIONS ${PKG_GSTREAMER_BASE_CFLAGS})

find_path(GSTREAMER_INCLUDE_DIRS gst/gst.h PATH_SUFFIXES gstreamer-${GSTREAMER_ABI_VERSION})
find_path(GSTREAMER_APP_INCLUDE_DIRS gst/app/gstappsrc.h PATH_SUFFIXES gstreamer-${GSTREAMER_ABI_VERSION})
find_path(GSTREAMER_BASE_INCLUDE_DIRS gst/base/gstbasesrc.h PATH_SUFFIXES gstreamer-${GSTREAMER_ABI_VERSION})
find_library(GSTREAMER_LIBRARIES NAMES gstreamer-${GSTREAMER_ABI_VERSION})
find_library(GSTREAMER_BASE_LIBRARIES NAMES gstbase-${GSTREAMER_ABI_VERSION})
find_library(GSTREAMER_APP_LIBRARIES NAMES gstapp-${GSTREAMER_ABI_VERSION})

message("GSTREAMER_INCLUDE_DIRS include dir = ${GSTREAMER_INCLUDE_DIRS}")
message("GSTREAMER_BASE_INCLUDE_DIRS include dir = ${GSTREAMER_BASE_INCLUDE_DIRS}")
message("GSTREAMER_APP_INCLUDE_DIRS include dir = ${GSTREAMER_APP_INCLUDE_DIRS}")
message("GSTREAMER_LIBRARIES lib = ${GSTREAMER_LIBRARIES}")
message("GSTREAMER_BASE_LIBRARIES lib = ${GSTREAMER_BASE_LIBRARIES}")
message("GSTREAMER_APP_LIBRARIES lib = ${GSTREAMER_APP_LIBRARIES}")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(GSTREAMER DEFAULT_MSG
                                  GSTREAMER_LIBRARIES GSTREAMER_BASE_LIBRARIES GSTREAMER_APP_LIBRARIES GSTREAMER_INCLUDE_DIRS GSTREAMER_BASE_INCLUDE_DIRS GSTREAMER_APP_INCLUDE_DIRS)

mark_as_advanced(GSTREAMER_INCLUDE_DIRS GSTREAMER_BASE_INCLUDE_DIRS GSTREAMER_APP_INCLUDE_DIRS GSTREAMER_LIBRARIES GSTREAMER_BASE_LIBRARIES GSTREAMER_APP_LIBRARIES)

# --------------
# Assemble
# --------------
include_directories(${MEDIASTREAMER2_INCLUDE_DIRS})
include_directories(${ORTP_INCLUDE_DIRS})
include_directories(${OPENH264_INCLUDE_DIRS})
include_directories(${LIBXML2_INCLUDE_DIRS})
include_directories(${GOBJECT_INCLUDE_DIRS})
include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${GSTREAMER_BASE_INCLUDE_DIRS})
include_directories(${GSTREAMER_APP_INCLUDE_DIRS})

add_definitions(${MEDIASTREAMER2_DEFINITIONS})
add_definitions(${ORTP_DEFINITIONS})
add_definitions(${OPENH264_DEFINITIONS})
add_definitions(${LIBXML2_DEFINITIONS})
add_definitions(${GLIB2_DEFINITIONS})
add_definitions(${GOBJECT_DEFINITIONS})
add_definitions(${GSTREAMER_DEFINITIONS})
add_definitions(${GSTREAMER_APP_DEFINITIONS})
add_definitions(${GSTREAMER_BASE_DEFINITIONS})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(MSIMXVPULIB_SOURCES msimxvpuh264.cpp msimxvpuh264dec.cpp msimxvpuh264enc.cpp)
set(MSIMXVPULIB_HEADERS msimxvpuh264dec.h msimxvpuh264enc.h)

add_library(msimxvpuh264 SHARED ${MSIMXVPULIB_SOURCES} ${MSIMXVPULIB_HEADERS})

set(MSIMXVPUH264LIB_MAJOR_VERSION 0)
set(MSIMXVPUH264LIB_MINOR_VERSION 0)
set(MSIMXVPUH264LIB_PATCH_VERSION 0)
set(MSIMXVPUH264LIB_VERSION ${MSIMXVPUH264LIB_MAJOR_VERSION}.${MSIMXVPUH264LIB_MINOR_VERSION}.${MSIMXVPUH264LIB_PATCH_VERSION}) 

set_property(TARGET msimxvpuh264 PROPERTY VERSION ${MSIMXVPUH264LIB_VERSION})
set_property(TARGET msimxvpuh264 PROPERTY SOVERSION ${MSIMXVPUH264LIB_MAJOR_VERSION})

target_link_libraries(msimxvpuh264 ${OPENH264_LIBRARIES} ${MEDIASTREAMER2_LIBRARIES} ${ORTP_LIBRARIES} ${GLIB2_LIBRARIES} ${GOBJECT_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES} ${GSTREAMER_APP_LIBRARIES} ${LIBXML2_LIBRARIES} pthread)

set(LIB_INSTALL_PATH /usr/lib/mediastreamer/plugins)
set(INCLUDE_INSTALL_PATH /usr/include)

# Generate libtool *.la file
# FIXME: need to add dependency libs
create_libtool_file(msimxvpuh264 ${LIB_INSTALL_PATH})

install(FILES ${MSIMXVPULIB_HEADERS} DESTINATION ${INCLUDE_INSTALL_PATH})
install(TARGETS msimxvpuh264 LIBRARY DESTINATION ${LIB_INSTALL_PATH})
