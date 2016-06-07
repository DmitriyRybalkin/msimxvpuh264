# msimxvpuh264
This repo stores the source code for mediastreamer2 plugin which is based on Freescale iMX6 D/Q VPU Encoder/Decoder

The current project is based on source code of the following libraries and plugins:

1. [Freescale iMX VPU API](https://github.com/Freescale/libimxvpuapi)
2. [Mediastreamer2 lib](http://www.linphone.org/technical-corner/mediastreamer2/overview)
3. [H.264 encoder/decoder plugin for mediastreamer2 based on the openh264 library](https://github.com/Linphone-sync/msopenh264)

Cross-compilation:

The current project is based on hardware specific code of Freescale iMX VPU API, therefore, it can be cross-compiled and tested only as the shared library of Freescale platforms.

1. Create building directory: mkdir build && cd build
2. Configure with cmake toolchain: cmake ../ -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake (Note: you may see sample of toolchain.cmake file within the current directory)
3. make