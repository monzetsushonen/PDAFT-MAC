#!/bin/zsh
set -eu
resources="${0:A:h}"
lab="${PDAFT_WORK_ROOT:?App must supply its external working directory}"
components="${PDAFT_COMPONENTS_ROOT:?Select runtime components in App}"
source=$(<"$lab/bridge/resources.path")
[[ -d "$source/rom" && -f "$source/diva.exe" ]] || { print -u2 'Selected game resources unavailable'; exit 2; }
actual=$(/usr/bin/shasum -a 256 "$source/diva.exe")
[[ "${actual%% *}" == 8bad363bafa079c5974daba8a4cf2e2edf91f227686cbf99b45d44786d2de5af ]] || { print -u2 'Only verified PDAFT 7.10 executable is supported'; exit 2; }
# Separate overlay per resource directory; a different selection never rewrites saves.
key=$(print -rn -- "$source" | /usr/bin/shasum -a 256);key="${key%% *}"
game="$lab/games/$key"
mkdir -p "$lab/games" "$lab/tmp" "$lab/prefix" "$lab/bridge"
if [[ ! -f "$game/.app-prepared-v1" ]]; then
    target="$game"
    [[ ! -e "$target" && ! -L "$target" ]] || { print -u2 "Incomplete overlay exists; refusing overwrite";exit 2; }
    game="$target.prepare.$$"
    mkdir "$game"
    /bin/cp -R "$components/Bootstrap/." "$game/"
    for name in diva.exe CPUSBSG.dll MUsbPuIf.dll glut32.dll CP9550D_COL.lut; do
        /bin/cp "$source/$name" "$game/$name"
    done
    for name in rom firm; do
        [[ -d "$source/$name" ]] || exit 2
        /bin/ln -s "$source/$name" "$game/$name"
    done
    mkdir -p "$game/mdata" "$game/ram/databank" "$game/ram/osage_play_data"
    for name in M215 M220 M230 M240 M250 M260 M270; do
        [[ -f "$source/mdata/$name/info.txt" ]] || { print -u2 "Missing official update $name";exit 2; }
    done
    : > "$game/enabled-mdata.txt"
    for dir in "$source"/mdata/*(/N); do
        [[ -f "$dir/info.txt" && "${dir:t}" != ._* ]] || continue
        /bin/ln -s "$dir" "$game/mdata/${dir:t}"
        print -r -- "${dir:t}" >> "$game/enabled-mdata.txt"
    done
    # Keep supplied patches with the resource folder; never copy saves/configs.
    if [[ -d "$source/patches" ]]; then
        mkdir -p "$game/patches"
        for patch in "$source"/patches/*.p(N); do
            [[ "${patch:t}" != custom_freeplay_text_example.p ]] || continue
            /bin/cp "$patch" "$game/patches/"
        done
    fi
    print 'all-mdatas-v1' > "$game/.app-prepared-v1"
    /bin/mv "$game" "$target"
    game="$target"
fi
# Apply the selected language to owned overlays only; source assets stay read-only.
mode=en
[[ ! -f "$lab/bridge/language.txt" ]] || mode=$(<"$lab/bridge/language.txt")
case "$mode" in en|ja|zh-CN) ;; *) print -u2 'Unsupported language';exit 2;; esac
font_args=()
if [[ -f "$lab/bridge/font.path" ]]; then
    font=$(<"$lab/bridge/font.path")
    [[ -f "$font" && "$font" == "$lab/bridge/fonts/"* ]] || { print -u2 'Imported font unavailable; use Restore Source Han Sans in App';exit 2; }
    font_args=("$font")
fi
if [[ -f "$lab/bridge/launch-settings.json" ]]; then
    export PDAFT_LAUNCH_SETTINGS="$lab/bridge/launch-settings.json"
fi
if [[ -f "$lab/bridge/lyrics.zh-CN.json" ]]; then
    export PDAFT_LYRICS_FILE="$lab/bridge/lyrics.zh-CN.json"
fi
if [[ -f "$components/Localization/song-titles.json" ]]; then
    "$resources/PDAFTLocalizer" "$source" "$game" "$components/Localization" "$mode" "${font_args[@]}"
else
    [[ "$mode" != zh-CN ]] || { print -u2 'Chinese patch is not installed in selected components/Localization; choose English or Japanese';exit 2; }
    "$resources/PDAFTLocalizer" --profile-only "$game/plugins/config.ini"
fi
"$resources/prepare-video-runtime.sh" "$game"
# UI skins replace the same MDATA files and are mutually exclusive.
skin="$source/mdata/MZKN"
if [[ -d "$skin" ]]; then
if [[ -f "$lab/bridge/skin.path" ]]; then
    skin=$(<"$lab/bridge/skin.path")
    [[ -f "$skin/info.txt" ]] || { print -u2 'Selected skin unavailable';exit 2; }
    # A skin is an overlay, not a complete MZKN replacement. Retain the base
    # mdata_spr_db.bin so the game's database loader can register its sprites.
    skin_key=$(print -rn -- "$skin" | /usr/bin/shasum -a 256);skin_key="${skin_key%% *}"
    merged="$game/skin-overlays/$skin_key"
    if [[ ! -d "$merged" ]]; then
        mkdir -p "$merged"
        /bin/cp -R "$source/mdata/MZKN/." "$merged/"
        /bin/cp -R "$skin/." "$merged/"
    fi
    skin="$merged"
fi
[[ ! -e "$game/mdata/MZKN" || -L "$game/mdata/MZKN" ]] || exit 2
if [[ -L "$game/mdata/MZKN" ]];then /bin/rm "$game/mdata/MZKN";fi
/bin/ln -s "$skin" "$game/mdata/MZKN"
fi
export DYLD_FRAMEWORK_PATH="$components/Wine/lib/external"
export WINEPREFIX="$lab/prefix" WINEARCH=win64 WINEESYNC=1 TMPDIR="$lab/tmp/"
export WINEDEBUG=-all WINEDLLOVERRIDES='mscoree,mshtml,winegstreamer='
# Create prefix and install the bundled Microsoft runtime once. No network installer.
if [[ ! -f "$lab/prefix/.app-vc-ready-v1" ]]; then
    "$components/Wine/bin/wine64" wineboot.exe -u
    "$components/Wine/bin/wineserver" -w
    "$components/Wine/bin/wine64" "$components/vc_redist.x64.exe" /install /quiet /norestart
    "$components/Wine/bin/wineserver" -w
    touch "$lab/prefix/.app-vc-ready-v1"
fi
# Only this app-owned prefix path may be linked/relinked.
link="$lab/prefix/drive_c/PDAFT"
if [[ -L "$link" ]]; then /bin/rm "$link"; fi
[[ ! -e "$link" ]] || { print -u2 'Owned prefix has a non-link PDAFT directory; refusing to overwrite it';exit 2; }
/bin/ln -s "$game" "$link"
print -r -- "$game" > "$lab/bridge/active-game.path"
