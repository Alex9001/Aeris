#!/usr/bin/env bash
set -euo pipefail
app=$(realpath "$1")
runtime=$(mktemp -d)
chmod 700 "$runtime"
trap 'kill "${weston_pid:-}" 2>/dev/null || true; rm -rf "$runtime"' EXIT
export XDG_DATA_HOME="$runtime/data" XDG_CONFIG_HOME="$runtime/config"
export APPIMAGE_EXTRACT_AND_RUN=1
xvfb-run -a env QT_QPA_PLATFORM=xcb "$app" --smoke-test
XDG_RUNTIME_DIR="$runtime" weston --backend=headless-backend.so --socket=aeris-smoke --idle-time=0 --use-pixman --log="$runtime/weston.log" &
weston_pid=$!
for _ in $(seq 1 50); do
  if [ -S "$runtime/aeris-smoke" ]; then break; fi
  sleep 0.1
done
test -S "$runtime/aeris-smoke"
XDG_RUNTIME_DIR="$runtime" WAYLAND_DISPLAY=aeris-smoke QT_QPA_PLATFORM=wayland "$app" --smoke-test
