; The name of the installer
Name "Xjadeo"

; The file to write
OutFile "xjadeo_installer_vVERSION.exe"

; The default installation directory
InstallDir $PROGRAMFILES\xjadeo

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\RSS\xjadeo" "Install_Dir"

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "Xjadeo (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
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
  WriteRegStr HKLM SOFTWARE\RSS\xjadeo "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo" "DisplayName" "Xjadeo"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
  
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  CreateDirectory "$SMPROGRAMS\xjadeo"
  CreateShortCut "$SMPROGRAMS\xjadeo\qjadeo.lnk" "$INSTDIR\qjadeo.exe" "" "$INSTDIR\qjadeo.exe" 0
  CreateShortCut "$SMPROGRAMS\xjadeo\xjadeo.lnk" "$INSTDIR\xjadeo.exe" "" "$INSTDIR\xjadeo.exe" 0
  CreateShortCut "$SMPROGRAMS\xjadeo\uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
SectionEnd

;--------------------------------

; Uninstaller

Section "Uninstall"
  
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo"
  DeleteRegKey HKLM SOFTWARE\RSSxjadeo
  DeleteRegKey HKLM SOFTWARE\RSS\xjadeo

  ; Remove files and uninstaller
  Delete $INSTDIR\xjadeo.exe
  Delete $INSTDIR\ArdourMono.ttf
  Delete $INSTDIR\xjadeo.nsi
  Delete $INSTDIR\xjremote.bat
  Delete $INSTDIR\uninstall.exe

  Delete "$INSTDIR\*.dll"

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\xjadeo\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\xjadeo"
  RMDir "$INSTDIR"

SectionEnd
