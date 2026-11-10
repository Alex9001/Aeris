#!/usr/bin/env bash
set -euo pipefail
triplet=$1
mode=${2:-}
if [[ -n "$mode" && "$mode" != --dmg ]]; then
  printf 'Usage: %s <triplet> [--dmg]\n' "$0" >&2
  exit 1
fi
app=build-release/aeris.app
mkdir -p "$app/Contents/Resources/licenses" release-assets
python3 scripts/normalize-dependencies.py build-release/dependencies.json "vcpkg_installed/$triplet"
cp build-release/dependencies.json "$app/Contents/Resources/dependencies.json"
cp LICENSE THIRD_PARTY_NOTICES.md "$app/Contents/Resources/"
python3 scripts/licenses.py "$app/Contents/Resources/licenses" --vcpkg "vcpkg_installed/$triplet" --qt "$QT_ROOT_DIR"
# macdeployqt deploys Cocoa only; include and relocate our offscreen smoke plugin.
plugins="$app/Contents/PlugIns/platforms"
mkdir -p "$plugins"
cp "$QT_ROOT_DIR/plugins/platforms/libqoffscreen.dylib" "$plugins/"
macdeployqt "$app" -always-overwrite "-executable=$plugins/libqoffscreen.dylib"
codesign --force --deep --sign - "$app"
# Every non-system dylib must now resolve inside the relocatable bundle.
python3 scripts/audit-macos.py "$app"
smoke_bundle() {
  local bundle=$1 platform=$2
  env -u DYLD_LIBRARY_PATH -u DYLD_FRAMEWORK_PATH \
    -u DYLD_FALLBACK_LIBRARY_PATH -u DYLD_FALLBACK_FRAMEWORK_PATH \
    PATH=/usr/bin:/bin:/usr/sbin:/sbin QT_QPA_PLATFORM="$platform" \
    QT_PLUGIN_PATH="$bundle/Contents/PlugIns" \
    QT_QPA_PLATFORM_PLUGIN_PATH="$bundle/Contents/PlugIns/platforms" \
    "$bundle/Contents/MacOS/aeris" --smoke-test
}
smoke_bundle "$app" offscreen
tar -czf "release-assets/aeris_${triplet}.app.tar.gz" -C build-release aeris.app
if [[ "$mode" == --dmg ]]; then
  [[ "$triplet" == x64-osx-dynamic ]] || { printf 'Local DMG requires x64-osx-dynamic\n' >&2; exit 1; }
  python3 scripts/audit-macos.py "$app" --arch x86_64
  syft "dir:$app" -o spdx-json=release-assets/aeris_macos_x64.sbom.json
  python3 scripts/complete-sbom.py release-assets/aeris_macos_x64.sbom.json --vcpkg "vcpkg_installed/$triplet"
  python3 scripts/check-sbom.py release-assets/aeris_macos_x64.sbom.json
  dmg_stage=build-release/dmg
  mkdir -p "$dmg_stage"
  ditto "$app" "$dmg_stage/Aeris.app"
  ln -sfn /Applications "$dmg_stage/Applications"
  hdiutil create -volname Aeris -srcfolder "$dmg_stage" -ov -format UDZO release-assets/aeris_macos_x64.dmg
  mount=$(mktemp -d)
  trap 'hdiutil detach "$mount" >/dev/null 2>&1 || true; rmdir "$mount"' EXIT
  hdiutil attach -nobrowse -mountpoint "$mount" release-assets/aeris_macos_x64.dmg
  codesign --verify --deep --strict "$mount/Aeris.app"
  smoke_bundle "$mount/Aeris.app" cocoa
  hdiutil detach "$mount"
  rmdir "$mount"
  trap - EXIT
fi
