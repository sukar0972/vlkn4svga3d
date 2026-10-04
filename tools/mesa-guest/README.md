# Mesa guest state patch

Mesa 22.3.6's VGPU9 path replaces constant-alpha blend factors with constant-color factors and shares stencil references and masks between faces. The patch preserves this state for the VLKN backend when `SVGA_VLKN_EXTENDED_STATE=1` is set. Without that option, Mesa retains its existing behavior.

The stencil extension uses the existing reference, compare-mask and write-mask words. Bits 31:16 contain `0x564c`, bits 7:0 hold the clockwise face's value, and bits 15:8 hold the counterclockwise face's value. The backend decodes this representation for two-sided stencil draws. Constant-alpha blending uses the existing `BLENDFACTORALPHA` and `INVBLENDFACTORALPHA` values.

Build the guest module in Debian 12 with LLVM 15, matching VM119. From the repository root:

```sh
mkdir -p artifacts/mesa-guest
curl -fL https://archive.mesa3d.org/older-versions/22.x/mesa-22.3.6.tar.xz -o artifacts/mesa-guest/mesa-22.3.6.tar.xz
printf '%s\n' '4ec8ec65dbdb1ee9444dba72970890128a19543a58cf05931bd6f54f124e117f  artifacts/mesa-guest/mesa-22.3.6.tar.xz' | sha256sum -c -
tar -xJf artifacts/mesa-guest/mesa-22.3.6.tar.xz -C artifacts/mesa-guest
patch -d artifacts/mesa-guest/mesa-22.3.6 -p1 < patches/mesa-22.3.6-vlkn-state.patch
docker build -t vlkn-mesa-guest:22.3.6 tools/mesa-guest
docker run --rm --cpus=4 \
  -v "$PWD/artifacts/mesa-guest/mesa-22.3.6:/mesa" \
  -v "$PWD/tools/mesa-guest/build.sh:/build.sh:ro" \
  vlkn-mesa-guest:22.3.6 sh /build.sh
```

Place the built Gallium DRI module in a guest directory as `vmwgfx_dri.so`. Select it for a test process with `MESA_DRIVERS_PATH=/path/to/dri` and `SVGA_VLKN_EXTENDED_STATE=1`. The installed system driver can remain available for comparison. Software reference runs should use the system driver and unset both variables.

Record the module hash and these options alongside the backend hash. A change to either driver requires a new comparison. This guest patch is intended for the VLKN backend.
