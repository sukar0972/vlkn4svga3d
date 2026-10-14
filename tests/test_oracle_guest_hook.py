"""Check guest ownership and evidence preservation during hook cleanup."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts/graphics"))
import oracle_guest_hook as hook


class OracleHookTests(unittest.TestCase):
    def fixtures(self, directory):
        root = Path(directory) / "fixtures"
        root.mkdir()
        names = {f"{kind}/{name}" for kind in ("bin", "wrappers") for name in hook.FIXTURE_TESTS}
        names.update(("patches/piglit-20220119-depth-stencil-error.patch",
                      "patches/piglit-20220119-fbo-blending-init.patch",
                      "lib/libpiglitutil.so.0", "lib/libpiglitutil_gl.so.0"))
        for name in names:
            path = root / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"reviewed fixture")
        manifest = {"source_sha256": "a" * 64,
                    "files": {name: hook.sha256(root / name) for name in names}}
        (root / "manifest.json").write_text(json.dumps(manifest))
        return root, manifest

    def test_fixture_manifest_accepts_complete_verified_package(self):
        with tempfile.TemporaryDirectory() as directory:
            root, manifest = self.fixtures(directory)
            self.assertEqual(hook.fixture_manifest(root), manifest)

    def test_corrupt_fixture_is_rejected_before_transfer(self):
        with tempfile.TemporaryDirectory() as directory:
            root, _ = self.fixtures(directory)
            (root / "bin/fbo-blending-format-quirks").write_bytes(b"corrupt")
            with patch.object(hook, "command") as command, \
                    self.assertRaisesRegex(ValueError, "digest"):
                hook.install_fixtures({"piglit_fixtures": str(root)}, Path(directory))
            command.assert_not_called()

    def test_fixture_manifest_rejects_missing_unlisted_and_escaping_files(self):
        for change in ("missing", "unlisted", "escape", "directory_link"):
            with self.subTest(change=change), tempfile.TemporaryDirectory() as directory:
                root, manifest = self.fixtures(directory)
                if change == "missing":
                    del manifest["files"]["wrappers/fbo-blending-format-quirks"]
                elif change == "unlisted":
                    (root / "unexpected").write_bytes(b"unreviewed")
                elif change == "directory_link":
                    outside = Path(directory) / "outside"
                    outside.mkdir()
                    (root / "escape").symlink_to(outside, target_is_directory=True)
                else:
                    outside = Path(directory) / "outside"
                    outside.write_bytes(b"outside package")
                    (root / "escape").symlink_to(outside)
                    manifest["files"]["escape"] = hook.sha256(outside)
                (root / "manifest.json").write_text(json.dumps(manifest))
                with self.assertRaises(ValueError):
                    hook.fixture_manifest(root)

    def test_fixture_evidence_retains_manifest_and_original_binary_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root, manifest = self.fixtures(directory)
            original = {"package_version": "0~git20220119-124bca3c9-1",
                        "original_sha256": {name: "b" * 64 for name in hook.FIXTURE_TESTS}}
            with patch.object(hook, "command"), \
                    patch.object(hook, "guest_command", return_value=json.dumps(original)):
                result = hook.install_fixtures({"piglit_fixtures": str(root),
                                               "ssh_user": "operator", "ssh": "guest"}, Path(directory))
            self.assertEqual(result["manifest"], manifest)
            self.assertEqual(result["original_sha256"], original["original_sha256"])
            self.assertEqual(json.loads((Path(directory) / "piglit-fixtures.json").read_text()), result)

    def stage(self, directory, process=None):
        root = Path(directory) / "runtime"
        work = root / "run/guest"
        work.mkdir(parents=True)
        output = Path(directory) / "evidence"
        output.mkdir()
        (work / "owner.json").write_text(json.dumps({"output": str(output), "process": process}))
        (work / "disk.qcow2").write_bytes(b"disposable overlay")
        (work / "serial.log").write_text("guest boot evidence\n")
        (root / "run/libqemu_svga3d.so").write_bytes(b"candidate")
        (root / "tmp").mkdir()
        return {"runtime": str(root)}, work, output

    def test_partial_prepare_cleanup_preserves_logs_and_is_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            config, work, output = self.stage(directory)
            hook.cleanup(config, output)
            self.assertFalse(work.exists())
            self.assertFalse((Path(config["runtime"]) / "run/libqemu_svga3d.so").exists())
            self.assertIn("boot evidence", (output / "serial.log").read_text())
            hook.cleanup(config, output)

    def test_cleanup_refuses_a_different_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            config, work, output = self.stage(directory)
            with self.assertRaisesRegex(RuntimeError, "another stage"), patch.object(hook.os, "kill") as kill:
                hook.cleanup(config, output / "other")
            kill.assert_not_called()
            self.assertTrue((work / "disk.qcow2").exists())

    def test_cleanup_does_not_signal_reused_pid(self):
        with tempfile.TemporaryDirectory() as directory:
            config, work, output = self.stage(directory, {"pid": 123, "start": "old"})
            with patch.object(hook, "process_identity", return_value={"pid": 123, "start": "new"}), \
                    patch.object(hook.os, "kill") as kill:
                hook.cleanup(config, output)
            kill.assert_not_called()
            self.assertFalse(work.exists())

    def test_live_owned_guest_is_stopped_before_removing_overlay(self):
        with tempfile.TemporaryDirectory() as directory:
            identity = {"pid": 123, "start": "owned"}
            config, work, output = self.stage(directory, identity)

            def stop(pid, sig):
                self.assertEqual(pid, 123)
                self.assertEqual(sig, hook.signal.SIGTERM)
                self.assertTrue((work / "disk.qcow2").exists())

            with patch.object(hook, "process_identity", side_effect=[identity, None, None, None]), \
                    patch.object(hook.os, "kill", side_effect=stop) as kill:
                hook.cleanup(config, output)
            kill.assert_called_once()
            self.assertFalse(work.exists())

    def test_validation_failure_keeps_evidence_after_stopping_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            config, work, output = self.stage(directory)
            (Path(config["runtime"]) / "tmp/svga3d.log").write_text("[Vulkan Validation ERROR] bad draw\n")
            with self.assertRaisesRegex(RuntimeError, "validation"):
                hook.cleanup(config, output)
            self.assertFalse(work.exists())
            self.assertIn("bad draw", (output / "adapter.log").read_text())

    def test_real_process_identity_includes_start_time(self):
        identity = hook.process_identity(os.getpid())
        self.assertEqual(identity["pid"], os.getpid())
        self.assertTrue(identity["start"].isdigit())


if __name__ == "__main__":
    unittest.main()
