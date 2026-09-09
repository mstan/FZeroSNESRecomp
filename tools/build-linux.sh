#!/usr/bin/env bash
# Build and package FZeroSNESRecomp as a Linux x86_64 AppImage.
set -euo pipefail

APP_NAME="FZeroSNESRecomp"
CMAKE_TARGET="FZeroSNESRecomp"
RELEASE_SLUG="FZeroSNESRecomp"
ROM_EXTS="sfc smc"

LINUXDEPLOY_URL=https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
LINUXDEPLOY_SHA=36a2d7e274d12e1050d0e9ecfe11d339ed54720b2bec464c286d53f8b07f5c62
APPIMAGETOOL_URL=https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage
APPIMAGETOOL_SHA=a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0

REPO="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '\r\n' < "$REPO/VERSION")"
BUILD="$REPO/build-linux-prod"
OUT="$REPO/release-linux"
JOBS="$(nproc 2>/dev/null || echo 4)"
DO_RUN=0
DO_PACKAGE=1
INCLUDE_BS=1
BS_GEN="$REPO/captures/bs-deluxe/gen"
BS_MODS="$REPO/build-release/mods"

while [ $# -gt 0 ]; do
  case "$1" in
    --version) VERSION="$2"; shift 2;;
    --build) BUILD="$2"; shift 2;;
    --out) OUT="$2"; shift 2;;
    --jobs) JOBS="$2"; shift 2;;
    --run) DO_RUN=1; shift;;
    --no-package) DO_PACKAGE=0; shift;;
    --include-bs-deluxe) INCLUDE_BS=1; shift;;
    --bs-gen) BS_GEN="$2"; shift 2;;
    --bs-mods) BS_MODS="$2"; shift 2;;
    -h|--help)
      sed -n '1,70p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

FLAGS=(
  -DCMAKE_BUILD_TYPE=Release
  -DSNESRECOMP_BUILD_VERSION="$VERSION"
  -DSNESRECOMP_SDL_BACKEND="${SNESRECOMP_SDL_BACKEND:-SDL3}"
)

SDL_BACKEND="${SNESRECOMP_SDL_BACKEND:-SDL3}"
SDL_CFG_DIR="$( { find /usr/lib /usr/lib64 /usr/local/lib -type d -path "*cmake/$SDL_BACKEND" 2>/dev/null || true; } | head -1 )"
[ -n "$SDL_CFG_DIR" ] && FLAGS+=( "-D${SDL_BACKEND}_DIR=$SDL_CFG_DIR" )

if [ "$INCLUDE_BS" = "1" ]; then
  [ -f "$BS_GEN/deluxe_namespace.h" ] || { echo "missing BS native gen dir: $BS_GEN" >&2; exit 1; }
  [ -f "$BS_MODS/bs-deluxe.dat" ] || { echo "missing BS Deluxe payload: $BS_MODS/bs-deluxe.dat" >&2; exit 1; }
  [ -f "$REPO/patches/bs-deluxe-usa.ips" ] || { echo "missing tracked BS patch: patches/bs-deluxe-usa.ips" >&2; exit 1; }
  FLAGS+=( -DFZERO_DELUXE_GEN_DIR="$BS_GEN" -DFZERO_DELUXE_MODS_DIR="$BS_MODS" )
fi

[ -f "$REPO/snesrecomp/runner/runner.cmake" ] || {
  echo "snesrecomp submodule missing; run git submodule update --init --recursive" >&2
  exit 1
}
[ -f "$REPO/recomp-ui/recomp_ui.cmake" ] || {
  echo "recomp-ui submodule missing; run git submodule update --init --recursive" >&2
  exit 1
}

echo "[1/4] configure"
cmake -S "$REPO" -B "$BUILD" -G "Unix Makefiles" "${FLAGS[@]}"

echo "[2/4] build"
cmake --build "$BUILD" --target "$CMAKE_TARGET" -j"$JOBS"

BIN=""
while IFS= read -r file; do
  if [ "$(basename "$file")" = "$CMAKE_TARGET" ] && file -b "$file" | grep -q "ELF.*executable"; then
    BIN="$file"
    break
  fi
done < <(find "$BUILD" -maxdepth 3 -type f)
[ -n "$BIN" ] || { echo "no Linux ELF named $CMAKE_TARGET found under $BUILD" >&2; exit 1; }

if [ "$DO_PACKAGE" = "0" ]; then
  echo "$BIN"
  exit 0
fi

echo "[3/4] package AppImage"
mkdir -p "$OUT"
WORK="$(mktemp -d)"
cleanup() { rm -rf "$WORK"; }
trap cleanup EXIT

APPDIR="$WORK/AppDir"
mkdir -p "$APPDIR"
TOOLS_DIR="$BUILD/appimage-tools"
mkdir -p "$TOOLS_DIR"

fetch_tool() {
  local url="$1" sha="$2" dest="$3"
  if [ ! -f "$dest" ] || [ "$(sha256sum "$dest" | awk '{print $1}')" != "$sha" ]; then
    curl -fL --retry 3 "$url" -o "$dest.tmp"
    printf '%s  %s\n' "$sha" "$dest.tmp" | sha256sum -c - >/dev/null
    mv "$dest.tmp" "$dest"
  fi
  chmod 0755 "$dest"
}

