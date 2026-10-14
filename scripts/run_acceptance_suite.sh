#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p "$ROOT_DIR/artifacts"
RUN_DIR=$(mktemp -d "$ROOT_DIR/artifacts/acceptance-$(date -u +%Y%m%dT%H%M%SZ)-XXXXXX")
export HARNESS_RESULTS_DIR="$RUN_DIR/harness"
export HARNESS_ICD="${HARNESS_ICD:-$(find /usr/share/vulkan/icd.d -name 'lvp_icd*.json' -print -quit)}"
export SPIRV_TOOLS_DIR="${SPIRV_TOOLS_DIR:-$(dirname "$(command -v spirv-val || echo /missing/spirv-val)")}"
test -x "$SPIRV_TOOLS_DIR/spirv-val" || { echo 'Acceptance requires spirv-val; set SPIRV_TOOLS_DIR.' >&2; exit 1; }
python3 - "$RUN_DIR" <<'PY'
import hashlib, json, pathlib, platform, subprocess, sys
root = pathlib.Path.cwd()
files = [root / 'Makefile']
for folder in ('src', 'include', 'tests', 'scripts', 'integrations', 'examples'):
    files += sorted(p for p in (root / folder).rglob('*') if p.is_file() and p.suffix in ('.cpp', '.c', '.h', '.py', '.sh', '.patch'))
manifest = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
result = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
          'platform': platform.platform(), 'files': manifest,
          'source_sha256': hashlib.sha256(json.dumps(manifest, sort_keys=True).encode()).hexdigest()}
pathlib.Path(sys.argv[1], 'source.json').write_text(json.dumps(result, indent=2) + '\n')
PY
printf 'Evidence: %s\n' "$RUN_DIR"
make -j"${BUILD_JOBS:-4}" all test-install > "$RUN_DIR/build.log" 2>&1
make test > "$RUN_DIR/mock.log" 2>&1
./scripts/harness_loop.sh --skip-build > "$RUN_DIR/harness.log" 2>&1
cp artifacts/vulkan_driver_info.log artifacts/vulkan_validation.log "$RUN_DIR/"
cat "$HARNESS_RESULTS_DIR/iter-01/results.tsv"
printf 'Host acceptance passed. Driver identity: %s/vulkan_driver_info.log\n' "$RUN_DIR"
printf 'Source manifest and per-suite logs: %s\n' "$RUN_DIR"
printf 'Guest boot, Piglit and glmark2 require separate VM evidence.\n'
