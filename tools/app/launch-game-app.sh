#!/bin/zsh
set -eu
resources="${0:A:h}"
lab="${PDAFT_WORK_ROOT:?App must supply its external working directory}"
export PDAFT_TOOLS_ROOT="$resources"
export PDAFT_COMPONENTS_ROOT=$(<"$lab/bridge/components.path")
export PDAFT_WINE_ROOT="$PDAFT_COMPONENTS_ROOT/Wine"
export PDAFT_NATIVE_FRAME_PACING=1
export PDAFT_NATIVE_PRESENT_ONLY=1
# Source resources stay read-only even during prefix initialization.
/usr/bin/sandbox-exec -D "PDAFT_WORK_ROOT=$lab" -f "$resources/boot-sandbox.sb" /bin/zsh "$resources/prepare-app-runtime.sh"
export PDAFT_GAME_ROOT=$(<"$lab/bridge/active-game.path")
cache="$lab/prefix/drive_c/users/crossover/AppData/Local/PDAFT/ft_shader_cache.farc"
if [[ ! -f "$lab/bridge/glsl-compat-v1.ready" ]]; then
    if [[ -f "$cache" ]]; then
        /bin/mv -n "$cache" "$cache.before-glsl-compat-v1.$(/bin/date +%s).backup"
        [[ ! -f "$cache" ]] || exit 3
    fi
    print 'ALPHA_TEST_DEF guarded default v1' > "$lab/bridge/glsl-compat-v1.ready"
fi
# Coverage output adds a sprite shader attachment; invalidate only this owned cache once.
if [[ "${PDAFT_HDR_MV_ONLY:-0}" == 1 && ! -f "$lab/bridge/hdr-ui-layers-v4.ready" ]]; then
    if [[ -f "$cache" ]]; then
        /bin/mv -n "$cache" "$cache.before-hdr-ui-layers-v4.$(/bin/date +%s).backup"
        [[ ! -f "$cache" ]] || exit 3
    fi
    print 'DivaGL sprite sprite/font only scene RGB / UI alpha MRT v4' > "$lab/bridge/hdr-ui-layers-v4.ready"
fi
export PDAFT_NATIVE_PROFILER="$resources/libPDAFTFrameProducer.dylib"
export PDAFT_FRAME_CONTROL="$lab/bridge/control.bin"
export PDAFT_BRIDGE_LOG="$lab/bridge/producer.log"
export PDAFT_SANDBOX_PROFILE="$resources/boot-sandbox.sb"
export PDAFT_WINE_ROOT="$PDAFT_COMPONENTS_ROOT/Wine"
ulimit -n 8192 2>/dev/null || true
exec /bin/zsh "$resources/boot-reference.sh" > "$lab/bridge/game.log" 2>&1
