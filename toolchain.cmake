# CMake system name must be something like "Linux".
# This is important for cross-compiling.
set( WORKDIR_PREFIX /path/to/yocto/bitbake/sysroot/)
set( CMAKE_SYSTEM_NAME Linux )
set( CMAKE_SYSTEM_PROCESSOR arm )
set( CMAKE_C_COMPILER ${WORKDIR_PREFIX}/sysroots/x86_64-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-gcc )
set( CMAKE_CXX_COMPILER ${WORKDIR_PREFIX}/sysroots/x86_64-linux/usr/bin/arm-poky-linux-gnueabi/arm-poky-linux-gnueabi-g++ )
set( CMAKE_C_FLAGS " -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9  --sysroot=${WORKDIR_PREFIX}/sysroots/ventana  -O2 -pipe -g -feliminate-unused-debug-types" CACHE STRING "CFLAGS" )
set( CMAKE_CXX_FLAGS " -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9  --sysroot=${WORKDIR_PREFIX}/sysroots/ventana  -O2 -pipe -g -feliminate-unused-debug-types -fvisibility-inlines-hidden -fpermissive" CACHE STRING "CXXFLAGS" )
set( CMAKE_C_FLAGS_RELEASE "-O2 -pipe -g -feliminate-unused-debug-types  -O2 -pipe -g -feliminate-unused-debug-types -DNDEBUG" CACHE STRING "CFLAGS for release" )
set( CMAKE_CXX_FLAGS_RELEASE "-O2 -pipe -g -feliminate-unused-debug-types  -O2 -pipe -g -feliminate-unused-debug-types -fvisibility-inlines-hidden -DNDEBUG" CACHE STRING "CXXFLAGS for release" )
set( CMAKE_C_LINK_FLAGS " -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9  --sysroot=${WORKDIR_PREFIX}/sysroots/ventana  -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed" CACHE STRING "LDFLAGS" )
set( CMAKE_CXX_LINK_FLAGS " -march=armv7-a -mthumb-interwork -mfloat-abi=hard -mfpu=neon -mtune=cortex-a9  --sysroot=${WORKDIR_PREFIX}/sysroots/ventana  -O2 -pipe -g -feliminate-unused-debug-types -fvisibility-inlines-hidden -Wl,-O1 -Wl,--hash-style=gnu -Wl,--as-needed" CACHE STRING "LDFLAGS" )

# only search in the paths provided so cmake doesnt pick
# up libraries and tools from the native build machine
set( CMAKE_FIND_ROOT_PATH ${WORKDIR_PREFIX}/sysroots/ventana ${WORKDIR_PREFIX}/sysroots/x86_64-linux )
set( CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )

# We need to set the rpath to the correct directory as cmake does not provide any
# directory as rpath by default
set( CMAKE_INSTALL_RPATH  )

# Use native cmake modules
set( CMAKE_MODULE_PATH ${WORKDIR_PREFIX}/sysroots/ventana/usr/share/cmake/Modules/ )

# add for non /usr/lib libdir, e.g. /usr/lib64
set( CMAKE_LIBRARY_PATH ${WORKDIR_PREFIX}/sysroots/ventana/usr/lib ${WORKDIR_PREFIX}/sysroots/ventana/lib )
set( CMAKE_INCLUDE_PATH ${WORKDIR_PREFIX}/sysroots/ventana/usr/include )