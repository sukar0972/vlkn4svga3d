#!/bin/sh
set -eu
# The measured machine is Kaby Lake; Skylake instructions are supported there.
# This private build is for that lab, rather than a portable distribution.
VLKN_MESA_BUILD_DIR=${VLKN_MESA_BUILD_DIR:-build-vm119}
meson setup "$VLKN_MESA_BUILD_DIR" --buildtype=release \
    -Db_lto=false -Dc_args=-march=skylake -Dcpp_args=-march=skylake \
    -Dgallium-drivers=[] -Dvulkan-drivers=intel -Dplatforms=[] \
    -Degl=disabled -Dgbm=disabled -Dglx=disabled \
    -Dllvm=enabled -Dshared-llvm=enabled -Dgallium-rusticl=false \
    -Dtools=[] -Dvideo-codecs=[] -Dmesa-clc=enabled -Dprecomp-compiler=enabled
if [ "${VLKN_MESA_CONFIGURE_ONLY:-0}" != 1 ]; then
    ninja -C "$VLKN_MESA_BUILD_DIR" -j "${VLKN_MESA_JOBS:-2}"
fi
