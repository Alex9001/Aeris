#!/usr/bin/env bash
set -euo pipefail
triplet=$1
app=build-release/aeris.app
mkdir -p "$app/Contents/Resources/licenses" release-assets
python3 scripts/normalize-dependencies.py build-release/dependencies.json "vcpkg_installed/$triplet"
cp build-release/dependencies.json "$app/Contents/Resources/dependencies.json"
cp LICENSE THIRD_PARTY_NOTICES.md "$app/Contents/Resources/"
python3 scripts/licenses.py "$app/Contents/Resources/licenses" --vcpkg "vcpkg_installed/$triplet" --qt "$QT_ROOT_DIR"
macdeployqt "$app" -always-overwrite
# Every non-system dylib must now resolve inside the relocatable bundle.
python3 scripts/audit-macos.py "$app"
QT_QPA_PLATFORM=offscreen "$app/Contents/MacOS/aeris" --smoke-test
tar -czf "release-assets/aeris_${triplet}.app.tar.gz" -C build-release aeris.app
