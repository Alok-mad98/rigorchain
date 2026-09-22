/* Mainnet GUI launcher. Starts internal\rigor-qt.exe with a portable datadir.
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
        MessageBoxW(NULL, L"Cannot resolve launcher path.", L"Rigor Wallet", MB_ICONERROR);
        return 1;
    }
    cut = wcsrchr(dir, L'\\');
    if (!cut) {
        MessageBoxW(NULL, L"Cannot resolve install directory.", L"Rigor Wallet", MB_ICONERROR);
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
                    L"Wallet GUI is missing. Use rigor-wallet.exe from the install folder.",
                    L"Rigor Wallet", MB_ICONERROR);
        return 1;
    }

    if (_snwprintf(datadir, MAX_PATH, L"%sdata", dir) < 0) {
        return 1;
    }
    CreateDirectoryW(datadir, NULL);

    _snwprintf(cmdline, 32768,
               L"\"%s\" -server -datadir=\"%s\" -rpcallowip=127.0.0.1",
               exe, datadir);

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = (WORD)show;

    if (!CreateProcessW(exe, cmdline, NULL, NULL, FALSE, 0, NULL, dir, &si, &pi)) {
        MessageBoxW(NULL, L"Failed to start the Rigor Wallet GUI.", L"Rigor Wallet", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
