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

  File "avcodec-55.dll"
  File "avformat-55.dll"
  File "avutil-52.dll"
  File "swscale-2.dll"
  File "libogg-0.dll"
  File "libtheora-0.dll"
  File "libtheoradec-1.dll"
  File "libtheoraenc-1.dll"
  File "libfreetype-6.dll"
  File "libltc-11.dll"
  File "libiconv-2.dll"
  File "zlib1.dll"
  File "pthreadGC2.dll"
  File "libx264-142.dll"
  File "libportmidi-0.dll"
  File "libporttime-0.dll"

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

  Delete $INSTDIR\avcodec-55.dll
  Delete $INSTDIR\avformat-55.dll
  Delete $INSTDIR\avutil-52.dll
  Delete $INSTDIR\swscale-2.dll
  Delete $INSTDIR\libogg-0.dll
  Delete $INSTDIR\libtheora-0.dll
  Delete $INSTDIR\libtheoradec-1.dll
  Delete $INSTDIR\libtheoraenc-1.dll
  Delete $INSTDIR\libfreetype-6.dll
  Delete $INSTDIR\libltc-11.dll
  Delete $INSTDIR\libx264-142.dll
  Delete $INSTDIR\libiconv-2.dll
  Delete $INSTDIR\zlib1.dll
  Delete $INSTDIR\pthreadGC2.dll
  Delete $INSTDIR\libportmidi-0.dll
  Delete $INSTDIR\libporttime-0.dll

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\xjadeo\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\xjadeo"
  RMDir "$INSTDIR"

SectionEnd
