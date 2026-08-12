; RIGOR Windows Alpha installer
; Output: dist/alpha/windows/rigor-wallet-0.1.0-alpha.exe
; Build: bash scripts/alpha/build_alpha_windows_nsis.sh

!include "MUI2.nsh"
!include "x64.nsh"
!include "FileFunc.nsh"

!ifndef REPO_ROOT
  !define REPO_ROOT "."
!endif
!ifndef STAGING_DIR
  !define STAGING_DIR "${REPO_ROOT}/dist/alpha/windows/staging"
!endif
!ifndef PRODUCT_VERSION
  !define PRODUCT_VERSION "0.1.0-alpha.1"
!endif
!ifndef OUTFILE
  !define OUTFILE "${REPO_ROOT}/dist/alpha/windows/rigor-wallet-0.1.0-alpha.exe"
!endif

Name "RIGOR Wallet Alpha"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\RIGOR Alpha"
RequestExecutionLevel admin
; Non-solid lzma: /SOLID on Linux makensis has produced CRC/"NSIS Error" installers
; with large static Qt PE payloads (~50–80 MiB). Per-file lzma is more reliable cross-build.
SetCompressor lzma
Unicode true
CRCCheck on

!define REGKEY "SOFTWARE\RIGOR Alpha"
!define COMPANY "RIGOR Protocol"
!define DATADIR_HINT "$APPDATA\RIGOR"

!define MUI_ICON "${REPO_ROOT}/core/share/icons/rigor/rigor.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${REPO_ROOT}/core/share/pixmaps/nsis-header.bmp"
!define MUI_FINISHPAGE_RUN "$INSTDIR\rigor-wallet.exe"
!define MUI_FINISHPAGE_RUN_PARAMETERS ""
!define MUI_ABORTWARNING

VIProductVersion 0.1.0.0
VIAddVersionKey ProductName "RIGOR Wallet Alpha"
VIAddVersionKey ProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey CompanyName "${COMPANY}"
VIAddVersionKey FileDescription "RIGOR Wallet Alpha Installer (unsigned)"

!insertmacro MUI_PAGE_WELCOME
; Forward slashes: makensis on Linux/WSL cannot open Windows-style backslashes.
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
    MessageBox MB_OK|MB_ICONSTOP "RIGOR Alpha requires 64-bit Windows."
    Abort
  ${EndIf}
  SetRegView 64
FunctionEnd

Section "RIGOR Alpha (required)" SEC_MAIN
  SectionIn RO
  SetOutPath "$INSTDIR"
  SetOverwrite on

  ; Product PE binaries (must be real PE32+)
  File "${STAGING_DIR}/rigor-wallet.exe"
  File "${STAGING_DIR}/rigord.exe"
  File "${STAGING_DIR}/rigor-miner.exe"
  File /nonfatal "${STAGING_DIR}/rigor-miner.cmd"
  File /nonfatal "${STAGING_DIR}/rigor-cli.exe"
  File /nonfatal "${STAGING_DIR}/rigor-cli.cmd"
  File /nonfatal "${STAGING_DIR}/README-ALPHA.txt"
  File /oname=LICENSE.txt "${REPO_ROOT}/installer/windows/RIGOR_LICENSE.txt"

  ; Config template + datadir guidance
  SetOutPath "$INSTDIR\config"
  File /nonfatal "${STAGING_DIR}/rigor.conf.template"
  File /nonfatal "${STAGING_DIR}/DATADIR.txt"

  ; Qt runtime (windeployqt) when staged
!ifdef INCLUDE_QT_RUNTIME
  SetOutPath "$INSTDIR"
  File /nonfatal /r "${STAGING_DIR}/qt-runtime/"
!endif

  ; Default data directory (created empty; wallet/node create chainstate on first run)
  CreateDirectory "${DATADIR_HINT}"
  CreateDirectory "${DATADIR_HINT}\testnet3"
  CreateDirectory "${DATADIR_HINT}\regtest"

  WriteUninstaller "$INSTDIR\Uninstall.exe"

  CreateDirectory "$SMPROGRAMS\RIGOR Alpha"
  CreateShortcut "$SMPROGRAMS\RIGOR Alpha\RIGOR Wallet.lnk" "$INSTDIR\rigor-wallet.exe"
  CreateShortcut "$SMPROGRAMS\RIGOR Alpha\RIGOR Node.lnk" "$INSTDIR\rigord.exe"
  ; Launch via .cmd so the console stays open (miner is CLI, not a GUI).
  CreateShortcut "$SMPROGRAMS\RIGOR Alpha\RIGOR Miner.lnk" "$INSTDIR\rigor-miner.cmd"
  CreateShortcut "$SMPROGRAMS\RIGOR Alpha\RIGOR CLI.lnk" "$INSTDIR\rigor-cli.cmd"
  CreateShortcut "$SMPROGRAMS\RIGOR Alpha\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${REGKEY}" "Path" "$INSTDIR"
  WriteRegStr HKLM "${REGKEY}" "Version" "${PRODUCT_VERSION}"
  WriteRegStr HKLM "${REGKEY}" "DataDir" "${DATADIR_HINT}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha" "DisplayName" "RIGOR Wallet Alpha"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha" "UninstallString" "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha" "DisplayIcon" "$INSTDIR\rigor-wallet.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha" "Publisher" "${COMPANY}"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha" "DisplayVersion" "${PRODUCT_VERSION}"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
  CreateShortcut "$DESKTOP\RIGOR Wallet.lnk" "$INSTDIR\rigor-wallet.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\rigor-wallet.exe"
  Delete "$INSTDIR\rigord.exe"
  Delete "$INSTDIR\rigor-miner.exe"
  Delete "$INSTDIR\rigor-miner.cmd"
  Delete "$INSTDIR\rigor-cli.exe"
  Delete "$INSTDIR\rigor-cli.cmd"
  Delete "$INSTDIR\README-ALPHA.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\COPYING"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir /r "$INSTDIR\config"
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\qt-runtime"
  Delete "$INSTDIR\*.dll"
  RMDir /r "$SMPROGRAMS\RIGOR Alpha"
  Delete "$DESKTOP\RIGOR Wallet.lnk"
  DeleteRegKey HKLM "${REGKEY}"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\RigorAlpha"
  RMDir "$INSTDIR"
  ; Datadir under %APPDATA%\RIGOR is retained (user funds). Remove manually if desired.
SectionEnd

LangString DESC_SEC_MAIN ${LANG_ENGLISH} "RIGOR Wallet, Node, and Miner binaries."
LangString DESC_SEC_DESKTOP ${LANG_ENGLISH} "Optional desktop shortcut for RIGOR Wallet."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} $(DESC_SEC_MAIN)
  !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} $(DESC_SEC_DESKTOP)
!insertmacro MUI_FUNCTION_DESCRIPTION_END
