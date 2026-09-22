; RIGOR Windows internal testnet wallet installer (v0.2).
; Network selection only: shortcuts and finish-run pass -testnet.
; Does not change consensus, RFC, Coq, or packed CLIENT_VERSION.

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!ifndef REPO_ROOT
  !define REPO_ROOT "."
!endif
!ifndef STAGING_DIR
  !define STAGING_DIR "${REPO_ROOT}/dist/testnet_packaging/windows/staging"
!endif
!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.2"
!endif
!ifndef OUTFILE
  !define OUTFILE "${REPO_ROOT}/dist/testnet_packaging/rigor-wallet-v0.2-testnet-windows.exe"
!endif

Name "Rigor Wallet Testnet"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\Rigor Wallet Testnet"
RequestExecutionLevel admin
SetCompressor lzma
Unicode true
CRCCheck on

!define REGKEY "SOFTWARE\Rigor Wallet Testnet"
!define COMPANY "Rigor Protocol"
!define DATADIR_HINT "$APPDATA\RIGOR"
BrandingText "RIGOR v${PRODUCT_VERSION} testnet"

!define MUI_ICON "${REPO_ROOT}/core/share/icons/rigor/rigor.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\rigor-wallet.exe"
!define MUI_ABORTWARNING

VIProductVersion 0.2.0.0
VIAddVersionKey ProductName "Rigor Wallet Testnet"
VIAddVersionKey ProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey FileVersion "${PRODUCT_VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey FileDescription "Rigor Wallet v${PRODUCT_VERSION} Testnet Installer (unsigned)"
VIAddVersionKey LegalCopyright "Copyright (C) 2026 Rigor contributors"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${REPO_ROOT}/installer/windows/RIGOR_LICENSE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE English

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "Rigor Wallet Testnet requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "Rigor Wallet Testnet (required)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite on

  ClearErrors
  IfFileExists "$INSTDIR\rigor-wallet.exe" 0 skip_wallet_lock_check
    Delete "$INSTDIR\rigor-wallet.exe"
    IfErrors 0 skip_wallet_lock_check
      MessageBox MB_OK|MB_ICONSTOP "Cannot replace rigor-wallet.exe. Quit Rigor Wallet Testnet completely (including the tray icon) and run setup again."
      Abort
  skip_wallet_lock_check:
  Delete "$INSTDIR\rigor-miner.exe"
  Delete "$INSTDIR\rigor-miner.cmd"
  Delete "$INSTDIR\minerd.exe"
  Delete "$INSTDIR\rigor-qt.exe"

  File "${STAGING_DIR}/rigor-wallet.exe"
  File "${STAGING_DIR}/rigord.exe"
  File "${STAGING_DIR}/rigor-cli.exe"
  File "${STAGING_DIR}/rigor.ico"
  File /oname=README.txt "${STAGING_DIR}/README.txt"
  File /oname=LICENSE.txt "${REPO_ROOT}/installer/windows/RIGOR_LICENSE.txt"
  File "${STAGING_DIR}/start-wallet.cmd"
  File "${STAGING_DIR}/rigor-cli-testnet.cmd"

  SetOutPath "$INSTDIR\internal"
  File "${STAGING_DIR}/internal/rigor-qt.exe"

  SetOutPath "$INSTDIR\config"
  File "${STAGING_DIR}/rigor.conf.template"
  File "${STAGING_DIR}/DATADIR.txt"

  CreateDirectory "$INSTDIR\data"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  SetOutPath "$INSTDIR"
  CreateDirectory "$SMPROGRAMS\Rigor Wallet Testnet"
  CreateShortcut "$SMPROGRAMS\Rigor Wallet Testnet\Rigor Wallet Testnet.lnk" "$INSTDIR\rigor-wallet.exe" "" "$INSTDIR\rigor.ico" 0
  CreateShortcut "$SMPROGRAMS\Rigor Wallet Testnet\Rigor CLI Testnet.lnk" "$INSTDIR\rigor-cli-testnet.cmd"
  CreateShortcut "$SMPROGRAMS\Rigor Wallet Testnet\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKLM "${REGKEY}" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${REGKEY}" "DataDir" "$INSTDIR\data"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet" "DisplayName" "Rigor Wallet Testnet"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet" "DisplayIcon" "$INSTDIR\rigor.ico"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet" "Publisher" "${COMPANY}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet" "DisplayVersion" "${PRODUCT_VERSION}"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  SetOutPath "$INSTDIR"
  CreateShortcut "$DESKTOP\Rigor Wallet Testnet.lnk" "$INSTDIR\rigor-wallet.exe" "" "$INSTDIR\rigor.ico" 0
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\rigor-wallet.exe"
  Delete "$INSTDIR\rigor-qt.exe"
  RMDir /r "$INSTDIR\internal"
  Delete "$INSTDIR\rigord.exe"
  Delete "$INSTDIR\rigor-cli.exe"
  Delete "$INSTDIR\rigor.ico"
  Delete "$INSTDIR\start-wallet.cmd"
  Delete "$INSTDIR\rigor-cli-testnet.cmd"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\config"
  RMDir /r "$SMPROGRAMS\Rigor Wallet Testnet"
  Delete "$DESKTOP\Rigor Wallet Testnet.lnk"
  DeleteRegKey HKLM "${REGKEY}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorWalletTestnet"
  RMDir "$INSTDIR"
SectionEnd

LangString DESC_SEC_MAIN ${LANG_ENGLISH} "Rigor Wallet Testnet, node, and CLI. Shortcuts start with -testnet. Mining software is not included."
LangString DESC_SEC_DESKTOP ${LANG_ENGLISH} "Optional desktop shortcut for Rigor Wallet Testnet."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} $(DESC_SEC_MAIN)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} $(DESC_SEC_DESKTOP)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
