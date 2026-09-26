# Shared snesrecomp pack transport

F-Zero Forever and the experimental SMW `feat/shared-data-packs` branch now pin
the same snesrecomp `feat/shared-content-packs` revision. The engine owns the
folder/ZIP catalog, envelope validation, bounded resource reads, optional disk
materialization and manifest writer. F-Zero retains course/FZEdit decoding,
records, cup ordering, vehicles, title resources and music/mechanics semantics.

The new envelope wraps the existing course index in `courses.json`. Re-export
local packs with `tools/export_runtime_packs.py`; do not install old and new
copies together. Music still uses `music/<course source stem>.pcm`. Existing
course records survive because decoded course hashes are unchanged.
See [PACK_FORMAT.md](PACK_FORMAT.md) for authoring details.

Windows validation, 2026-09-25:

- Both desktop and headless targets built; all 13 CTest cases passed.
- All 75 course/record hashes match the prior extraction. Folder and ZIP
  catalogs are identical; completed caches are reused; invalid packs rejected.
- CGP, Astra, MAX and Bower each started a race through the new loader.
- Raw FZEdit fixture decoded identically in the old loader, shared folder
  loader and wrapped ZIP loader.
- Five synthetic mixer checks passed: installed numbered music, Astra and
  Bower filename music, missing-song fallback, and Practice; six music-import
  unit tests passed.
- Astra completed-cup records, detail navigation, vehicle isolation and battery
  reload passed on both retail and expanded-car engines.

Evidence stays local under `captures/shared-*`. Original source packs, previous
builds and tester ZIPs were preserved. No new release is published by this
framework migration. Central tracking: `beads-8wg.2.78`.
