; RIGOR Windows SOLO miner installer (not the wallet package).
; Output: dist/alpha/windows/rigor-v0.2-miner-windows-x86_64.exe
; Build: bash scripts/alpha/package_windows_miner.sh

!include "MUI2.nsh"
!include "x64.nsh"

!ifndef REPO_ROOT
  !define REPO_ROOT "."
!endif
!ifndef STAGING_DIR
  !define STAGING_DIR "${REPO_ROOT}/dist/alpha/windows/miner-staging"
!endif
!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.2"
!endif
!ifndef OUTFILE
  !define OUTFILE "${REPO_ROOT}/dist/alpha/windows/rigor-v0.2-miner-windows-x86_64.exe"
!endif

Name "Rigor Miner"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\Rigor Miner"
RequestExecutionLevel admin
SetCompressor lzma
Unicode true
CRCCheck on

!define REGKEY "SOFTWARE\Rigor Miner"
!define COMPANY "Rigor Protocol"
BrandingText "RIGOR Miner v${PRODUCT_VERSION}"

!define MUI_ICON "${REPO_ROOT}/core/share/icons/rigor/rigor.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\rigor-miner.cmd"
!define MUI_ABORTWARNING

VIProductVersion 0.2.0.0
VIAddVersionKey ProductName "Rigor Miner"
VIAddVersionKey ProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey FileVersion "${PRODUCT_VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey FileDescription "Rigor SOLO miner + local dashboard (unsigned)"
VIAddVersionKey LegalCopyright "Copyright (C) 2026 Rigor contributors"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPO_ROOT}/installer/windows/RIGOR_LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Rigor Miner requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Rigor Miner (required)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite on

  File "${STAGING_DIR}/rigor-miner.cmd"
  File "${STAGING_DIR}/start-miner-dashboard.cmd"
  File "${STAGING_DIR}/load-rpc-auth.cmd"
  File "${STAGING_DIR}/gen-rpc-auth.ps1"
  File "${STAGING_DIR}/README.txt"
  File /oname=LICENSE.txt "${REPO_ROOT}/installer/windows/RIGOR_LICENSE.txt"
  !include "${STAGING_DIR}/optional-binaries.nsh"

  SetOutPath "$INSTDIR\dashboard"
  File /r "${STAGING_DIR}/dashboard/"

  WriteUninstaller "$INSTDIR\Uninstall.exe"
  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\Rigor Miner"
  CreateShortcut "$SMPROGRAMS\Rigor Miner\Rigor Miner.lnk" "$INSTDIR\rigor-miner.cmd"
  CreateShortcut "$SMPROGRAMS\Rigor Miner\Miner Dashboard.lnk" "$INSTDIR\start-miner-dashboard.cmd"
  CreateShortcut "$SMPROGRAMS\Rigor Miner\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKLM "${REGKEY}" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorMiner" "DisplayName" "Rigor Miner"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorMiner" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorMiner" "Publisher" "${COMPANY}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorMiner" "DisplayVersion" "${PRODUCT_VERSION}"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\Rigor Miner.lnk" "$INSTDIR\rigor-miner.cmd"
  CreateShortcut "$DESKTOP\RIGOR Miner Dashboard.lnk" "$INSTDIR\start-miner-dashboard.cmd"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\rigor-miner.exe"
  Delete "$INSTDIR\rigor-cli.exe"
  Delete "$INSTDIR\rigor-miner.cmd"
  Delete "$INSTDIR\start-miner-dashboard.cmd"
  Delete "$INSTDIR\load-rpc-auth.cmd"
  Delete "$INSTDIR\gen-rpc-auth.ps1"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\dashboard"
  RMDir /r "$SMPROGRAMS\Rigor Miner"
  Delete "$DESKTOP\Rigor Miner.lnk"
  Delete "$DESKTOP\RIGOR Miner Dashboard.lnk"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${REGKEY}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorMiner"
SectionEnd
