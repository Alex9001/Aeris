param([Parameter(Mandatory=$true)][string]$Arch,
      [Parameter(Mandatory=$true)][string]$Triplet,
      [string]$Makensis = $env:AERIS_MAKENSIS)
$ErrorActionPreference = 'Stop'
if (!$Makensis) {
    $command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($command) { $Makensis = $command.Source }
    else { $Makensis = "${env:ProgramFiles(x86)}/NSIS/makensis.exe" }
}
if (!(Test-Path $Makensis)) { throw "NSIS compiler not found: $Makensis" }
$stage = Join-Path $PWD 'build-release/portable'
New-Item -ItemType Directory -Force $stage, 'release-assets' | Out-Null
cmake --install build-release --prefix $stage
if ($LASTEXITCODE) { throw 'Install failed' }
$bin = Join-Path $stage 'bin'
Copy-Item "vcpkg_installed/$Triplet/bin/*.dll" $bin
Copy-Item 'build-deps/install/bin/*.dll' $bin
if (!(Test-Path "$bin/qt6keychain.dll")) { throw 'QtKeychain DLL was not staged' }
# Aeris links Qt Widgets. Deploy that SDK module and its plugins directly:
# scanning aeris.exe mistakes the separate qt6keychain.dll for an SDK module.
windeployqt --release --no-translations --no-compiler-runtime --widgets --dir $bin "$env:QT_ROOT_DIR/bin/Qt6Widgets.dll"
if ($LASTEXITCODE) { throw 'Qt deployment failed' }
# Use Microsoft's redistributable directory for application-local deployment.
# A redistributable installer alone does not make a per-user/portable app runnable.
$crtArch = if ($Arch -eq 'arm64') { 'arm64' } else { 'x64' }
if (!$env:VCToolsRedistDir) { throw 'Run packaging in an MSVC developer environment (VCToolsRedistDir missing)' }
$crt = Join-Path $env:VCToolsRedistDir "$crtArch/Microsoft.VC143.CRT"
if (!(Test-Path "$crt/vcruntime140.dll")) { throw "MSVC redistributable DLLs missing: $crt" }
Copy-Item "$crt/*.dll" $bin
Copy-Item "$env:QT_ROOT_DIR/plugins/platforms/qoffscreen.dll" "$bin/platforms/"
Copy-Item LICENSE, THIRD_PARTY_NOTICES.md $stage
python scripts/normalize-dependencies.py build-release/dependencies.json "vcpkg_installed/$Triplet"
if ($LASTEXITCODE) { throw 'Dependency metadata failed' }
Copy-Item build-release/dependencies.json "$stage/share/aeris/dependencies.json"
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
function Test-PackagedApp([string]$Application, [string]$Platform) {
    $savedPath = $env:Path
    $savedPlatform = $env:QT_QPA_PLATFORM
    $savedPlugins = $env:QT_PLUGIN_PATH
    $savedPlatformPlugins = $env:QT_QPA_PLATFORM_PLUGIN_PATH
    try {
        # Verify deployed DLLs/plugins without the SDK or vcpkg on PATH.
        $env:Path = "$env:SystemRoot\System32;$env:SystemRoot"
        $env:QT_QPA_PLATFORM = $Platform
        $env:QT_PLUGIN_PATH = Split-Path $Application
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path (Split-Path $Application) 'platforms'
        $p = Start-Process $Application -ArgumentList '--smoke-test' -Wait -PassThru
        if ($p.ExitCode -ne 0) { throw "Packaged $Platform app smoke test failed: $($p.ExitCode)" }
    } finally {
        $env:Path = $savedPath
        $env:QT_QPA_PLATFORM = $savedPlatform
        $env:QT_PLUGIN_PATH = $savedPlugins
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = $savedPlatformPlugins
    }
}
Test-PackagedApp "$extracted/bin/aeris.exe" 'offscreen'
Test-PackagedApp "$extracted/bin/aeris.exe" 'windows'
$version = (Select-String -Path CMakeLists.txt -Pattern 'project\(Aeris VERSION ([0-9.]+)').Matches.Groups[1].Value
$uninstallManifest = Join-Path $PWD 'build-release/uninstall-files.nsh'
python scripts/nsis-manifest.py $stage $uninstallManifest
if ($LASTEXITCODE) { throw 'Uninstall manifest generation failed' }
& $Makensis "/DVERSION=$version" "/DARCH=$Arch" "/DSTAGE=$stage" "/DUNINSTALL_MANIFEST=$uninstallManifest" packaging/windows/installer.nsi
if ($LASTEXITCODE) { throw 'NSIS packaging failed' }
$installer = Join-Path $PWD "release-assets/aeris_windows_$Arch-setup.exe"
$installDir = Join-Path $PWD 'build-release/smoke-installed'
$p = Start-Process $installer -ArgumentList '/S', "/D=$installDir" -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Installer smoke test failed' }
Test-PackagedApp "$installDir/bin/aeris.exe" 'offscreen'
Test-PackagedApp "$installDir/bin/aeris.exe" 'windows'
$retained = Join-Path $installDir 'unrelated-file.txt'
'Synthetic unrelated file; uninstall must preserve it.' | Set-Content $retained
$p = Start-Process "$installDir/Uninstall.exe" -ArgumentList '/S' -Wait -PassThru
if ($p.ExitCode -ne 0) { throw 'Uninstaller smoke test failed' }
$deadline = [DateTime]::UtcNow.AddSeconds(30)
while ((Test-Path "$installDir/bin/aeris.exe") -and [DateTime]::UtcNow -lt $deadline) {
    Start-Sleep -Milliseconds 100
}
if (Test-Path "$installDir/bin/aeris.exe") { throw 'Uninstaller retained the application' }
if (!(Test-Path $retained)) { throw 'Uninstaller deleted an unrelated file' }
Remove-Item $retained
Remove-Item $installDir -ErrorAction SilentlyContinue
