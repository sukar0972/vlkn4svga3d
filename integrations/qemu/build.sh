#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Run inside the supplied Docker image, from the repository root.
set -eu
root=$(pwd)
out=${1:?usage: integrations/qemu/build.sh /absolute/output/path}
case "$out" in /*) ;; *) echo 'Output must be an absolute path' >&2; exit 1;; esac
jobs=${BUILD_JOBS:-4}
mkdir -p "$out"
archive="$out/qemu-9.2.0.tar.xz"
if [ ! -f "$archive" ]; then curl -fL https://download.qemu.org/qemu-9.2.0.tar.xz -o "$archive"; fi
printf '%s  %s\n' f859f0bc65e1f533d040bbe8c92bcfecee5af2c921a6687c652fb44d089bd894 "$archive" | sha256sum -c -
if [ ! -d "$out/qemu-9.2.0" ]; then tar -xJf "$archive" -C "$out"; fi
if [ ! -f "$out/qemu-9.2.0/hw/display/vlkn-svga.c" ]; then
    sh integrations/qemu/apply.sh "$out/qemu-9.2.0"
else
    patch --dry-run --reverse -d "$out/qemu-9.2.0" -p1 < integrations/qemu/qemu-9.2.0.patch
fi
for file in vlkn-svga.c vlkn-svga.h; do
    if ! cmp -s "integrations/qemu/$file" "$out/qemu-9.2.0/hw/display/$file"; then
        cp "integrations/qemu/$file" "$out/qemu-9.2.0/hw/display/$file"
    fi
done
make -j"$jobs" BUILD_DIR="$out/core-build" LIB_DIR="$out/lib" install PREFIX="$out/prefix"
export PKG_CONFIG_PATH="$out/prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
mkdir -p "$out/qemu-build"
cd "$out/qemu-build"
if [ ! -f build.ninja ]; then
    ../qemu-9.2.0/configure --target-list=x86_64-softmmu --disable-docs \
        --disable-werror --disable-gtk --disable-sdl --disable-plugins \
        --disable-tools --disable-user --disable-rust --disable-slirp \
        --enable-tcg --disable-download
fi
ninja -j"$jobs" qemu-system-x86_64
cd "$root"
cc -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -O2 -c integrations/qemu/guest-smoke.c -o "$out/guest.o"
cc -m32 -c integrations/qemu/guest-smoke.S -o "$out/guest-start.o"
ld -m elf_i386 -T integrations/qemu/guest-smoke.ld "$out/guest-start.o" "$out/guest.o" -o "$out/guest.elf"
export VK_ICD_FILENAMES="${VK_ICD_FILENAMES:-$(find /usr/share/vulkan/icd.d -name 'lvp_icd*.json' -print -quit)}"
export XDG_CACHE_HOME="${XDG_CACHE_HOME:-/tmp}"
export SVGA3_VLKN_VALIDATE=1
export VK_LAYER_VALIDATE_SYNC=1
python3 -B integrations/qemu/test_native.py "$out/qemu-build/qemu-system-x86_64" --kernel "$out/guest.elf" --output "$out/results"
