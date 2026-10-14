#!/usr/bin/env python3
"""Administrator-owned hook for the prepared Oracle QEMU/lavapipe runtime.

Install outside the checkout as /opt/vlkn-ci/guest-hook. Configuration lives
in /opt/vlkn-ci/guest-hook.json. This is for a disposable trusted test host;
the chroot does not isolate hostile PR code from the host.
"""
import argparse
import fcntl
import grp
import hashlib
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import shlex
import signal
import subprocess
import sys
import time


CONFIG = Path("/opt/vlkn-ci/guest-hook.json")
FIXTURE_TESTS = ("fbo-blending-format-quirks",
                 "ext_packed_depth_stencil-depth-stencil-texture")


def fixture_manifest(directory):
    """Check the reviewed package before any guest executable is replaced."""
    directory = Path(directory).resolve()
    manifest = json.loads((directory / "manifest.json").read_text())
    if not isinstance(manifest, dict):
        raise ValueError("Invalid Piglit fixture manifest")
    files = manifest.get("files")
    source_digest = manifest.get("source_sha256")
    required = {f"{kind}/{name}" for kind in ("bin", "wrappers") for name in FIXTURE_TESTS}
    required.update(("patches/piglit-20220119-depth-stencil-error.patch",
                     "patches/piglit-20220119-fbo-blending-init.patch",
                     "lib/libpiglitutil.so.0", "lib/libpiglitutil_gl.so.0"))
    if (not isinstance(files, dict) or not required <= files.keys()
            or not isinstance(source_digest, str)
            or not re.fullmatch(r"[0-9a-f]{64}", source_digest)):
        raise ValueError("Incomplete Piglit fixture manifest")
    for name, digest in files.items():
        if not isinstance(name, str):
            raise ValueError("Invalid Piglit fixture filename")
        path = directory / name
        if (Path(name).is_absolute() or ".." in Path(name).parts
                or not path.resolve().is_relative_to(directory)
                or not path.is_file() or not isinstance(digest, str)
                or not re.fullmatch(r"[0-9a-f]{64}", digest)
                or sha256(path) != digest):
            raise ValueError("Invalid Piglit fixture or digest: " + name)
    entries = list(directory.rglob("*"))
    if any(path.is_symlink() and not path.is_file() for path in entries):
        raise ValueError("Piglit fixture bundle contains a directory or broken symlink")
    actual = {str(path.relative_to(directory)) for path in entries
              if path.is_file() and path != directory / "manifest.json"}
    if actual != files.keys():
        raise ValueError("Piglit fixture files differ from manifest")
    return manifest


def install_fixtures(config, output):
    if not config.get("piglit_fixtures"):
        return None
    directory = Path(config["piglit_fixtures"])
    manifest = fixture_manifest(directory)
    command(["runuser", "-u", config["ssh_user"], "--", "scp", "-qr", "-o",
             "BatchMode=yes", "-o", "ConnectTimeout=5", directory,
             f"{config['ssh']}:/tmp/vlkn-piglit-fixtures"])
    # Verify every transferred file before replacing either test. Keep the
    # installed originals and their hashes in the disposable overlay/evidence.
    script = '''import hashlib, json, shutil, subprocess, sys
from pathlib import Path
root = Path('/tmp/vlkn-piglit-fixtures')
manifest = json.loads(sys.argv[1])
version = subprocess.check_output(['dpkg-query', '-W', '-f=${Version}', 'piglit'], text=True)
if version != '0~git20220119-124bca3c9-1':
    raise RuntimeError('Unsupported Piglit package version: ' + version)
for name, digest in manifest['files'].items():
    if hashlib.sha256((root / name).read_bytes()).hexdigest() != digest:
        raise RuntimeError('Transferred Piglit fixture digest mismatch: ' + name)
destination = Path('/opt/vlkn-ci/piglit-fixtures')
if destination.exists():
    raise RuntimeError('Piglit fixtures already installed in this overlay')
shutil.copytree(root, destination)
originals = Path('/opt/vlkn-ci/piglit-original')
originals.mkdir()
hashes = {}
for name in sys.argv[2:]:
    installed = Path('/usr/lib/x86_64-linux-gnu/piglit/bin') / name
    hashes[name] = hashlib.sha256(installed.read_bytes()).hexdigest()
    shutil.copy2(installed, originals / name)
    shutil.copyfile(destination / 'wrappers' / name, installed)
    installed.chmod(0o755)
shutil.rmtree(root)
print(json.dumps({'package_version': version, 'original_sha256': hashes}))
'''
    originals = json.loads(guest_command(config, ["sudo", "python3", "-c", script,
                                                   json.dumps(manifest), *FIXTURE_TESTS], timeout=120))
    evidence = {"manifest": manifest, "manifest_sha256": sha256(directory / "manifest.json"),
                **originals}
    (output / "piglit-fixtures.json").write_text(json.dumps(evidence, indent=2) + "\n")
    return evidence


