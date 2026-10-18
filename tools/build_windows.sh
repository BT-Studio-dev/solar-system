#!/usr/bin/env bash
# Build the Windows application and installer without a Windows machine.
#
#   tools/build_windows.sh [all|app|installer|clean]
#
# Requires a C compiler able to emit i686 COFF objects and link PE images
# (Zig is used here: `pip install ziglang`, then `python -m ziglang cc`).
# The results run on Windows 7 SP1 and later with no runtime dependencies:
# the application links no CRT at all, and setup carries the app inside itself.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CC="${CC_WIN:-python3 -m ziglang cc}"
TARGET=x86-windows-gnu
OUT="$ROOT/build"
DIST="$ROOT/dist"
OBJ_APP="$OUT/obj/app"
OBJ_SETUP="$OUT/obj/setup"
VERSION="$(tr -d '[:space:]' < VERSION)"
DEFINES=(-D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0800)

APP_SOURCES=(
  src/core.c src/theme.c src/sim.c src/content.c src/crt_min.c src/gfx_gdi.c
  src/scene.c src/app.c src/ui.c src/main_win32.c
)
APP_LIBS=(-lgdiplus -lcomdlg32 -lshell32 -lcomctl32 -luser32 -lgdi32 -lkernel32)

SETUP_SOURCES=(src/installer/payload.c src/installer/install.c src/installer/setup_ui.c)
SETUP_LIBS=(-lcomctl32 -lshell32 -lole32 -ladvapi32 -luxtheme -luser32 -lgdi32 -lkernel32)

mkdir -p "$OBJ_APP" "$OBJ_SETUP" "$OUT" "$DIST"

compile() { # compile <source> <object> [extra flags...]
  local src="$1" obj="$2"; shift 2
  echo "  CC  $src"
  $CC -target $TARGET -Os -std=c99 -Wno-macro-redefined -ffunction-sections \
      -fno-asynchronous-unwind-tables "${DEFINES[@]}" -Isrc "$@" -c "$src" -o "$obj"
}

link_exe() { # link_exe <output> <entry> <objects...> -- <libs...>
  local out="$1" entry="$2"; shift 2
  local objs=()
  while [[ $# -gt 0 && "$1" != "--" ]]; do objs+=("$1"); shift; done
  [[ $# -gt 0 ]] && shift
  echo "  LD  $(basename "$out")"
  $CC -target $TARGET -Os -nostdlib -s \
      -Wl,--entry="$entry" -Wl,--subsystem,windows -Wl,--gc-sections \
      "${objs[@]}" "${DEFINES[@]}" -o "$out" "$@"
}

patch_resources() { # patch_resources <input> <output> <manifest> <icon-id> <description>
  python3 tools/pe_patch.py --input "$1" --output "$2" \
      --icon assets/icon.ico --icon-id "$4" --manifest "$3" --version "$VERSION" \
      --product "Solar System Sandbox" --company "Solar Sandbox Project" \
      --description "$5" --subsystem-version 6.1
}

build_app() {
  echo "== application"
  local objs=()
  for src in "${APP_SOURCES[@]}"; do
    local obj="$OBJ_APP/$(basename "${src%.c}").o"
    compile "$src" "$obj"
    objs+=("$obj")
  done
  link_exe "$OUT/SolarSandbox.raw.exe" WinMainCRTStartup "${objs[@]}" -- "${APP_LIBS[@]}"
  patch_resources "$OUT/SolarSandbox.raw.exe" "$OUT/SolarSandbox.exe" \
      tools/app.manifest 101 "Solar System Sandbox Simulator"
  rm -f "$OUT/SolarSandbox.raw.exe"
  if [[ -d content ]]; then
    # the portable build gets the same sample folders as an installed copy
    cp -R content/. "$OUT/"
  fi
}

build_installer() {
  echo "== installer"
  if [[ ! -f "$OUT/SolarSandbox.exe" ]]; then
    echo "  (building the application first)"
    build_app
  fi

  rm -rf "$OUT/payload"
  mkdir -p "$OUT/payload"
  cp "$OUT/SolarSandbox.exe" "$OUT/payload/SolarSandbox.exe"
  cp LICENSE "$OUT/payload/LICENSE.txt"
  cp README.md "$OUT/payload/README.txt"
  cp assets/icon.ico "$OUT/payload/icon.ico"
  if [[ -d content ]]; then
    # Assets, Parts, Texture Pack, Mods/Custom_, Logs and Saving, with samples
    cp -R content/. "$OUT/payload/"
  fi
  if compgen -G "docs/screenshots/*.png" > /dev/null; then
    mkdir -p "$OUT/payload/docs/screenshots"
    cp docs/screenshots/*.png "$OUT/payload/docs/screenshots/"
  fi

  python3 tools/make_payload.py --dir "$OUT/payload" --out "$OUT/payload.bin" \
      --version "$VERSION" --name "Solar System Sandbox" --publisher "Solar Sandbox Project"
  python3 tools/make_coff_payload.py --input "$OUT/payload.bin" --output "$OUT/payload_data.o"

  local objs=("$OBJ_APP/core.o" "$OBJ_APP/theme.o" "$OBJ_APP/crt_min.o")
  for src in "${SETUP_SOURCES[@]}"; do
    local obj="$OBJ_SETUP/$(basename "${src%.c}").o"
    compile "$src" "$obj" -DSSS_INSTALLER
    objs+=("$obj")
  done
  objs+=("$OUT/payload_data.o")

  link_exe "$OUT/SolarSandboxSetup.raw.exe" WinMainCRTStartup "${objs[@]}" -- "${SETUP_LIBS[@]}"
  patch_resources "$OUT/SolarSandboxSetup.raw.exe" "$DIST/SolarSystemSandbox-$VERSION-Setup.exe" \
      tools/installer.manifest 1 "Solar System Sandbox Setup"
  rm -f "$OUT/SolarSandboxSetup.raw.exe"
}

case "${1:-all}" in
  app) build_app ;;
  installer) build_installer ;;
  all) build_app; build_installer ;;
  clean) rm -rf "$OUT" "$DIST" ;;
  *) echo "usage: $0 [all|app|installer|clean]"; exit 1 ;;
esac
echo "done: $(ls -lh "$OUT"/*.exe "$DIST"/*.exe 2>/dev/null | awk '{print $9, $5}' | tr '\n' ' ')"
