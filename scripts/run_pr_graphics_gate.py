#!/usr/bin/env python3
"""Compare real guest Piglit and glmark2 runs for two source checkouts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import statistics
import subprocess
import sys

from run_piglit import CLEAN_ENV, load_results

RELEASE_FLAGS = "-O2 -std=c++17 -Wall -Wextra -pthread -fPIC -MMD -MP -DNDEBUG"


def parse_glmark(text):
    scores = re.findall(r"^\s*glmark2 Score:\s*(\d+)\s*$", text, re.M)
    renderers = re.findall(r"^\s*GL_RENDERER:\s*(.+)$", text, re.M)
    versions = re.findall(r"^[ \t]*glmark2 ([0-9][^\r\n]*)$", text, re.M)
    scenes = re.findall(r"^(\[.+?\].*?): FPS:\s*\d+(?:\.\d+)?\s+FrameTime:", text, re.M)
    if (len(scores) != 1 or int(scores[0]) <= 0 or len(renderers) != 1
            or "SVGA3D" not in renderers[0] or len(versions) != 1 or not scenes):
        raise RuntimeError("Missing/invalid glmark2 score, scene list, version or SVGA3D identity")
    return {"score": int(scores[0]), "renderer": renderers[0],
            "version": versions[0], "scenes": scenes}


def compare_piglit(base, candidate):
    # The runner's exit status checks failures/subtests/completeness. Additionally
    # prevent a new skip or a reduced selection from hiding a regression.
    for renderer in ("svga", "llvmpipe"):
        before = load_results(base / renderer)
        after = load_results(candidate / renderer)
        if not before or before.keys() != after.keys():
            raise RuntimeError(f"Piglit {renderer} test selections differ")
        for name, case in before.items():
            if case["result"] == "pass" and after[name]["result"] != "pass":
                raise RuntimeError(f"Piglit {renderer} lost a passing case: {name}")
            previous = case.get("subtests", {})
            current = after[name].get("subtests", {})
            if previous.keys() != current.keys():
                raise RuntimeError(f"Piglit {renderer} subtest selections differ: {name}")
            for subtest, result in previous.items():
                if result == "pass" and current[subtest] != "pass":
                    raise RuntimeError(f"Piglit {renderer} lost a passing subtest: {name}/{subtest}")


def compare_scores(samples):
    if set(samples) != {"base", "candidate"} or any(len(runs) != 3 for runs in samples.values()):
        raise RuntimeError("glmark2 requires three samples for each build")
    reference = samples["base"][0]
    for runs in samples.values():
        for run in runs:
            for key in ("renderer", "version", "scenes"):
                if run[key] != reference[key]:
                    raise RuntimeError(f"glmark2 {key} changed between measurements")
    medians = {name: statistics.median(run["score"] for run in runs)
               for name, runs in samples.items()}
    if medians["candidate"] < medians["base"]:
        raise RuntimeError(f"glmark2 regression: {medians['candidate']} < {medians['base']}")
    return medians


def run(argv, log, timeout):
    print(shlex.join(map(str, argv)), flush=True)
    with log.open("w") as stream:
        process = subprocess.Popen(list(map(str, argv)), stdout=stream, stderr=subprocess.STDOUT,
                                   start_new_session=True)
        try:
            status = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # SIGTERM allows an administrator's sudo wrapper to forward the
            # signal and release the hook lock before the cleanup operation.
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()
            raise
        if status:
            raise subprocess.CalledProcessError(status, argv)


def prepare(hook, library, output, validation):
    run([hook, "prepare", "--library", library, "--validation", str(validation),
         "--output", output], output / "prepare.log", 600)
    config = json.loads((output / "guest.json").read_text())
    if not isinstance(config, dict):
        raise RuntimeError("Guest hook must return a JSON object")
    expected_hash = hashlib.sha256(library.read_bytes()).hexdigest()
    if (config["loaded_library_sha256"] != expected_hash or type(config["validation"]) is not int
            or config["validation"] != validation):
        raise RuntimeError("Guest hook did not verify the requested library/validation mode")
    ssh = config["ssh"]
    if not isinstance(ssh, str) or not ssh or ssh.startswith("-") or any(c.isspace() for c in ssh):
        raise RuntimeError("Invalid guest SSH destination")
    if not isinstance(config["conditions"], dict) or not config["conditions"]:
        raise RuntimeError("Guest hook must record matched machine/workload conditions")
    if not isinstance(config.get("display", ":0"), str) or not config.get("display", ":0"):
        raise RuntimeError("Invalid guest display")
    mesa = config.get("mesa_driver_path")
    if mesa is not None and (not isinstance(mesa, str) or not mesa.startswith("/")):
        raise RuntimeError("Guest Mesa driver path must be absolute")
    if type(config.get("extended_state", False)) is not bool or (config.get("extended_state") and not mesa):
        raise RuntimeError("Extended state requires a Mesa driver path and a boolean setting")
    return config


def guest_env(config):
    env = ["env"]
    for key in CLEAN_ENV:
        env += ["-u", key]
    env += [f"DISPLAY={config.get('display', ':0')}", "vblank_mode=0", "__GL_SYNC_TO_VBLANK=0"]
    if config.get("mesa_driver_path"):
        env += [f"LIBGL_DRIVERS_PATH={config['mesa_driver_path']}"]
    if config.get("extended_state"):
        env += ["SVGA_VLKN_EXTENDED_STATE=1"]
    return env


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--guest-hook", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    hook = args.guest_hook.resolve()
    sources = {"base": args.base.resolve(), "candidate": args.candidate.resolve()}
    report = {"passed": False, "revisions": {}, "libraries": {}, "stages": [], "build_flags": RELEASE_FLAGS,
              "samples": {"base": [], "candidate": []}}
    conditions = None
    piglit_dirs = {}
    try:
        if not hook.is_file():
            raise RuntimeError(f"Provision the dedicated guest hook first: {hook}")
        libraries = {}
        for name, source in sources.items():
            report["revisions"][name] = subprocess.check_output(
                ["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
            build = out / f"{name}-build"
            # The PR base may predate preload-lab-release. Build the existing
            # preload-lab target with identical flags and fresh output paths.
            run(["make", "-C", source, "-j2", f"BUILD_DIR={build / 'obj'}",
                 f"BIN_DIR={build / 'bin'}", f"LIB_DIR={build / 'lib'}",
                 f"CXXFLAGS={RELEASE_FLAGS}", "preload-lab"],
                out / f"{name}-build.log", 900)
            libraries[name] = build / "lib/libqemu_svga3d.so"
            report["libraries"][name] = hashlib.sha256(libraries[name].read_bytes()).hexdigest()
        # First run accuracy with Vulkan validation, then interleave fresh-boot
        # warmed release measurements. Reverse order every pair to reduce drift.
        stages = [(name, "piglit", 1) for name in sources]
        for index in range(3):
            order = ("base", "candidate") if index % 2 == 0 else ("candidate", "base")
            stages += [(name, f"glmark2-{index + 1}", 0) for name in order]
        for name, stage, validation in stages:
            directory = out / f"{name}-{stage}"
            directory.mkdir()
            library = libraries[name]
            record = {"build": name, "stage": stage, "validation": validation}
            report["stages"].append(record)
            try:
                config = prepare(hook, library, directory, validation)
                # Validation mode and library hash intentionally differ; all
                # other settings must match, including Mesa path and protocol.
                matched = {key: config.get(key) for key in
                           ("conditions", "display", "mesa_driver_path", "extended_state")}
                if conditions is None:
                    conditions = matched
                    report["conditions"] = matched
                elif matched != conditions:
                    raise RuntimeError("Guest conditions changed between builds/runs")
                if stage == "piglit":
                    piglit_dir = directory / "piglit"
                    argv = [sys.executable, Path(__file__).with_name("run_piglit.py"),
                            "--ssh", config["ssh"], "--display", config.get("display", ":0"),
                            "--test-timeout", "600", "--run-timeout", "3600", "--setup-timeout", "120",
                            "--output", piglit_dir]
                    if config.get("mesa_driver_path"):
                        argv += ["--mesa-driver-path", config["mesa_driver_path"]]
                    if config.get("extended_state"):
                        argv += ["--extended-state"]
                    run(argv, directory / "piglit.log", 8100)
                    piglit_dirs[name] = piglit_dir
                    if len(piglit_dirs) == 2:
                        compare_piglit(piglit_dirs["base"], piglit_dirs["candidate"])
                else:
                    ssh = ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=8", config["ssh"]]
                    phases = {}
                    for phase, duration in (("warmup", "0.5"), ("timed", "1.0")):
                        command = guest_env(config) + ["timeout", "--kill-after=15s", "600",
                                  "glmark2", "--size", "800x600", "--benchmark", f":duration={duration}"]
                        log = directory / f"{phase}.log"
                        run(ssh + [shlex.join(command)], log, 630)
                        result = parse_glmark(log.read_text())
                        phases[phase] = result
                    if any(phases["warmup"][key] != result[key] for key in ("renderer", "version", "scenes")):
                        raise RuntimeError("glmark2 identity or scenes changed after warmup")
                    report["samples"][name].append(result)
                record["completed"] = True
            except Exception as error:
                record["error"] = str(error)
                raise
            finally:
                # The hook must also clean up a partially failed prepare.
                try:
                    run([hook, "cleanup", "--output", directory], directory / "cleanup.log", 600)
                    record["cleaned_up"] = True
                except (OSError, subprocess.SubprocessError) as error:
                    record["cleanup_error"] = str(error)
                    raise RuntimeError(f"{record.get('error', 'Stage completed')}; cleanup failed: {error}") from error
        report["medians"] = {name: statistics.median(run["score"] for run in runs)
                             for name, runs in report["samples"].items()}
        compare_scores(report["samples"])
        report["passed"] = True
        print(f"PASS: Piglit; glmark2 medians {report['medians']}")
        return 0
    except (RuntimeError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        report["error"] = str(error)
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    finally:
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    sys.exit(main())
