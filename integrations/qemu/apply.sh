#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
src=${1:?usage: apply.sh /path/to/qemu-9.2.0}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(cat "$src/VERSION")" = 9.2.0 || { echo 'Expected QEMU 9.2.0 source' >&2; exit 1; }
patch --dry-run -d "$src" -p1 < "$here/qemu-9.2.0.patch"
patch -d "$src" -p1 < "$here/qemu-9.2.0.patch"
cp "$here/vlkn-svga.c" "$here/vlkn-svga.h" "$src/hw/display/"
