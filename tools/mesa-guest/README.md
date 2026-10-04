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

Place the built Gallium DRI module in a guest directory as `vmwgfx_dri.so`. Select it for a test process with `LIBGL_DRIVERS_PATH=/path/to/dri` and `SVGA_VLKN_EXTENDED_STATE=1`. The installed system driver can remain available for comparison. Software reference runs should use the system driver and unset both variables.

Record the module hash and these options alongside the backend hash. A change to either driver requires a new comparison. This guest patch is intended for the VLKN backend.

Run the paired Piglit comparison with the custom hardware module and an unchanged software reference:

```sh
python3 scripts/run_piglit.py --ssh svga3d@10.0.0.144 \
  --mesa-driver-path /home/svga3d/vlkn-mesa/dri --extended-state \
  --test-timeout 600 --run-timeout 14400
```

The runner checks the loaded module path and records its SHA-256. It clears the driver path and extension environment for the llvmpipe reference.

Color-write states use the same `0x564c` tag, retain the RGBA mask in bits 0–3, and mark a logical RGB attachment with bit 8. Mesa emits each attachment separately so blending can treat its destination alpha as one even when the physical image is RGBA.

The opt-in driver also requests Mesa’s logical RGB destination-alpha override, emits valid 1D shadow sampler declarations, and copies matching packed depth/stencil pixels through CPU maps when framebuffer orientation requires a flip. The fallback preserves both planes and snapshots the source before writing, including overlapping copies; scaling and multisampling retain the existing paths.

Compile `accuracy_probe.c` in the guest with `cc -O2 -Wall -Wextra accuracy_probe.c -o accuracy_probe -lGL -lX11 -lm`. Run it with the custom driver variables above, then with `LIBGL_ALWAYS_SOFTWARE=1` and the custom variables unset. It checks asymmetric depth and stencil data through all four framebuffer orientations, an overlapping packed copy, and unchanged texture state after rejected unsized storage.

The 2022 Piglit depth/stencil texture case rejects `GL_INVALID_ENUM` for unsized cube storage when depth cube maps are unavailable. This also fails on llvmpipe with GL 2.1 and `GL_EXT_gpu_shader4` disabled. [ARB_texture_storage](https://registry.khronos.org/OpenGL/extensions/ARB/ARB_texture_storage.txt) requires `INVALID_ENUM` for unsized internal formats; [OpenGL error semantics](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf) allow any applicable error when several conditions hold. The optional `patches/piglit-20220119-depth-stencil-error.patch` accepts both applicable errors for this one call. Keep unmodified Piglit results alongside any corrected run; the patch does not waive pixel failures or unsupported features.

To use the corrected driver for desktop applications on Debian 12 amd64, add the contents of `xsessionrc.example` to the guest user's `~/.xsessionrc` and log in again. The example assumes the module is in `~/vlkn-mesa/dri` and keeps the system DRI directory as a fallback for software rendering. Back up any existing session file first. Software comparison commands must still unset both variables, as the Piglit runner does automatically. Verify the default client loader with `DISPLAY=:0 LD_DEBUG=files glxinfo -B`; it should load the custom `vmwgfx_dri.so`.
