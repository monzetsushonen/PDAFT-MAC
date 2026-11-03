#!/bin/zsh
# Experimental offline reference boot. Disables Wine WMV decoder to bypass
# the observed winegstreamer crash. This does not qualify video support or Metal.
set -eu
resources="${0:A:h}"
tools_root="${PDAFT_TOOLS_ROOT:-$resources}"
export PDAFT_DLL_OVERRIDES='dinput8=n,b;dnsapi=n,b;mscoree,mshtml,winegstreamer='
export MTL_HUD_ENABLED="${PDAFT_METAL_HUD:-1}"
export MTL_HUD_LOG_ENABLED="${PDAFT_METAL_HUD_LOG:-1}"
export GST_PLUGIN_SYSTEM_PATH_1_0="$PDAFT_WORK_ROOT/bridge/gst-plugins-v1"
export GST_PLUGIN_PATH_1_0=""
export GST_REGISTRY_1_0="$PDAFT_WORK_ROOT/bridge/gst-native-registry-v1.bin"
export GST_REGISTRY_FORK=no
export GST_DEBUG="${PDAFT_VIDEO_DEBUG:-0}"
export PDAFT_NATIVE_MOVIES=1
exec zsh "$tools_root/run-wine.sh" 'C:\PDAFT\diva.exe' --launch -w "$@"