def command(argv, timeout=60):
    return subprocess.check_output(list(map(str, argv)), text=True,
                                   stderr=subprocess.STDOUT, timeout=timeout)


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def process_identity(pid):
    proc = Path(f"/proc/{pid}")
    try:
        # Include start time to avoid signaling a reused PID.
        fields = (proc / "stat").read_text().rsplit(")", 1)[1].split()
        return None if fields[0] == "Z" else {"pid": pid, "start": fields[19]}
    except FileNotFoundError:
        return None


def guest_command(config, argv, timeout=30):
    return command(["runuser", "-u", config["ssh_user"], "--", "ssh", "-o",
                    "BatchMode=yes", "-o", "ConnectTimeout=5", config["ssh"],
                    shlex.join(argv)], timeout)


def cleanup(config, output):
    root = Path(config["runtime"])
    work = root / "run/guest"
    owner = work / "owner.json"
    if not owner.exists():
        return
    state = json.loads(owner.read_text())
    if state["output"] != str(output):
        raise RuntimeError("Guest belongs to another stage; refusing cleanup")
    identity = state.get("process")
    pidfile = work / "qemu.pid"
    if identity is None and pidfile.exists():
        identity = process_identity(int(pidfile.read_text()))
        if identity is not None:
            # A launch interrupted before recording identity must still belong
            # to this runtime and use its unique pidfile.
            proc = Path(f"/proc/{identity['pid']}")
            if (proc / "root").resolve() != root:
                raise RuntimeError("PID file does not identify this runtime")
    if identity and process_identity(identity["pid"]) == identity:
        os.kill(identity["pid"], signal.SIGTERM)
        deadline = time.monotonic() + 30
        while process_identity(identity["pid"]) == identity and time.monotonic() < deadline:
            time.sleep(0.2)
        if process_identity(identity["pid"]) == identity:
            os.kill(identity["pid"], signal.SIGKILL)
            deadline = time.monotonic() + 10
            while process_identity(identity["pid"]) == identity and time.monotonic() < deadline:
                time.sleep(0.2)
        if process_identity(identity["pid"]) == identity:
            raise RuntimeError("QEMU could not be stopped; overlay retained")
    for source, name in ((work / "qemu.log", "qemu.log"),
                         (work / "serial.log", "serial.log"),
                         (root / "tmp/svga3d.log", "adapter.log")):
        if source.exists():
            shutil.copyfile(source, output / name)
    shutil.rmtree(work)
    (root / "run/libqemu_svga3d.so").unlink(missing_ok=True)
    log = output / "adapter.log"
    if log.exists() and any(marker in log.read_text(errors="replace") for marker in
                            ("[Vulkan Validation ERROR]", "[Vulkan Validation WARN]")):
        raise RuntimeError("Vulkan validation reported errors/warnings; inspect adapter.log")


