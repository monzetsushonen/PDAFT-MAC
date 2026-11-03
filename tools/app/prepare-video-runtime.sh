#!/bin/zsh
set -eu
resources="${0:A:h}"
lab="${PDAFT_WORK_ROOT:?App must supply working directory}"
game="${1:?App must supply owned game overlay}"
[[ "$game" == "$lab/games/"* ]] || exit 2
plugins="$lab/bridge/gst-plugins-v1"
runtime="$PDAFT_COMPONENTS_ROOT/Wine/lib/GStreamer.framework/Versions/1.0/lib/gstreamer-1.0"
mkdir -p "$plugins"
for name in coreelements app playback typefindfunctions isomp4 videoparsersbad libav videoconvertscale audioconvert audioresample audioparsers asf debug deinterlace videobox videocrop videofilter videorate volume audiobuffersplit; do
    source="$runtime/libgst$name.dylib"
    [[ -f "$source" ]] || { print -u2 "Missing bundled video plugin: $name";exit 2; }
    target="$plugins/libgst$name.dylib"
    [[ ! -e "$target" || -L "$target" ]] || { print -u2 'Refusing to replace non-link video plugin';exit 2; }
    [[ ! -L "$target" ]] || /bin/rm "$target"
    /bin/ln -s "$source" "$target"
done
# Upgrade existing app-owned profiles without touching other preferences.
config="$game/plugins/config.ini"
/usr/bin/sed -E -e 's/^No_Movies[[:space:]]*=.*/No_Movies = 0/' -e 's/^DivaMovie\.dva[[:space:]]*=.*/DivaMovie.dva = 0/' "$config" > "$config.video-new"
/bin/mv "$config.video-new" "$config"
config="$game/plugins/DivaGL.ini"
/usr/bin/sed -E 's/^task_movie_player_no_interop[[:space:]]*=.*/task_movie_player_no_interop=0/' "$config" > "$config.video-new"
/bin/mv "$config.video-new" "$config"
