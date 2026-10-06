#!/usr/bin/env bash
# Real, decorated X11 captures with synthetic data; no access to the user's vault.
set -euo pipefail
build=$(realpath "${1:-build-linux}")
mkdir -p "${2:-docs/screenshots}"
output=$(realpath "${2:-docs/screenshots}")
runtime=$(mktemp -d "$build/screenshots.XXXXXX")
trap 'rm -rf "$runtime"' EXIT
cat > "$runtime/openbox.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<openbox_config xmlns="http://openbox.org/3.4/rc">
  <theme>
    <name>Breeze-ob</name><titleLayout>NLIMC</titleLayout>
    <font place="ActiveWindow"><name>DejaVu Sans</name><size>18</size><weight>Normal</weight></font>
    <font place="InactiveWindow"><name>DejaVu Sans</name><size>18</size><weight>Normal</weight></font>
  </theme>
  <focus><focusNew>yes</focusNew><followMouse>no</followMouse></focus>
</openbox_config>
XML
export XDG_CONFIG_HOME="$runtime/config" XDG_DATA_HOME="$runtime/data" TMPDIR="$runtime"
export QT_QPA_PLATFORM=xcb QT_SCALE_FACTOR=2 QT_FONT_DPI=96 AERIS_SCREENSHOT_DIR="$output"
for scene in Light-Compact Dark-Cards Midnight-List Ocean-Cards Forest-Compact Violet-Cards Rose-List Paper-Compact System-List; do
export AERIS_SCREENSHOT_SCENE="$scene"
xvfb-run -a -s '-screen 0 3200x2200x24 -dpi 96' bash -c '
  openbox --config-file "$1" > "$4/openbox.log" 2>&1 &
  wm_pid=$!
  trap '\''kill "$wm_pid" 2>/dev/null || true'\'' EXIT
  sleep 1
  "$2/test_ui" gallery
' _ "$runtime/openbox.xml" "$build" "$output" "$runtime"
done
