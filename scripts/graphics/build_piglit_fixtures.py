#!/usr/bin/env python3
"""Build the two repaired fixtures from the pinned Debian Piglit source archive."""
import argparse
import json
from pathlib import Path
import shlex
import shutil
import subprocess
import tarfile

from oracle_guest_hook import FIXTURE_TESTS, fixture_manifest, sha256

SOURCE_SHA256 = "322e1b88aae35f92e15893b19e51ecd7854312f45e44c20c7a10cdde2adac454"
PATCHES = ("piglit-20220119-depth-stencil-error.patch",
           "piglit-20220119-fbo-blending-init.patch")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True, help="New build/evidence directory")
    args = parser.parse_args()
    if sha256(args.source) != SOURCE_SHA256:
        parser.error("Source archive does not match Debian Piglit 20220119")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    source = output / "source"
    source.mkdir()
    with tarfile.open(args.source) as archive:
        # Debian 12's Python 3.11 lacks tarfile extraction filters. This
        # pinned archive contains only files/directories; reject links and
        # escaping paths explicitly before extracting with that version.
        for member in archive.getmembers():
            path = Path(member.name)
            if (path.is_absolute() or ".." in path.parts
                    or not (member.isfile() or member.isdir())):
                raise ValueError("Unsafe Piglit archive member: " + member.name)
        archive.extractall(source)
    roots = list(source.iterdir())
    if len(roots) != 1 or not roots[0].is_dir():
        raise ValueError("Expected one Piglit source directory")
    source = roots[0]
    package = output / "fixtures"
    for name in ("bin", "lib", "wrappers", "patches"):
        (package / name).mkdir(parents=True)
    patch_root = Path(__file__).resolve().parents[2] / "patches"
    build = output / "build"
    with (output / "build.log").open("w") as log:
        def run(argv, **kwargs):
            log.write(shlex.join(map(str, argv)) + "\n")
            log.flush()
            subprocess.run(list(map(str, argv)), stdout=log, stderr=subprocess.STDOUT,
                           check=True, timeout=900, **kwargs)

        for name in PATCHES:
            shutil.copyfile(patch_root / name, package / "patches" / name)
            with (package / "patches" / name).open() as patch:
                run(["patch", "--batch", "--forward", "-d", source, "-p1"], stdin=patch)
        run(["cmake", "-S", source, "-B", build, "-DCMAKE_BUILD_TYPE=Release",
             "-DPIGLIT_BUILD_GLES1_TESTS=OFF", "-DPIGLIT_BUILD_GLES2_TESTS=OFF",
             "-DPIGLIT_BUILD_GLES3_TESTS=OFF", "-DPIGLIT_BUILD_VK_TESTS=OFF",
             "-DPIGLIT_BUILD_DMA_BUF_TESTS=OFF"])
        run(["cmake", "--build", build, "--parallel", "1", "--target", *FIXTURE_TESTS])
    for name in FIXTURE_TESTS:
        shutil.copyfile(build / "bin" / name, package / "bin" / name)
        (package / "bin" / name).chmod(0o755)
        wrapper = package / "wrappers" / name
        wrapper.write_text("#!/bin/sh\nexport LD_LIBRARY_PATH=/opt/vlkn-ci/piglit-fixtures/lib\n"
                           "exec /opt/vlkn-ci/piglit-fixtures/bin/" + name + ' "$@"\n')
        wrapper.chmod(0o755)
    for path in (build / "lib").glob("*.so*"):
        if path.is_file():
            shutil.copyfile(path, package / "lib" / path.name)
    manifest = {"source_sha256": SOURCE_SHA256,
                "version": "Debian piglit 0~git20220119-124bca3c9-1; two fixture repairs",
                "files": {str(path.relative_to(package)): sha256(path)
                          for path in sorted(package.rglob("*")) if path.is_file()}}
    (package / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    fixture_manifest(package)
    print(package)


if __name__ == "__main__":
    main()
