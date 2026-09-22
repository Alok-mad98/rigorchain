/* Testnet-only GUI launcher. Always starts rigor-qt.exe with -testnet.
 * Packaging only; does not change consensus or wallet logic. */
#define UNICODE
#define _UNICODE
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WCHAR dir[MAX_PATH];
    WCHAR exe[MAX_PATH];
    WCHAR datadir[MAX_PATH];
    WCHAR cmdline[32768];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    WCHAR *cut;

    (void)inst;
    (void)prev;
    (void)cmd;

    if (!GetModuleFileNameW(NULL, dir, MAX_PATH)) {
        MessageBoxW(NULL, L"Cannot resolve launcher path.", L"Rigor Wallet Testnet", MB_ICONERROR);
        return 1;
    }
    cut = wcsrchr(dir, L'\\');
    if (!cut) {
        MessageBoxW(NULL, L"Cannot resolve install directory.", L"Rigor Wallet Testnet", MB_ICONERROR);
        return 1;
    }
    cut[1] = 0;
    if (_snwprintf(exe, MAX_PATH, L"%sinternal\\rigor-qt.exe", dir) < 0) {
        return 1;
    }
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        if (_snwprintf(exe, MAX_PATH, L"%srigor-qt.exe", dir) < 0) {
            return 1;
        }
    }
    if (GetFileAttributesW(exe) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxW(NULL,
                    L"TESTNET GUI is missing. Use Rigor Wallet Testnet / start-wallet.cmd.",
                    L"Rigor Wallet Testnet", MB_ICONERROR);
        return 1;
    }

    /* Portable data dir next to this launcher (Litecoin-style -datadir=%~dp0).
     * Ignores QSettings/registry strDataDir from a previous install path. */
    if (_snwprintf(datadir, MAX_PATH, L"%sdata", dir) < 0) {
        return 1;
    }
    CreateDirectoryW(datadir, NULL);

    /* -server turns on localhost RPC so SOLO miners can connect on 18761.
     * Public TESTNET P2P peers (already listed on the official download page). */
    _snwprintf(cmdline, 32768,
               L"\"%s\" -testnet -server -datadir=\"%s\" -rpcallowip=127.0.0.1 "
               L"-addnode=seed1.testnet.rigor.network:18760 -addnode=seed2.testnet.rigor.network:18760",
               exe, datadir);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = (WORD)show;

    if (!CreateProcessW(exe, cmdline, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"Failed to start rigor-qt.exe -testnet.", L"Rigor Wallet Testnet", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
