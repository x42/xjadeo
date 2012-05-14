; The name of the installer
Name "Jadeo"

; The file to write
OutFile "jadeo_installer_vVERSION.exe"

; The default installation directory
InstallDir $PROGRAMFILES\xjadeo

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
InstallDirRegKey HKLM "Software\RSSxjadeo" "Install_Dir"

;--------------------------------

; Pages

Page components
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

;--------------------------------

; The stuff to install
Section "jadeo (required)"

  SectionIn RO
  
  ; Set output path to the installation directory.
  SetOutPath $INSTDIR
  
  ; Put file there
  File "xjadeo.exe"
  File "xjinfo.exe"
	File "SDL.dll"
	File "avcodec-54.dll"
	File "avformat-54.dll"
	File "avutil-51.dll"
	File "freetype6.dll"
	File "swscale-2.dll"
	File "libltcsmpte-0.dll"
	File "zlib1.dll"
	File "FreeMonoBold.ttf"
  File "xjadeo.nsi"

  File "qjadeo.exe"
  File "qjadeo_fr.qm"
  File "qjadeo_ru.qm"
  File "QtCore4.dll"
  File "QtGui4.dll"
  File "QtSvg4.dll"
  File "QtTest4.dll"
  File "libgcc_s_dw2-1.dll"
  File "mingwm10.dll"

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
  WriteRegStr HKLM SOFTWARE\RSSxjadeo "Install_Dir" "$INSTDIR"
  
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\xjadeo" "DisplayName" "Jadeo"
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

  ; Remove files and uninstaller
  Delete $INSTDIR\xjadeo.exe
  Delete $INSTDIR\xjinfo.exe
  Delete $INSTDIR\SDL.dll
  Delete $INSTDIR\avcodec-52.dll
  Delete $INSTDIR\avformat-52.dll
  Delete $INSTDIR\avutil-49.dll
  Delete $INSTDIR\swscale-0.dll
  Delete $INSTDIR\freetype6.dll
  Delete $INSTDIR\libltcsmpte-0.dll
  Delete $INSTDIR\zlib1.dll
  Delete $INSTDIR\FreeMonoBold.ttf
  Delete $INSTDIR\xjadeo.nsi
  Delete $INSTDIR\qjadeo.exe
  Delete $INSTDIR\qjadeo_fr.qm
  Delete $INSTDIR\qjadeo_ru.qm
  Delete $INSTDIR\QtCore4.dll
  Delete $INSTDIR\QtGui4.dll
  Delete $INSTDIR\QtSvg4.dll
  Delete $INSTDIR\QtTest4.dll
  Delete $INSTDIR\libgcc_s_dw2-1.dll
  Delete $INSTDIR\mingwm10.dll
  Delete $INSTDIR\xjremote.bat
  Delete $INSTDIR\uninstall.exe

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\xjadeo\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\xjadeo"
  RMDir "$INSTDIR"

SectionEnd
