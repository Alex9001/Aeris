param([Parameter(Mandatory=$true)][string]$Arch, [Parameter(Mandatory=$true)][string]$Triplet)
$ErrorActionPreference = 'Stop'
$stage = Join-Path $PWD 'build-release/portable'
New-Item -ItemType Directory -Force $stage, 'release-assets' | Out-Null
cmake --install build-release --prefix $stage
if ($LASTEXITCODE) { throw 'Install failed' }
$bin = Join-Path $stage 'bin'
Copy-Item "vcpkg_installed/$Triplet/bin/*.dll" $bin
Copy-Item 'build-deps/install/bin/*.dll' $bin
windeployqt --release --no-translations --compiler-runtime "$bin/aeris.exe"
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
Copy-Item LICENSE, THIRD_PARTY_NOTICES.md $stage
python scripts/licenses.py "$stage/licenses" --vcpkg "vcpkg_installed/$Triplet" --qt "$env:QT_ROOT_DIR"
if ($LASTEXITCODE) { throw 'License collection failed' }
syft "dir:$stage" -o "spdx-json=release-assets/aeris_windows_$Arch.sbom.json"
if ($LASTEXITCODE) { throw 'SBOM generation failed' }
python scripts/complete-sbom.py "release-assets/aeris_windows_$Arch.sbom.json" --vcpkg "vcpkg_installed/$Triplet"
if ($LASTEXITCODE) { throw 'SBOM enrichment failed' }
python scripts/check-sbom.py "release-assets/aeris_windows_$Arch.sbom.json"
if ($LASTEXITCODE) { throw 'SBOM incomplete' }
Compress-Archive -Path "$stage/*" -DestinationPath "release-assets/aeris_windows_$Arch.zip" -Force
# Smoke test an extracted portable ZIP on its native architecture.
$extracted = Join-Path $PWD 'build-release/smoke-portable'
Expand-Archive "release-assets/aeris_windows_$Arch.zip" $extracted -Force
$env:QT_QPA_PLATFORM = 'offscreen'
$p = Start-Process "$extracted/bin/aeris.exe" -ArgumentList '--smoke-test' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Packaged portable app smoke test failed' }
$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(Aeris VERSION ([0-9.]+)').Matches.Groups[1].Value
$uninstallManifest = Join-Path $PWD 'build-release/uninstall-files.nsh'
python scripts/nsis-manifest.py $stage $uninstallManifest
if ($LASTEXITCODE) { throw 'Uninstall manifest generation failed' }
& "${env:ProgramFiles(x86)}/NSIS/makensis.exe" "/DVERSION=$version" "/DARCH=$Arch" "/DSTAGE=$stage" "/DUNINSTALL_MANIFEST=$uninstallManifest" packaging/windows/installer.nsi
if ($LASTEXITCODE) { throw 'NSIS packaging failed' }
$installer = Join-Path $PWD "release-assets/aeris_windows_$Arch-setup.exe"
$installDir = Join-Path $PWD 'build-release/smoke-installed'
$p = Start-Process $installer -ArgumentList '/S', "/D=$installDir" -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Installer smoke test failed' }
$p = Start-Process "$installDir/bin/aeris.exe" -ArgumentList '--smoke-test' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Installed app smoke test failed' }
$p = Start-Process "$installDir/Uninstall.exe" -ArgumentList '/S' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Uninstaller smoke test failed' }
