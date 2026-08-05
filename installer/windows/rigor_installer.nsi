; RIGOR Wallet Windows installer — TASK-WALLET-PROGRAM-025
; Build: makensis /DREPO_ROOT=/path/to/ring installer/windows/rigor_installer.nsi

!include "MUI2.nsh"
!include "x64.nsh"

!ifndef REPO_ROOT
  !define REPO_ROOT "."
!endif
!ifndef STAGING_DIR
  !define STAGING_DIR "${REPO_ROOT}/dist/windows/staging"
!endif
!ifndef OUTFILE
  !define OUTFILE "${REPO_ROOT}/dist/windows/installers/RIGOR-Wallet-0.1.0-testnet.exe"
!endif

Name "RIGOR Wallet (Testnet Candidate)"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\RIGOR Wallet"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
SetDateSave off
Unicode true
CRCCheck force
XPStyle on

!define REGKEY "SOFTWARE\RIGOR Wallet"
!define COMPANY "RIGOR Protocol"
!define URL "https://rigorchain.com"
!define PRODUCT_VERSION "0.1.0-testnet-candidate"

!define MUI_ICON "${REPO_ROOT}/core/share/icons/rigor/rigor.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\rigor-qt.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS ""
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_ABORTWARNING

VIProductVersion 0.1.0.0
VIAddVersionKey ProductName "RIGOR Wallet"
VIAddVersionKey ProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey FileVersion "${PRODUCT_VERSION}"
VIAddVersionKey FileDescription "RIGOR Wallet Testnet Candidate Installer"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

Section "RIGOR Wallet" SEC_MAIN
  SetOutPath "$INSTDIR"
  SetOverwrite on

  File "${STAGING_DIR}\rigor-qt.exe"
  File /nonfatal "${STAGING_DIR}\rigord.exe"
  File /nonfatal "${STAGING_DIR}\rigor-cli.exe"
  File /nonfatal "${STAGING_DIR}\rigor.conf.template"
  File /nonfatal "${STAGING_DIR}\version_manifest.json"
  File /nonfatal "${REPO_ROOT}\core\COPYING"
  File /nonfatal "${REPO_ROOT}\core\doc\README_windows.txt"

!ifdef INCLUDE_QT_RUNTIME
  SetOutPath "$INSTDIR"
  File /nonfatal /r "${STAGING_DIR}\qt-runtime\*.*"
!endif

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\RIGOR Wallet"
  CreateShortcut "$SMPROGRAMS\RIGOR Wallet\RIGOR Wallet.lnk" "$INSTDIR\rigor-qt.exe"
  CreateShortcut "$SMPROGRAMS\RIGOR Wallet\RIGOR Wallet (testnet).lnk" "$INSTDIR\rigor-qt.exe" "-testnet"
  CreateShortcut "$DESKTOP\RIGOR Wallet.lnk" "$INSTDIR\rigor-qt.exe"
  CreateShortcut "$SMPROGRAMS\RIGOR Wallet\Uninstall RIGOR Wallet.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKCU "${REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKCU "${REGKEY}" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWallet" "DisplayName" "RIGOR Wallet"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWallet" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWallet" "DisplayIcon" "$INSTDIR\rigor-qt.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\rigor-qt.exe"
  Delete "$INSTDIR\rigord.exe"
  Delete "$INSTDIR\rigor-cli.exe"
  Delete "$INSTDIR\rigor.conf.template"
  Delete "$INSTDIR\version_manifest.json"
  Delete "$INSTDIR\COPYING"
  Delete "$INSTDIR\README_windows.txt"
!ifdef INCLUDE_QT_RUNTIME
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  Delete "$INSTDIR\*.dll"
!endif
  Delete "$INSTDIR\Uninstall.exe"
  Delete "$SMPROGRAMS\RIGOR Wallet\RIGOR Wallet.lnk"
  Delete "$SMPROGRAMS\RIGOR Wallet\RIGOR Wallet (testnet).lnk"
  Delete "$DESKTOP\RIGOR Wallet.lnk"
  Delete "$SMPROGRAMS\RIGOR Wallet\Uninstall RIGOR Wallet.lnk"
  RMDir "$SMPROGRAMS\RIGOR Wallet"
  RMDir "$INSTDIR"
  DeleteRegKey HKCU "${REGKEY}"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWallet"
SectionEnd

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "64-bit Windows required."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Function .onInstSuccess
  ; First-run launcher hook: wallet shows FirstRunWizard on first launch
  WriteRegStr HKCU "Software\RigorWallet\firstrun" "pending" "1"
FunctionEnd
