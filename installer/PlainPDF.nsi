Unicode true
!include "MUI2.nsh"
!include "x64.nsh"
!include "WinVer.nsh"

!define PRODUCT "PlainPDF (${ARCH})"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\PlainPDF-${ARCH}"
!define PROG_ID "PlainPDF.PDF.${ARCH}"
!define PROG_KEY "Software\Classes\${PROG_ID}"
!define OPEN_WITH "Software\Classes\.pdf\OpenWithProgids"
!define MUI_ICON "${__FILEDIR__}\..\src\PlainPDF.ico"
!define MUI_UNICON "${MUI_ICON}"

Name "${PRODUCT}"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\PlainPDF-${ARCH}"
RequestExecutionLevel user
SetCompressor /SOLID zlib
ShowInstDetails nevershow

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "PlainPDF"
VIAddVersionKey "FileDescription" "PlainPDF ${ARCH} Setup"
VIAddVersionKey "FileVersion" "1.0.0"
VIAddVersionKey "ProductVersion" "1.0.0"
VIAddVersionKey "LegalCopyright" "Copyright (c) 2026 Nolan White"

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit
	!if "${ARCH}" == "x64"
		${IfNot} ${IsNativeAMD64}
			MessageBox MB_OK|MB_ICONSTOP "This PlainPDF installer requires x64 Windows."
			Abort
		${EndIf}
	!else if "${ARCH}" == "arm64"
		${IfNot} ${IsNativeARM64}
			MessageBox MB_OK|MB_ICONSTOP "This PlainPDF installer requires ARM64 Windows."
			Abort
		${EndIf}
	!endif
	${IfNot} ${AtLeastWin10}
		MessageBox MB_OK|MB_ICONSTOP "PlainPDF requires Windows 10 22H2 or later."
		Abort
	${EndIf}
	ReadRegStr $0 HKCU "${UNINSTALL_KEY}" "InstallLocation"
	${If} $0 != ""
		StrCpy $INSTDIR $0
	${EndIf}
FunctionEnd

Section "PlainPDF" Main
	SectionIn RO
	SetShellVarContext current
	SetOutPath "$INSTDIR"
	File /r "${STAGE}\*"
	WriteUninstaller "$INSTDIR\Uninstall.exe"
	CreateShortCut "$SMPROGRAMS\${PRODUCT}.lnk" "$INSTDIR\PlainPDF.exe" "" "$INSTDIR\PlainPDF.exe" 0
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT}"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayVersion" "1.0.0"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "Publisher" "Nolan White"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "DisplayIcon" "$INSTDIR\PlainPDF.exe"
	WriteRegStr HKCU "${UNINSTALL_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
	WriteRegStr HKCU "${UNINSTALL_KEY}" "QuietUninstallString" '"$INSTDIR\Uninstall.exe" /S'
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoModify" 1
	WriteRegDWORD HKCU "${UNINSTALL_KEY}" "NoRepair" 1
SectionEnd

Section /o "Offer PlainPDF in Windows Open with" Association
	WriteRegStr HKCU "${PROG_KEY}" "" "PDF document"
	WriteRegStr HKCU "${PROG_KEY}\DefaultIcon" "" "$INSTDIR\PlainPDF.exe,0"
	WriteRegStr HKCU "${PROG_KEY}\shell\open\command" "" '"$INSTDIR\PlainPDF.exe" "%1"'
	WriteRegNone HKCU "${OPEN_WITH}" "${PROG_ID}"
	System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${Main} "Install PlainPDF and its required files for the current user."
	!insertmacro MUI_DESCRIPTION_TEXT ${Association} "List PlainPDF as an Open with option. Your current default PDF application stays selected."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section 'Uninstall'
	SetShellVarContext current
	Delete "$SMPROGRAMS\${PRODUCT}.lnk"
	DeleteRegKey HKCU "${UNINSTALL_KEY}"
	DeleteRegValue HKCU "${OPEN_WITH}" "${PROG_ID}"
	DeleteRegKey HKCU "${PROG_KEY}"
	System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
	!include "${REMOVE_FILES}"
	Delete "$INSTDIR\Uninstall.exe"
	RMDir "$INSTDIR"
SectionEnd
