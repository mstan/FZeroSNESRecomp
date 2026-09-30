#!/usr/bin/env bash
# Regenerate src/gen/*.c from a locally staged, verified F-Zero (USA) ROM.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

PYTHON="${PYTHON:-$(command -v python3 || command -v python || true)}"
SNESRECOMP_ROOT="${SNESRECOMP_ROOT:-./snesrecomp}"
EXPECTED_SHA256="bf16c3c867c58e2ab061c70de9295b6930d63f29f81cc986f5ecae03e0ad18d2"

if [ -z "$PYTHON" ]; then
  echo "regen.sh: no python3/python interpreter found on PATH" >&2
  exit 1
fi
if [ ! -f "$SNESRECOMP_ROOT/tools/v2_emit.py" ]; then
  echo "regen.sh: SNESRECOMP_ROOT is not a snesrecomp checkout: $SNESRECOMP_ROOT" >&2
  echo "          run 'bash tools/bootstrap.sh' to initialize the submodule" >&2
  exit 1
fi
if [ ! -f fzero.sfc ]; then
  echo "regen.sh: stage a verified F-Zero (USA) ROM as fzero.sfc first." >&2
  exit 1
fi

ACTUAL_SHA256="$("$PYTHON" - <<'PY'
from pathlib import Path
import hashlib
print(hashlib.sha256(Path("fzero.sfc").read_bytes()).hexdigest())
PY
)"
if [ "$ACTUAL_SHA256" != "$EXPECTED_SHA256" ]; then
  echo "regen.sh: fzero.sfc SHA-256 mismatch" >&2
  echo "  expected: $EXPECTED_SHA256" >&2
  echo "  actual:   $ACTUAL_SHA256" >&2
  exit 1
fi

ANALYSIS_BACKEND="${SNESRECOMP_ANALYSIS_BACKEND:-native}"
case "$ANALYSIS_BACKEND" in
  native|python|auto) ;;
  *) echo "regen.sh: invalid SNESRECOMP_ANALYSIS_BACKEND: $ANALYSIS_BACKEND" >&2; exit 2 ;;
esac

if [ "$ANALYSIS_BACKEND" = native ]; then
  "$PYTHON" "$SNESRECOMP_ROOT/tools/build_native_analyzer.py"
fi

# Clean interpreter discoveries harvested from a real run become optional AOT
# roots on the next regeneration. They never replace the interpreter fallback,
# and v2_emit rejects any tuple that is not safe to materialize.
PROFILE_MANIFEST="recomp/tier2_coverage.json"
emit_extra=()
if [ -f "$PROFILE_MANIFEST" ]; then
  emit_extra+=(--profile-manifest "$PROFILE_MANIFEST"
               --legacy-profile-rom-sha256 "$EXPECTED_SHA256")
  echo "regen.sh: using AOT coverage profile $PROFILE_MANIFEST"
else
  echo "regen.sh: no AOT coverage profile yet; run the headless host to harvest one"
fi

"$PYTHON" "$SNESRECOMP_ROOT/tools/v2_emit.py" --rom fzero.sfc \
  --cfg-dir recomp --out-dir src/gen --cfg-roots --no-host-root-scan \
  --analysis-backend "$ANALYSIS_BACKEND" \
  "${emit_extra[@]+"${emit_extra[@]}"}"
"$PYTHON" "$SNESRECOMP_ROOT/tools/v2_sync_funcs_h.py" \
  --cfg-dir recomp --out recomp/funcs.h
