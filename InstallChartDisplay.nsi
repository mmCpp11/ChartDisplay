!include "MUI2.nsh"

Name "ChartDisplay"
OutFile "InstallChartDisplay.exe"
Unicode True

InstallDir "$LOCALAPPDATA\ChartDisplay\bin"

;Pages
  !insertmacro MUI_PAGE_LICENSE "gpl-3.0.rtf"
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES

  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_LANGUAGE "English"

Section

SetOutPath "$INSTDIR"

File ChartDisplay.exe
File bz2.dll
File pugixml.dll
File sqlite3.dll
File zip.dll
File zlib1.dll

; Release VC++ runtime, staged next to the exe by the Release post-build step. Bundling it app-local lets
; the program launch on a machine without the VC++ 2015-2022 x64 redistributable installed.
File msvcp140.dll
File msvcp140_atomic_wait.dll
File vcruntime140.dll
File vcruntime140_1.dll

SetOutPath "$INSTDIR\.."

File "Third Party Licenses.txt"
File Readme.md

; No seed database is shipped: the app generates chartdisplay.sqlite in
; $LOCALAPPDATA\ChartDisplay on first run (AIRAC cycles built arithmetically).

SetOutPath "$INSTDIR"

WriteUninstaller "$INSTDIR\uninstall.exe"

CreateShortCut "$SMPROGRAMS\ChartDisplay\ChartDisplay.lnk" "$INSTDIR\ChartDisplay.exe"
CreateShortCut "$SMPROGRAMS\ChartDisplay\uninstall.lnk" "$INSTDIR\uninstall.exe"

WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayName" "$(^Name)"
WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "UninstallString" "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"

RMDir /r "$LOCALAPPDATA\ChartDisplay\Charts"
RMDir /r "$LOCALAPPDATA\ChartDisplay\download"
Delete "$LOCALAPPDATA\ChartDisplay\chartdisplay.sqlite"

Delete "$INSTDIR\ChartDisplay.exe"
Delete "$INSTDIR\bz2.dll"
Delete "$INSTDIR\pugixml.dll"
Delete "$INSTDIR\sqlite3.dll"
Delete "$INSTDIR\zip.dll"
Delete "$INSTDIR\zlib1.dll"
Delete "$INSTDIR\msvcp140.dll"
Delete "$INSTDIR\msvcp140_atomic_wait.dll"
Delete "$INSTDIR\vcruntime140.dll"
Delete "$INSTDIR\vcruntime140_1.dll"
Delete "$INSTDIR\uninstall.exe"

RMDir "$INSTDIR"
RMDir /r "$INSTDIR\.."

RMDir /r "$SMPROGRAMS\ChartDisplay"

DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"

SectionEnd