LINUXDEPLOY_BIN="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
APPIMAGETOOL_BIN="$TOOLS_DIR/appimagetool-x86_64.AppImage"
fetch_tool "$LINUXDEPLOY_URL" "$LINUXDEPLOY_SHA" "$LINUXDEPLOY_BIN"
fetch_tool "$APPIMAGETOOL_URL" "$APPIMAGETOOL_SHA" "$APPIMAGETOOL_BIN"
LINUXDEPLOY="$LINUXDEPLOY_BIN --appimage-extract-and-run"
APPIMAGETOOL="$APPIMAGETOOL_BIN --appimage-extract-and-run"

ICON="$WORK/fzerosnesrecomp.png"
if command -v convert >/dev/null 2>&1 && [ -f "$REPO/assets/img/boxart.tga" ]; then
  convert "$REPO/assets/img/boxart.tga" -resize 240x240 -background transparent -gravity center -extent 256x256 "$ICON"
else
  python3 - "$ICON" <<'PY'
import struct, sys, zlib
N = 256
raw = (bytes([0]) + bytes([16, 24, 28]) * N) * N
def chunk(tag, data):
    body = tag + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)
open(sys.argv[1], "wb").write(
    b"\x89PNG\r\n\x1a\n" +
    chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 2, 0, 0, 0)) +
    chunk(b"IDAT", zlib.compress(raw, 9)) +
    chunk(b"IEND", b""))
PY
fi

DESKTOP="$WORK/fzerosnesrecomp.desktop"
cat > "$DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=FZeroSNESRecomp
Exec=FZeroSNESRecomp
Icon=fzerosnesrecomp
Categories=Game;
Terminal=false
EOF

$LINUXDEPLOY --appdir "$APPDIR" --executable "$BIN" --desktop-file "$DESKTOP" --icon-file "$ICON"

[ -d "$(dirname "$BIN")/assets" ] || { echo "launcher assets missing beside $BIN" >&2; exit 1; }
cp -r "$(dirname "$BIN")/assets" "$APPDIR/usr/bin/assets"

if [ "$INCLUDE_BS" = "1" ]; then
  mkdir -p "$APPDIR/usr/bin/mods" "$APPDIR/usr/bin/patches"
  cp "$BS_MODS/bs-deluxe.dat" "$APPDIR/usr/bin/mods/bs-deluxe.dat"
  cp "$BS_MODS/bs-deluxe-import.json" "$APPDIR/usr/bin/mods/bs-deluxe-import.json"
  cp "$BS_MODS/BS-Deluxe-credits.txt" "$APPDIR/usr/bin/mods/BS-Deluxe-credits.txt"
  cp "$REPO/patches/bs-deluxe-usa.ips" "$APPDIR/usr/bin/patches/bs-deluxe-usa.ips"
fi

rm -f "$APPDIR/AppRun"
cat > "$APPDIR/AppRun" <<EOF
#!/bin/sh
HERE="\$(dirname "\$(readlink -f "\$0")")"
export LD_LIBRARY_PATH="\$HERE/usr/lib:\${LD_LIBRARY_PATH:-}"
export SDL_JOYSTICK_HIDAPI_STEAM=1
export SDL_GAMECONTROLLER_ALLOW_STEAM_VIRTUAL_GAMEPAD=1
SELF="\${APPIMAGE:-\$0}"
ROMDIR="\$(dirname "\$(readlink -f "\$SELF")")"
export FZERO_VIDEO_CONFIG="\$ROMDIR/fzero-video.ini"

if [ -d "\$HERE/usr/bin/patches" ] && [ -w "\$ROMDIR" ]; then
  mkdir -p "\$ROMDIR/patches" 2>/dev/null || true
  cp -a "\$HERE/usr/bin/patches/." "\$ROMDIR/patches/" 2>/dev/null || true
fi

ROM=""
for ext in $ROM_EXTS; do
  for file in "\$ROMDIR"/*."\$ext"; do [ -e "\$file" ] && ROM="\$file" && break 2; done
done
if [ -n "\$ROM" ]; then
  cached=""
  [ -f "\$ROMDIR/rom.cfg" ] && cached="\$(head -n1 "\$ROMDIR/rom.cfg" 2>/dev/null | tr -d '\\r\\n')"
  if [ -z "\$cached" ] || [ ! -f "\$cached" ]; then
    [ -w "\$ROMDIR" ] && printf '%s\\n' "\$ROM" > "\$ROMDIR/rom.cfg" 2>/dev/null || true
  fi
fi

cd "\$ROMDIR" 2>/dev/null || true
exec "\$HERE/usr/bin/FZeroSNESRecomp" "\$@"
EOF
chmod +x "$APPDIR/AppRun"

APP="$OUT/$RELEASE_SLUG-linux-$VERSION-x86_64.AppImage"
rm -f "$APP"
ARCH=x86_64 $APPIMAGETOOL "$APPDIR" "$APP"
chmod +x "$APP"

echo "[4/4] layout test"
bash "$REPO/tools/test_appimage_layout.sh" "$APPDIR"
sha256sum "$APP"

if [ "$DO_RUN" = "1" ]; then
  "$APP" || true
fi