def prepare(config, library, output, validation):
    root = Path(config["runtime"])
    work = root / "run/guest"
    if work.exists():
        raise RuntimeError("A graphics stage is already present; clean it up first")
    work.mkdir()
    (work / "owner.json").write_text(json.dumps({"output": str(output)}))
    account = pwd.getpwnam("graphics-ci")
    base = Path(config["base_image"])
    shutil.copyfile(config["seed"], work / "seed.iso")
    shutil.copyfile(library, root / "run/libqemu_svga3d.so")
    command(["qemu-img", "create", "-f", "qcow2", "-F", "qcow2", "-b", os.path.relpath(base, work),
             work / "disk.qcow2"])
    for path in work.iterdir():
        if path.name != "owner.json":
            os.chown(path, account.pw_uid, account.pw_gid)
    os.chown(work, account.pw_uid, account.pw_gid)
    (root / "tmp/svga3d.log").unlink(missing_ok=True)
    shutil.rmtree(root / "tmp/shader-cache", ignore_errors=True)
    # No pre-existing environment (including credentials or trace flags) is
    # passed to the process that loads the candidate constructor.
    argv = ["chroot", f"--userspec={account.pw_uid}:{account.pw_gid}",
            f"--groups={grp.getgrnam('kvm').gr_gid}", root,
            "/usr/bin/env", "-i", "HOME=/tmp", "PATH=/usr/bin:/bin",
            "MESA_SHADER_CACHE_DIR=/tmp/shader-cache",
            "VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json",
            f"SVGA3_VLKN_VALIDATE={validation}", "LP_NUM_THREADS=1",
            "LD_PRELOAD=/run/libqemu_svga3d.so", "/usr/bin/qemu-system-x86_64",
            "-machine", "q35,accel=kvm", "-cpu", "host", "-smp", "1", "-m", "512",
            "-drive", "file=/run/guest/disk.qcow2,if=virtio,format=qcow2",
            "-drive", "file=/run/guest/seed.iso,media=cdrom,readonly=on",
            "-vga", "vmware", "-display", "none", "-serial", "file:/run/guest/serial.log",
            "-netdev", f"user,id=n0,hostfwd=tcp:127.0.0.1:{config['ssh_port']}-:22",
            "-device", "virtio-net-pci,netdev=n0", "-daemonize", "-pidfile", "/run/guest/qemu.pid"]
    (output / "launch-command.txt").write_text(shlex.join(map(str, argv)) + "\n")
    # Use a file, rather than a captured pipe: a daemonized child may retain
    # stderr after its launcher exits, preventing check_output from returning.
    with (work / "qemu.log").open("w") as log:
        subprocess.run(list(map(str, argv)), stdout=log, stderr=subprocess.STDOUT,
                       check=True, timeout=60)
    pid = int((work / "qemu.pid").read_text())
    identity = process_identity(pid)
    if identity is None:
        raise RuntimeError("QEMU exited during preparation")
    (work / "owner.json").write_text(json.dumps({"output": str(output), "process": identity}))
    deadline = time.monotonic() + 360
    while True:
        try:
            guest_command(config, ["sh", "-c",
                "systemctl is-active --quiet graphics-xorg.service && DISPLAY=:0 glxinfo -B >/dev/null"])
            break
        except subprocess.SubprocessError:
            if process_identity(pid) != identity or time.monotonic() >= deadline:
                raise RuntimeError("Guest SSH/Xorg did not become ready")
            time.sleep(2)
    if config.get("mesa_driver"):
        # Stage a reviewed host module into each disposable overlay. This lets
        # the gate pin a corrected driver without changing the shared base disk.
        command(["runuser", "-u", config["ssh_user"], "--", "scp", "-q", "-o",
                 "BatchMode=yes", "-o", "ConnectTimeout=5", config["mesa_driver"],
                 f"{config['ssh']}:/tmp/vlkn-ci-mesa.so"])
        guest_command(config, ["sudo", "install", "-m", "0755", "/tmp/vlkn-ci-mesa.so",
                               "/opt/mesa-vlkn/dri/vmwgfx_dri.so"])
        guest_command(config, ["rm", "/tmp/vlkn-ci-mesa.so"])
    fixtures = install_fixtures(config, output)
    guest_packages = guest_command(config, ["dpkg-query", "-W", "piglit", "glmark2",
                                           "libgl1-mesa-dri", "xserver-xorg-core"])
    guest_mesa = guest_command(config, ["sha256sum", "/opt/mesa-vlkn/dri/vmwgfx_dri.so"])
    if config.get("mesa_driver") and guest_mesa.split()[0] != sha256(config["mesa_driver"]):
        raise RuntimeError("Guest Mesa module does not match the pinned host module")
    glxinfo = guest_command(config, ["env", "DISPLAY=:0", "LIBGL_DRIVERS_PATH=/opt/mesa-vlkn/dri",
                                   "SVGA_VLKN_EXTENDED_STATE=1", "glxinfo", "-B"])
    if "OpenGL renderer string: SVGA3D" not in glxinfo:
        raise RuntimeError("Guest does not expose SVGA3D")
    (output / "guest-glxinfo.txt").write_text(glxinfo)
    proc = Path(f"/proc/{pid}")
    maps = (proc / "maps").read_text()
    env = dict(item.split("=", 1) for item in (proc / "environ").read_bytes().decode().split("\0")
               if "=" in item)
    expected = sha256(library)
    loaded = proc / "root/run/libqemu_svga3d.so"
    mapped = [line for line in maps.splitlines() if line.endswith("/run/libqemu_svga3d.so")]
    layers = "libVkLayer_khronos_validation.so" in maps
    if (not mapped or any(int(line.split()[4]) != loaded.stat().st_ino for line in mapped)
            or sha256(loaded) != expected or env.get("SVGA3_VLKN_VALIDATE") != str(validation)
            or layers != bool(validation) or "libvulkan_lvp.so" not in maps):
        raise RuntimeError("Loaded library, lavapipe or validation layer does not match request")
    (output / "process-maps.txt").write_text(maps)
    (output / "process-settings.json").write_text(json.dumps({
        "process": identity, "owner": account.pw_name, "library_sha256": expected,
        "validation_layer_loaded": layers, "environment": env}, indent=2) + "\n")
    result = {"ssh": config["ssh"], "display": ":0", "mesa_driver_path": "/opt/mesa-vlkn/dri",
              "extended_state": True, "loaded_library_sha256": expected, "validation": validation,
              "conditions": {**config["conditions"], "base_image_sha256": sha256(base),
                  "qemu_sha256": sha256(root / "usr/bin/qemu-system-x86_64"),
                  "vulkan_icd_sha256": sha256(root / "usr/lib/x86_64-linux-gnu/libvulkan_lvp.so"),
                  "seed_sha256": sha256(config["seed"]),
                  "hook_sha256": sha256(__file__),
                  "guest_packages": guest_packages, "guest_mesa": guest_mesa,
                  "piglit_fixtures": fixtures,
                  "launch": {"vcpus": 1, "memory_mib": 512, "lp_num_threads": 1},
                  "host_kernel": os.uname().release}}
    (output / "guest.json").write_text(json.dumps(result, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=("prepare", "cleanup"))
    parser.add_argument("--library", type=Path)
    parser.add_argument("--validation", type=int, choices=(0, 1))
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.geteuid() != 0:
        parser.error("Run the installed administrator hook through sudo")
    config = json.loads(CONFIG.read_text())
    output = args.output.resolve()
    with (Path(config["runtime"]) / "run/hook.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        if args.operation == "cleanup":
            cleanup(config, output)
        else:
            if args.library is None or args.validation is None:
                parser.error("prepare requires --library and --validation")
            prepare(config, args.library.resolve(), output, args.validation)


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print(f"Guest hook failed: {error}", file=sys.stderr)
        sys.exit(1)
