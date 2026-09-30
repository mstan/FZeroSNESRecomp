"""Generate a private native BS Deluxe module and import its indivisible data.

Run after stock regeneration. SNESRECOMP_NATIVE_ANALYZER may point at a pinned
prebuilt analyzer. Output stays under ignored captures/ and build directories.
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--archive", type=Path, required=True)
p.add_argument("--stock", type=Path, default=ROOT / "fzero.sfc")
p.add_argument("--work", type=Path, default=ROOT / "captures/bs-deluxe")
# The build embeds this file, so it belongs beside the generated sources
# where FZERO_DELUXE_DATA_FILE looks for it by default.
p.add_argument("--out", type=Path, default=None)
a = p.parse_args()
if a.out is None:
    a.out = a.work / "mods"
a.work.mkdir(parents=True, exist_ok=True)

def run(*args):
    subprocess.run([sys.executable, *map(str, args)], cwd=ROOT, check=True)

# Inspector refuses oracle overwrites. A prior oracle must match the importer
# target before it can be used as a generation reference.
from import_bs_deluxe import DELUXE_SHA256, DELUXE_VERSION
from inspect_bs_deluxe import sha
oracle = a.work / "oracle.sfc"
extra = [] if oracle.exists() else ["--oracle", oracle]
run(ROOT / "tools/inspect_bs_deluxe.py", "--archive", a.archive,
    "--stock", a.stock, "--out", a.work / "audit.json", *extra)
if sha(oracle.read_bytes()) != DELUXE_SHA256:
    raise ValueError(f"Existing private oracle does not match pinned USA Deluxe {DELUXE_VERSION}; "
                     f"delete {oracle} to regenerate from the current archive")
cfg = a.work / "cfg"
cfg.mkdir(exist_ok=True)
for source in (ROOT / "recomp").glob("*.cfg"):
    shutil.copy2(source, cfg / source.name)
shutil.copy2(ROOT / "recomp/funcs.h", cfg / "funcs.h")
run(ROOT / "snesrecomp/tools/v2_emit.py", "--rom", oracle, "--cfg-dir", cfg,
    "--out-dir", a.work / "gen", "--cfg-roots", "--no-host-root-scan",
    "--analysis-backend", "native", "--module-id", "bs-deluxe", "--module-prefix", "deluxe")
run(ROOT / "tools/import_bs_deluxe.py", "--archive", a.archive,
    "--stock", a.stock, "--out", a.out, "--gen", a.work / "gen")
