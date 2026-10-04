#!/bin/sh
set -eu
cd /mesa
meson setup build-vlkn --buildtype=release -Dgallium-drivers=svga -Dvulkan-drivers=[] -Dplatforms=x11 -Degl=disabled -Dgbm=disabled -Dglx=dri -Dglvnd=true -Dgles1=disabled -Dgles2=enabled -Dllvm=enabled -Dshared-llvm=enabled -Dtools=[] -Dvideo-codecs=[] -Dgallium-va=disabled -Dgallium-vdpau=disabled -Dgallium-xa=disabled -Dgallium-nine=false -Dgallium-opencl=disabled
ninja -C build-vlkn -j4
