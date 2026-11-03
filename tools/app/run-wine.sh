#!/bin/zsh
set -eu
lab="${PDAFT_WORK_ROOT:?App must supply external working directory}"
tools_root="${0:A:h}"
wine_root="${PDAFT_WINE_ROOT:-$lab/runtime/Game Porting Toolkit.app/Contents/Resources/wine}"
export WINEPREFIX="$lab/prefix"
export WINEARCH=win64
export TMPDIR="$lab/tmp/"
export WINEDEBUG="${PDAFT_WINEDEBUG:--all,err+all}"
export WINEDLLOVERRIDES="${PDAFT_DLL_OVERRIDES:-dinput8=n,b;dnsapi=n,b;mscoree,mshtml=}"
export WINEESYNC=1
profile="${PDAFT_SANDBOX_PROFILE:-$tools_root/boot-sandbox.sb}"
cd "${PDAFT_GAME_ROOT:-$lab/game-p0-clean}"
if [[ -n "${PDAFT_NATIVE_PROFILER:-}" ]]; then
    # Set DYLD after Apple's protected sandbox launcher, inside the sandbox.
    exec /usr/bin/sandbox-exec -D "PDAFT_WORK_ROOT=$lab" -f "$profile" /bin/zsh -c \
        'export DYLD_INSERT_LIBRARIES="$PDAFT_NATIVE_PROFILER"; export DYLD_FRAMEWORK_PATH="$PDAFT_WINE_ROOT/lib/external"; exec "$@"' pdaft \
        "${PDAFT_WINE_LOADER:-$wine_root/bin/wine64}" "$@"
fi
exec /usr/bin/sandbox-exec -D "PDAFT_WORK_ROOT=$lab" -f "$profile" "${PDAFT_WINE_LOADER:-$wine_root/bin/wine64}" "$@"
