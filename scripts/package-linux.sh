#!/usr/bin/env bash
set -euo pipefail
build=${1:-build-release}
out=${2:-release-assets}
arch=$(uname -m)
case "$arch" in x86_64) artifact_arch=amd64;; aarch64) artifact_arch=arm64;; *) exit 2;; esac
mkdir -p "$out"
appdir="$build/AppDir"
DESTDIR="$PWD/$appdir" cmake --install "$build" --prefix /usr
python3 scripts/licenses.py "$appdir/usr/share/aeris/licenses"
# linuxdeploy-plugin-qt brings both platform integrations and their dependencies.
export QMAKE=${QMAKE:-qmake6}
export EXTRA_QT_MODULES='waylandcompositor'
export EXTRA_QT_PLUGINS='platforms;imageformats;wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration'
export EXTRA_PLATFORM_PLUGINS='libqwayland-generic.so;libqwayland-egl.so;libqxcb.so'
export OUTPUT="$PWD/$out/aeris_linux_${artifact_arch}.AppImage"
export APPIMAGE_EXTRACT_AND_RUN=1
"${LINUXDEPLOY:?set LINUXDEPLOY}" --appdir "$appdir" --plugin qt --output appimage
find "$appdir" -name '*qxcb*' -print -quit | rg -q .
find "$appdir" -name '*qwayland*' -print -quit | rg -q .
for group in wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
  find "$appdir/usr/plugins/$group" -name '*.so' -print -quit | rg -q .
done
syft "dir:$appdir" -o "spdx-json=$out/aeris_linux_${artifact_arch}.sbom.json"
python3 scripts/complete-sbom.py "$out/aeris_linux_${artifact_arch}.sbom.json" --build "$build"
python3 scripts/check-sbom.py "$out/aeris_linux_${artifact_arch}.sbom.json"
