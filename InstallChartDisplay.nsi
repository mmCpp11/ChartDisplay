!include "MUI2.nsh"
!include "FileFunc.nsh"

Name "ChartDisplay"
OutFile "InstallChartDisplay.exe"
Unicode True

; Everything this installer touches is per-user
RequestExecutionLevel user

; Set when launched with /UPDATE
Var UpdateMode

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
VIAddVersionKey "LegalCopyright"  "Copyright (C) 2025-2026 Matthew Moran"

InstallDir "$LOCALAPPDATA\ChartDisplay\bin"

; Refuse to install/uninstall while ChartDisplay is running

Function .onInit
  StrCpy $UpdateMode 0
  ${GetParameters} $R0
  ClearErrors
  ${GetOptions} $R0 "/UPDATE" $R1
  IfErrors not_update
  StrCpy $UpdateMode 1
  not_update:

  ; Install over the existing copy rather than the default location
  ReadRegStr $0 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "InstallLocation"
  StrCmp $0 "" 0 have_location
  ; Installs predating InstallLocation still wrote UninstallString ($INSTDIR\uninstall.exe); derive it from
  ; there so the first self-update lands on an existing custom directory instead of forking a second copy.
  ReadRegStr $1 HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "UninstallString"
  StrCmp $1 "" no_prior_install
  ${GetParent} $1 $0
  StrCmp $0 "" no_prior_install
  have_location:
  StrCpy $INSTDIR $0
  no_prior_install:

  StrCpy $R2 0   ; 500ms ticks spent waiting for the app to exit
  check_running:
  FindWindow $0 "ChartDisplayWinClass"
  StrCmp $0 0 done_running
  StrCmp $UpdateMode 1 wait_for_exit
  MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "ChartDisplay appears to be running.$\n$\nClose it, then click Retry to continue installing (or Cancel to abort)." /SD IDCANCEL IDRETRY check_running
  Abort
  wait_for_exit:
  IntCmp $R2 60 wait_timeout 0 wait_timeout   ; 60 ticks = 30s, then give up
  Sleep 500
  IntOp $R2 $R2 + 1
  Goto check_running
  wait_timeout:
  ; The app never went away. Leave the installed copy untouched; the updater falls back to the download page.
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

; Skips the page it is attached to during a self-update. The license was accepted at first install and
; .onInit has already forced $INSTDIR to the recorded location, so neither page can tell the user anything
; or change anything; Abort in a PRE function is how MUI skips a page.
Function SkipInUpdate
  StrCmp $UpdateMode 1 0 +2
  Abort
FunctionEnd

;Pages
  !define MUI_PAGE_CUSTOMFUNCTION_PRE SkipInUpdate
  !insertmacro MUI_PAGE_LICENSE "gpl-3.0.rtf"
  !define MUI_PAGE_CUSTOMFUNCTION_PRE SkipInUpdate
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
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "InstallLocation" "$INSTDIR"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayVersion" "${PRODUCT_VERSION}"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "DisplayIcon" "$INSTDIR\ChartDisplay.exe"
WriteRegStr   HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "Publisher" "Matthew Moran"
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "NoModify" 1
WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)" "NoRepair" 1

StrCmp $UpdateMode 1 0 skip_relaunch
; MUI_PAGE_INSTFILES is the last page, so without this the installer waits on its completed progress view for
; a Close click. A self-update should get out of the way once the app is back up; a manual install still stops
; on that page so the result can be read.
SetAutoClose true
Exec "$INSTDIR\ChartDisplay.exe"
skip_relaunch:
SectionEnd

Section "Uninstall"

; Ask before deleting charts
MessageBox MB_YESNO|MB_ICONQUESTION "Also delete your downloaded charts and custom chart entries?$\n$\nThis frees several GB in $LOCALAPPDATA\ChartDisplay but means a full chart download on any future reinstall.$\n$\nChoose No to keep them." /SD IDNO IDYES remove_userdata
Goto keep_userdata
remove_userdata:
RMDir /r "$LOCALAPPDATA\ChartDisplay\Charts"
RMDir /r "$LOCALAPPDATA\ChartDisplay\download"
Delete "$LOCALAPPDATA\ChartDisplay\chartdisplay.sqlite"
Delete "$LOCALAPPDATA\ChartDisplay\custom_charts.xml"
keep_userdata:

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

Delete "$INSTDIR\..\Third Party Licenses.txt"
Delete "$INSTDIR\..\Readme.md"

; Both non-recursive on purpose
RMDir "$INSTDIR"
RMDir "$INSTDIR\.."

RMDir /r "$SMPROGRAMS\ChartDisplay"

DeleteRegKey HKCU "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\$(^Name)"

; App preferences (Help->Check for Program Updates on Start)
DeleteRegKey HKCU "Software\ChartDisplay"

SectionEnd
