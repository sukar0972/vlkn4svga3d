"""Check that missing evidence and accuracy/performance regressions fail the gate."""
import json
from contextlib import redirect_stdout, redirect_stderr
import io
from pathlib import Path
import subprocess
from unittest.mock import patch
import sys
import tempfile
import time
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import run_pr_graphics_gate as gate


LOG = """    glmark2 2023.01
    GL_RENDERER: SVGA3D; build: RELEASE; LLVM;
[build] use-vbo=false: FPS: 300 FrameTime: 3.333 ms
[texture] texture-filter=linear: FPS: 400 FrameTime: 2.500 ms
 glmark2 Score: 350
"""


class GraphicsGateTests(unittest.TestCase):
    def test_equal_or_higher_median_passes(self):
        for scores in ([350, 350, 350], [349, 351, 352]):
            samples = {"base": [gate.parse_glmark(LOG) for _ in range(3)],
                       "candidate": [{**gate.parse_glmark(LOG), "score": n} for n in scores]}
            self.assertGreaterEqual(gate.compare_scores(samples)["candidate"], 350)

    def test_lower_median_fails_even_with_one_fast_run(self):
        samples = {"base": [gate.parse_glmark(LOG) for _ in range(3)],
                   "candidate": [{**gate.parse_glmark(LOG), "score": n} for n in (349, 349, 500)]}
        with self.assertRaisesRegex(RuntimeError, "regression"):
            gate.compare_scores(samples)

    def test_missing_zero_duplicate_score_or_software_renderer_fails(self):
        for log in (LOG.replace(" glmark2 Score: 350\n", ""),
                    LOG.replace("Score: 350", "Score: 0"),
                    LOG + "glmark2 Score: 999\n", LOG.replace("SVGA3D", "llvmpipe"),
                    LOG.replace("[build]", "build").replace("[texture]", "texture")):
            with self.subTest(log=log), self.assertRaises(RuntimeError):
                gate.parse_glmark(log)

    def test_changed_scenes_or_version_fails(self):
        for key, value in (("scenes", ["[build] use-vbo=false"]), ("version", "2024.01")):
            samples = {"base": [gate.parse_glmark(LOG) for _ in range(3)],
                       "candidate": [{**gate.parse_glmark(LOG), key: value} for _ in range(3)]}
            with self.assertRaisesRegex(RuntimeError, key):
                gate.compare_scores(samples)

    def piglit(self, directory, case):
        for renderer in ("svga", "llvmpipe"):
            path = directory / renderer
            path.mkdir(parents=True)
            (path / "results.json").write_text(json.dumps({"tests": case}))

    def test_piglit_new_skip_or_missing_case_fails(self):
        for candidate in ({"a": {"result": "skip"}}, {}):
            with tempfile.TemporaryDirectory() as tmp:
                base, after = Path(tmp) / "base", Path(tmp) / "candidate"
                self.piglit(base, {"a": {"result": "pass"}})
                self.piglit(after, candidate)
                with self.assertRaises(RuntimeError):
                    gate.compare_piglit(base, after)

    def test_piglit_lost_subtest_pass_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            base, after = Path(tmp) / "base", Path(tmp) / "candidate"
            self.piglit(base, {"a": {"result": "pass", "subtests": {"pixel": "pass"}}})
            self.piglit(after, {"a": {"result": "pass", "subtests": {"pixel": "skip"}}})
            with self.assertRaisesRegex(RuntimeError, "subtest"):
                gate.compare_piglit(base, after)

    def test_existing_unsupported_skip_is_allowed(self):
        with tempfile.TemporaryDirectory() as tmp:
            base, after = Path(tmp) / "base", Path(tmp) / "candidate"
            cases = {"supported": {"result": "pass"}, "unsupported": {"result": "skip"}}
            self.piglit(base, cases)
            self.piglit(after, cases)
            gate.compare_piglit(base, after)

    def test_incomplete_sample_set_fails(self):
        for count in (0, 1, 2, 4):
            with self.subTest(count=count), self.assertRaisesRegex(RuntimeError, "three samples"):
                gate.compare_scores({"base": [gate.parse_glmark(LOG) for _ in range(count)],
                                     "candidate": [gate.parse_glmark(LOG) for _ in range(3)]})

    def exercise_gate(self, tmp, *, failure=None, cleanup_failure=False, changed_conditions=False,
                      changed_warmup=False, candidate_score=350, config_override=None):
        root = Path(tmp)
        hook = root / "hook"
        hook.touch()
        calls = []

        def fake_run(argv, log, timeout):
            argv = list(map(str, argv))
            calls.append(argv)
            log.write_text("command evidence\n")
            if argv[0] == "make":
                self.assertEqual(argv[-1], "preload-lab")
                self.assertIn(f"CXXFLAGS={gate.RELEASE_FLAGS}", argv)
                library = Path(next(arg.split("=", 1)[1] for arg in argv if arg.startswith("LIB_DIR=")))
                library.mkdir(parents=True)
                (library / "libqemu_svga3d.so").write_bytes(b"tested library")
            elif argv[0] == str(hook):
                directory = Path(argv[-1])
                if argv[1] == "cleanup":
                    if cleanup_failure:
                        raise subprocess.CalledProcessError(1, argv)
                    return
                if failure == "prepare":
                    raise subprocess.CalledProcessError(1, argv)
                library = Path(argv[argv.index("--library") + 1])
                config = {"ssh": "graphics@guest", "validation": int(argv[argv.index("--validation") + 1]),
                          "loaded_library_sha256": gate.hashlib.sha256(library.read_bytes()).hexdigest(),
                          "conditions": {"image": "fixed", "worker_threads": 1}}
                if changed_conditions and directory.name.startswith("candidate-"):
                    config["conditions"]["worker_threads"] = 2
                config.update(config_override or {})
                (directory / "guest.json").write_text(json.dumps(config))
            elif argv[0] == "ssh":
                if failure == "benchmark":
                    raise subprocess.TimeoutExpired(argv, timeout)
                text = LOG
                if log.parent.name.startswith("candidate-"):
                    text = text.replace("Score: 350", f"Score: {candidate_score}")
                if changed_warmup and log.name == "warmup.log":
                    text = text.replace("2023.01", "2024.01")
                log.write_text(text)
            else:
                if failure == "piglit":
                    raise subprocess.CalledProcessError(1, argv)
                self.piglit(Path(argv[argv.index("--output") + 1]), {"pixels": {"result": "pass"}})

        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()), \
                patch.object(gate, "run", side_effect=fake_run), patch.object(
                gate.subprocess, "check_output", return_value="revision\n"):
            result = gate.main(["--base", str(root / "base"), "--candidate", str(root / "candidate"),
                                "--guest-hook", str(hook), "--output", str(root / "evidence")])
        report = json.loads((root / "evidence/summary.json").read_text())
        return result, report, calls

    def test_complete_gate_uses_fresh_builds_and_cleans_every_stage(self):
        with tempfile.TemporaryDirectory() as tmp:
            result, report, calls = self.exercise_gate(tmp)
            self.assertEqual(result, 0)
            self.assertTrue(report["passed"])
            self.assertEqual(report["medians"], {"base": 350, "candidate": 350})
            self.assertEqual([stage["build"] for stage in report["stages"]],
                             ["base", "candidate", "base", "candidate", "candidate", "base", "base", "candidate"])
            self.assertTrue(all(stage["cleaned_up"] for stage in report["stages"]))
            self.assertEqual(sum(len(call) > 1 and call[1] == "cleanup" for call in calls), 8)
            self.assertEqual(sum(call[0] == "ssh" for call in calls), 12)

    def test_stage_failures_always_cleanup_and_preserve_summary(self):
        for failure in ("prepare", "piglit", "benchmark"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as tmp:
                result, report, calls = self.exercise_gate(tmp, failure=failure)
                self.assertEqual(result, 1)
                self.assertFalse(report["passed"])
                self.assertIn("error", report)
                self.assertTrue(report["stages"][-1]["cleaned_up"])
                self.assertEqual(calls[-1][1], "cleanup")

    def test_cleanup_failure_retains_original_preparation_error(self):
        with tempfile.TemporaryDirectory() as tmp:
            result, report, _ = self.exercise_gate(tmp, failure="prepare", cleanup_failure=True)
            self.assertEqual(result, 1)
            self.assertIn("cleanup failed", report["error"])
            self.assertIn("error", report["stages"][-1])
            self.assertIn("cleanup_error", report["stages"][-1])

    def test_changed_conditions_or_warmup_identity_fail_and_cleanup(self):
        for options in ({"changed_conditions": True}, {"changed_warmup": True}):
            with self.subTest(options=options), tempfile.TemporaryDirectory() as tmp:
                result, report, _ = self.exercise_gate(tmp, **options)
                self.assertEqual(result, 1)
                self.assertTrue(report["stages"][-1]["cleaned_up"])

    def test_invalid_hook_identity_and_options_fail_and_cleanup(self):
        for config in ({"loaded_library_sha256": "wrong"}, {"validation": 0}, {"validation": True},
                       {"ssh": "-bad"}, {"conditions": {}}, {"display": []},
                       {"mesa_driver_path": "relative"}, {"extended_state": "false"},
                       {"extended_state": True}):
            with self.subTest(config=config), tempfile.TemporaryDirectory() as tmp:
                result, report, _ = self.exercise_gate(tmp, config_override=config)
                self.assertEqual(result, 1)
                self.assertTrue(report["stages"][-1]["cleaned_up"])

    def test_full_gate_reports_performance_regression(self):
        with tempfile.TemporaryDirectory() as tmp:
            result, report, _ = self.exercise_gate(tmp, candidate_score=349)
            self.assertEqual(result, 1)
            self.assertIn("regression", report["error"])
            self.assertEqual(len(report["samples"]["candidate"]), 3)

    def test_missing_real_hook_exits_unsuccessfully_with_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            result = subprocess.run([sys.executable, str(Path(gate.__file__)), "--base", tmp,
                                     "--candidate", tmp, "--guest-hook", str(root / "absent"),
                                     "--output", str(root / "evidence")], capture_output=True, text=True)
            self.assertEqual(result.returncode, 1)
            report = json.loads((root / "evidence/summary.json").read_text())
            self.assertFalse(report["passed"])
            self.assertIn("Provision", report["error"])

    def test_timeout_terminates_child_processes_and_keeps_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            pidfile = root / "child.pid"
            # The command writes evidence then starts a descendant that would
            # otherwise remain alive after the command's timeout.
            script = "import subprocess,sys,time; from pathlib import Path; " \
                     "p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)']); " \
                     "Path(sys.argv[1]).write_text(str(p.pid)); print('started',flush=True); time.sleep(60)"
            with redirect_stdout(io.StringIO()), self.assertRaises(subprocess.TimeoutExpired):
                gate.run([sys.executable, "-c", script, pidfile], root / "command.log", 1)
            self.assertIn("started", (root / "command.log").read_text())
            child = Path(f"/proc/{pidfile.read_text()}/stat")
            # Reaping the leader does not guarantee that its separately
            # scheduled descendant has handled the group signal yet.
            deadline = time.monotonic() + 1
            while True:
                try:
                    state = child.read_text().rsplit(")", 1)[1].split()[0]
                except FileNotFoundError:
                    break
                if state == "Z":
                    break
                self.assertLess(time.monotonic(), deadline, "Timed-out descendant remains alive")
                time.sleep(0.01)


if __name__ == "__main__":
    unittest.main()
