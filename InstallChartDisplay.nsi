!include "MUI2.nsh"

Name "ChartDisplay"
OutFile "InstallChartDisplay.exe"
Unicode True

; Version is single-sourced from version.h. This .nsi is compiled from
; the x64\Release staging dir, so version.h sits at ..\..\ChartDisplay\version.h relative to here.
!searchparse /file "..\..\ChartDisplay\version.h" `#define CD_VER_MAJOR ` CD_VER_MAJOR
!searchparse /file "..\..\ChartDisplay\version.h" `#define CD_VER_MINOR ` CD_VER_MINOR
!searchparse /file "..\..\ChartDisplay\version.h" `#define CD_VER_PATCH ` CD_VER_PATCH
!searchparse /file "..\..\ChartDisplay\version.h" `#define CD_VER_BUILD ` CD_VER_BUILD
!define PRODUCT_VERSION "${CD_VER_MAJOR}.${CD_VER_MINOR}.${CD_VER_PATCH}"

; Stamp the installer .exe's own version resource
VIProductVersion "${CD_VER_MAJOR}.${CD_VER_MINOR}.${CD_VER_PATCH}.${CD_VER_BUILD}"
VIAddVersionKey "ProductName"     "ChartDisplay"
VIAddVersionKey "ProductVersion"  "${PRODUCT_VERSION}"
VIAddVersionKey "FileVersion"     "${PRODUCT_VERSION}"
VIAddVersionKey "FileDescription" "ChartDisplay installer"
VIAddVersionKey "LegalCopyright"  "Copyright (C) 2025 Matthew Moran"

InstallDir "$LOCALAPPDATA\ChartDisplay\bin"

; Refuse to install/uninstall while ChartDisplay is running: its exe would be locked and File/Delete would
; fail mid-operation. FindWindow is done on the app's registered window class.
Function .onInit
  check_running:
  FindWindow $0 "ChartDisplayWinClass"
  StrCmp $0 0 done_running
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "ChartDisplay appears to be running.$\n$\nClose it, then click Retry to continue installing (or Cancel to abort)." /SD IDCANCEL IDRETRY check_running
  Abort
  done_running:
FunctionEnd

Function un.onInit
  un_check_running:
  FindWindow $0 "ChartDisplayWinClass"
  StrCmp $0 0 un_done_running
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "ChartDisplay appears to be running.$\n$\nClose it, then click Retry to continue uninstalling (or Cancel to abort)." /SD IDCANCEL IDRETRY un_check_running
  Abort
  un_done_running:
FunctionEnd

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

WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayName" "$(^Name)"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "UninstallString" "$INSTDIR\uninstall.exe"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayVersion" "${PRODUCT_VERSION}"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayIcon" "$INSTDIR\ChartDisplay.exe"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "Publisher" "Matthew Moran"
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "NoModify" 1
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "NoRepair" 1
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
