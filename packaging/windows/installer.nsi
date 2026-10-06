Unicode true
!include "MUI2.nsh"
Name "Aeris"
OutFile "..\..\release-assets\aeris_windows_${ARCH}-setup.exe"
InstallDir "$LOCALAPPDATA\Programs\Aeris"
RequestExecutionLevel user
SetCompressor /SOLID lzma
!define MUI_ICON "..\..\assets\aeris.ico"
!define MUI_UNICON "..\..\assets\aeris.ico"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "..\..\LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Section "Aeris" Main
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*"
  CreateShortcut "$SMPROGRAMS\Aeris.lnk" "$INSTDIR\bin\aeris.exe"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "DisplayName" "Aeris"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "Publisher" "CYBER FRACTURE"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris" "NoRepair" 1
SectionEnd
Section "Uninstall"
  SetShellVarContext current
  Delete "$SMPROGRAMS\Aeris.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Aeris"
  !include "${UNINSTALL_MANIFEST}"
SectionEnd
