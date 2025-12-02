#include "Notepad_IPC.hpp"
#include "Notepad_Window.hpp"

namespace {
    std::uint8_t ForwardProperShowStyle (std::uintptr_t nCmdShow);
    BOOL CloseHandleEx (HANDLE hProcess, HANDLE hHandle);
}

bool AskInstancesForOpenFile (HWND hAbove, File * file, std::map <DWORD, HWND> * instances) {
    struct Trampoline {
        HWND hAbove;
        File * file;
        std::map <DWORD, HWND> * instances;
    } p = { hAbove, file, instances };

    return EnumWindows ([] (HWND hWnd, LPARAM p) {
                            if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {
                                auto trampoline = (Trampoline *) p;

                                COPYDATASTRUCT data = {
                                    Window::CopyCode::OpenFileCheck,
                                    sizeof (FILE_ID_INFO),
                                    (void *) &trampoline->file->id
                                };

                                DWORD_PTR result = 0;
                                if (SendMessageTimeout (hWnd, WM_COPYDATA, NULL, (LPARAM) &data,
                                                        SMTO_NOTIMEOUTIFNOTHUNG | SMTO_ERRORONEXIT | SMTO_BLOCK,
                                                        1000, &result)) {
                                    if (result) {
                                        if (IsIconic (hWnd)) {
                                            OpenIcon (hWnd);
                                        }
                                        if (trampoline->hAbove) {
                                            SetWindowPos (hWnd, trampoline->hAbove, 0, 0, 0, 0,
                                                          SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                                        } else {
                                            SetForegroundWindow (hWnd);
                                        }
                                        return FALSE;
                                    }
                                }

                                if (trampoline->instances) {
                                    DWORD pid;
                                    if (GetWindowThreadProcessId (hWnd, &pid)) {
                                        try {
                                            trampoline->instances->insert ({ pid, hWnd });
                                        } catch (...) {
                                            // ignore failures to insert
                                        }
                                    }
                                }
                            }
                            return TRUE;
                        }, (LPARAM) &p) == FALSE;
}

bool AskInstanceToOpenFile (DWORD pid, HWND hWnd, HANDLE hFile, int nCmdShow) {
    if (HANDLE hPeer = OpenProcess (PROCESS_DUP_HANDLE, FALSE, pid)) {
        HANDLE hPeerFile = NULL;

        if (DuplicateHandle (GetCurrentProcess (), hFile, hPeer, &hPeerFile, 0, FALSE, DUPLICATE_SAME_ACCESS)) {

            Window::OpenFileRequest request = {};
            request.handle = (std::intptr_t) hPeerFile;
            request.nCmdShow = ForwardProperShowStyle (nCmdShow);

            COPYDATASTRUCT data = {
                Window::CopyCode::OpenFile,
                sizeof request, &request
            };

            if (auto hPeerWindow = (HWND) SendMessage (hWnd, WM_COPYDATA, NULL, (LPARAM) &data)) {
                SetForegroundWindow (hPeerWindow);
                return true;
            }

            // on failure, cleanup the handle we copied into the other process, to not lock the file

            CloseHandleEx (hPeer, hPeerFile);
        }
    }
    return false;
}

bool AskInstancesToOpenWindow (int nCmdShow) {
    return EnumWindows ([] (HWND hWnd, LPARAM nCmdShow) {
                            if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {

                                Window::OpenFileRequest request = {};
                                request.nCmdShow = ForwardProperShowStyle (nCmdShow);

                                COPYDATASTRUCT data = {
                                    Window::CopyCode::OpenFile,
                                    sizeof request, &request
                                };

                                DWORD pid;
                                if (GetWindowThreadProcessId (hWnd, &pid)) {
                                    AllowSetForegroundWindow (pid);
                                }

                                DWORD_PTR result = 0;
                                if (SendMessageTimeout (hWnd, WM_COPYDATA, NULL, (LPARAM) &data,
                                                        SMTO_NOTIMEOUTIFNOTHUNG | SMTO_ERRORONEXIT | SMTO_BLOCK,
                                                        1000, &result)) {
                                    if (result)
                                        return FALSE;
                                }
                            }
                            return TRUE;
                        }, (LPARAM) nCmdShow) == FALSE;
}

namespace {
    std::uint8_t ForwardProperShowStyle (std::uintptr_t nCmdShow) {
        if (nCmdShow == SW_SHOWDEFAULT) {
            STARTUPINFO si {};
            si.cb = sizeof si;
            GetStartupInfo (&si);
            if (si.dwFlags & STARTF_USESHOWWINDOW) {
                nCmdShow = si.wShowWindow;
            } else {
                nCmdShow = SW_SHOWNORMAL;
            }
        }
        if (nCmdShow > SW_MAX) {
            nCmdShow = SW_SHOWNORMAL;
        }
        return (std::uint8_t) nCmdShow;
    }

    // CloseHandleEx
    //  - closes 'handle' in 'process'
    //  - 'process' must be open with PROCESS_DUP_HANDLE access rights
    //  - 'handle' must be valid handle value in 'process', local handle of the same value is unaffected
    //
    BOOL CloseHandleEx (HANDLE process, HANDLE handle) {
        HANDLE cleanup = NULL;
        if (DuplicateHandle (process, handle, GetCurrentProcess (), &cleanup, 0, FALSE, DUPLICATE_CLOSE_SOURCE)) {
            CloseHandle (cleanup);
            return TRUE;
        } else
            return FALSE;
    }
}