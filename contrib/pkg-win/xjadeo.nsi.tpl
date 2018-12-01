SetCompressor /SOLID lzma
SetCompressorDictSize 32
RequestExecutionLevel admin

Name "Xjadeo"
OutFile "xjadeo_installer_@WARCH@_v@VERSION@.exe"

InstallDir $@PROGRAMFILES@\xjadeo
InstallDirRegKey HKLM "Software\RSS\xjadeo\@WARCH@" "Install_Dir"

;--------------------------------

!include MUI2.nsh

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

;--------------------------------

Section "Xjadeo (required)" SecMainProg
  SectionIn RO
  
  SetOutPath $INSTDIR
  
  ; Put file there
  File "xjadeo.exe"
  File "ArdourMono.ttf"
  File "xjadeo.nsi"
  File /r "*.dll"
  ClearErrors
  FileOpen $0 $INSTDIR\xjremote.bat w
  IfErrors done
  FileWrite $0 "@echo off"
  FileWriteByte $0 "13"
  FileWriteByte $0 "10"
  FileWrite $0 "cd $INSTDIR"
  FileWriteByte $0 "13"
  FileWriteByte $0 "10"
  FileWrite $0 "xjadeo.exe -R"
  FileWriteByte $0 "13"
  FileWriteByte $0 "10"
  FileClose $0
  done:
  
  ; Write the installation path into the registry
  WriteRegStr HKLM SOFTWARE\RSS\xjadeo\@WARCH@ "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo-@WARCH@" "DisplayName" "Xjadeo@SFX@"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo-@WARCH@" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo-@WARCH@" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo-@WARCH@" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Start Menu Shortcuts" SecMenu
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\xjadeo@SFX@"
  CreateShortCut "$SMPROGRAMS\xjadeo@SFX@\xjadeo.lnk" "$INSTDIR\xjadeo.exe" "" "$INSTDIR\xjadeo.exe" 0
  CreateShortCut "$SMPROGRAMS\xjadeo@SFX@\uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

;--------------------------------

LangString DESC_SecMainProg ${LANG_ENGLISH} "X-JACK-Video Monitor"
LangString DESC_SecMenu ${LANG_ENGLISH} "Create Start-Menu Shortcuts (recommended)."

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
!insertmacro MUI_DESCRIPTION_TEXT ${SecMainProg} $(DESC_SecMainProg)
!insertmacro MUI_DESCRIPTION_TEXT ${SecMenu} $(DESC_SecMenu)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------

; Uninstaller

Section "Uninstall"
  SetShellVarContext all

  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo-@WARCH@"
  DeleteRegKey HKLM SOFTWARE\RSSxjadeo
  DeleteRegKey HKLM SOFTWARE\RSS\xjadeo
  DeleteRegKey HKLM SOFTWARE\RSS\xjadeo\@WARCH@

  ; Remove files and uninstaller
  Delete $INSTDIR\xjadeo.exe
  Delete $INSTDIR\ArdourMono.ttf
  Delete $INSTDIR\xjadeo.nsi
  Delete $INSTDIR\xjremote.bat
  Delete $INSTDIR\uninstall.exe

  Delete "$INSTDIR\*.dll"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\xjadeo@SFX@\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\xjadeo@SFX@"
  RMDir "$INSTDIR"
SectionEnd
