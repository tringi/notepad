#include <Windows.h>
#include <objbase.h>
#include <ShellAPI.h>
#include <Shobjidl.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <DwmAPI.h>
#include <io.h>
#include <fcntl.h>

#include <VersionHelpers.h>

#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"
#include "Notepad_File.hpp"

#include "Windows\Windows_GetFullPath.hpp"
#include "Windows\Windows_AccessibilityTextScale.hpp"

#include <cwchar>
#include <vector>
#include <set>

const wchar_t * szVersionInfo [8] = {};
wchar_t szTmpPathBuffer [MAX_NT_PATH];
Windows::TextScale scale;
HHOOK hHook = NULL;
ITaskbarList3 * taskbar = nullptr;

ATOM InitializeGUI (HINSTANCE);
bool InitVersionInfoStrings (HINSTANCE);
DWORD WINAPI ApplicationRecoveryCallback (PVOID);

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

bool AskInstancesForOpenFile (File * file, std::map <DWORD, HWND> * instances) {
    struct Trampoline {
        File * file;
        std::map <DWORD, HWND> * instances;
    } p = { file, instances };

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
                                        SetForegroundWindow (hWnd);
                                        return FALSE;
                                    }
                                }

                                DWORD pid;
                                if (GetWindowThreadProcessId (hWnd, &pid)) {
                                    try {
                                        trampoline->instances->insert ({ pid, hWnd });
                                    } catch (...) {
                                        // ignore failures to insert
                                    }
                                }
                            }
                            return TRUE;
                        }, (LPARAM) &p) == FALSE;
}

BOOL CloseHandleEx (HANDLE hProcess, HANDLE hHandle) {
    HANDLE hCleanup = NULL;
    if (DuplicateHandle (hProcess, hHandle, GetCurrentProcess (), &hCleanup, 0, FALSE, DUPLICATE_CLOSE_SOURCE)) {
        CloseHandle (hCleanup);
        return TRUE;
    } else
        return FALSE;
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

int CALLBACK wWinMain (_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    SetLastError (0);
    if (Window::InitAtom (hInstance)) {

        // this should have the effect of allowing one previously admin/elevated instance
        // to open another file, which would need entering credentials again, probably

        ChangeWindowMessageFilter (WM_COPYDATA, MSGFLT_ADD);

        // TODO: consider supporting multiple files

        /*auto argc = 0;
        auto args = CommandLineToArgvW (lpCmdLine, &argc);

        std::printf ("ARGS:\n");
        for (auto i = 0; i != argc; ++i) {
            wchar_t full [32768];
            Windows::GetFullPath (args [i], 32768, full, nullptr);
            std::wprintf (L"%s\n", full);
        }
        std::printf ("\n");// */

        // request to restore

        if (lpCmdLine [0] == L'?' && lpCmdLine [1] == L'\0') {

            // TODO: check registry for open file names, and (optional) diff buffers

            return ERROR_CALL_NOT_IMPLEMENTED;
        }

        // remove quotes (if any)

        if (lpCmdLine [0] == L'"') {
            lpCmdLine++;
            if (auto * end = std::wcschr (lpCmdLine, L'"')) {
                *end = L'\0';
            }
        }

        File file;

        if (lpCmdLine [0]) {
            if (file.init (lpCmdLine)) {

                // ask other instances/windows if they have this file open
                // if one answers yes: activates the window and return true

                std::map <DWORD, HWND> instances;

                if (AskInstancesForOpenFile (&file, &instances))
                    return ERROR_SUCCESS;

                // if there is at least one instance running, ask it to open the file

                if (instances.size ()) {
                    if (InitVersionInfoStrings (hInstance)) {

                        Settings::Init ();
                        if (Settings::Get (L"Single Instance", Settings::Defaults::SingleInstance)) {

                            for (auto [pid, hWnd] : instances) {
                                if (AskInstanceToOpenFile (pid, hWnd, file.handle, nCmdShow))
                                    return ERROR_SUCCESS;
                            }
                        }
                    }
                }
            } else {
                switch (auto error = GetLastError ()) {
                    case ERROR_FILE_NOT_FOUND:
                        // does not exists, create?
                        break;

                    case ERROR_SHARING_VIOLATION:
                        // some other app has the file already open exclusively
                        break;

                    case ERROR_ACCESS_DENIED:
                        // not enought access rights? or the file is pending to be deleted
                        break;

                    default:
                        ; // messagebox
                }
            }
        }

        // if not empty, get file ID 
        //   - file may not exist, ask to create
        //   - have file size, ask other instances if they can map it (if not, reuse this one)

        if (InitVersionInfoStrings (hInstance)) {

            Settings::Init ();
            if (Settings::Get (L"Single Instance", Settings::Defaults::SingleInstance)) {

                if (AskInstancesToOpenWindow (nCmdShow))
                    return ERROR_SUCCESS;
            }

            if (InitializeGUI (hInstance)
                    && scale.initialize ()) {

                if (SUCCEEDED (CoCreateInstance (CLSID_TaskbarList, NULL, CLSCTX_ALL, IID_ITaskbarList3, (void **) &taskbar))) {
                    if (!SUCCEEDED (taskbar->HrInit ())) {
                        taskbar->Release ();
                        taskbar = nullptr;
                    }
                }

                RegisterApplicationRestart (L"?", 0);
                //RegisterApplicationRecoveryCallback (

                const auto hAccelerators = LoadAccelerators (hInstance, MAKEINTRESOURCE (1));
                const auto hDarkMenuAccelerators = LoadAccelerators (hInstance, MAKEINTRESOURCE (2));

                hHook = SetWindowsHookEx (WH_CALLWNDPROC,
                                          [] (int code, WPARAM wParam, LPARAM lParam) -> LRESULT {
                                              if (code == HC_ACTION) {
                                                  auto cwp = reinterpret_cast <const CWPSTRUCT *> (lParam);
                                                  switch (cwp->message) {
                                                      case WM_KEYDOWN:
                                                          if (Window::dark_menu_tracking != nullptr) {
                                                              if (GetClassWord (cwp->hwnd, GCW_ATOM) == 32768) { // menu window class ID
                                                                  Window::dark_menu_tracking->OnTrackedMenuKey (cwp->hwnd, cwp->wParam);
                                                              }
                                                          }
                                                  }
                                              }
                                              return CallNextHookEx (hHook, code, wParam, lParam);
                                          },
                                          NULL, GetCurrentThreadId ());

                new Window (nCmdShow, std::move (file));

                MSG message {};
                const HANDLE handles [] = {
                    scale.GetEventHandle ()
                };
                constexpr DWORD nHandles = sizeof handles / sizeof handles [0];

                DWORD mwmoFlags = 0x04FFu;
                if (IsWindows8OrGreater ()) {
                    mwmoFlags |= QS_TOUCH | QS_POINTER;
                }

                do
                switch (MsgWaitForMultipleObjectsEx (nHandles, handles, INFINITE, mwmoFlags, 0)) {
                    case WAIT_OBJECT_0 + 0:
                        if (scale.OnEvent ()) {
                            EnumThreadWindows (GetCurrentThreadId (),
                                               [] (HWND hWnd, LPARAM lParam) {
                                                   PostMessage (hWnd, Window::Message::UpdateFontSize, 0, 0);
                                                   return TRUE;
                                               }, NULL);
                        }
                        break;

                    case WAIT_OBJECT_0 + nHandles:
                        while (PeekMessage (&message, NULL, 0u, 0u, PM_REMOVE)) {
                            if (message.message == WM_QUIT) {
                                break;
                            }
                            switch (message.message) {
                                case WM_SYSKEYDOWN:
                                    switch (message.wParam) {
                                        case VK_MENU:
                                            if (auto root = GetAncestor (message.hwnd, GA_ROOT)) {
                                                PostMessage (root, Window::Message::ShowAccelerators, 0, 0);
                                            }
                                            break;
                                    }
                            }

                            if (message.hwnd) {
                                auto root = GetAncestor (message.hwnd, GA_ROOT);

                                if (Window::GetPresentation ().dark) {
                                    if (TranslateAccelerator (root, hDarkMenuAccelerators, &message))
                                        continue;
                                }
                                if (TranslateAccelerator (root, hAccelerators, &message))
                                    continue;

                            } else {
                                switch (message.message) {
                                    case WM_COMMAND:
                                        switch (message.wParam) {
                                            case 0xCF:
                                                EnumThreadWindows (GetCurrentThreadId (),
                                                                   [] (HWND hWnd, LPARAM lParam) {
                                                                       PostMessage (hWnd, WM_CLOSE, 0, 0);
                                                                       return TRUE;
                                                                   }, NULL);
                                                break;
                                        }
                                        break;

                                    case Window::Message::UpdateSettings:

                                        break;
                                }
                            }
                            TranslateMessage (&message);
                            DispatchMessage (&message);
                        }
                        break;

                    default:
                    case WAIT_FAILED:
                        return (int) GetLastError ();

                } while (message.message != WM_QUIT);

                taskbar->Release ();
                scale.terminate ();

                CoUninitialize ();
                return (int) message.wParam;
            }
        }
    }
    return (int) GetLastError ();
}

bool InitVersionInfoStrings (HINSTANCE hInstance) {
    if (szVersionInfo [0])
        return true;

    if (HRSRC hRsrc = FindResource (hInstance, MAKEINTRESOURCE (1), RT_VERSION)) {
        if (HGLOBAL hGlobal = LoadResource (hInstance, hRsrc)) {
            auto data = LockResource (hGlobal);
            auto size = SizeofResource (hInstance, hRsrc);

            if (data && (size >= 92)) {
                struct Header {
                    WORD wLength;
                    WORD wValueLength;
                    WORD wType;
                };

                // StringFileInfo
                //  - not searching, leap of faith that the layout is stable

                auto pdata = static_cast <const unsigned char *> (data) + 76;
                auto h = *reinterpret_cast <const Header *> (data);
                auto pstrings = pdata + h.wValueLength;
                auto p = reinterpret_cast <const wchar_t *> (pstrings) + 12;
                auto e = p + reinterpret_cast <const Header *> (pstrings)->wLength / 2 - 12;
                auto i = 0u;

                const Header * header = nullptr;
                do {
                    header = reinterpret_cast <const Header *> (p);
                    auto length = header->wLength / 2;

                    if (header->wValueLength) {
                        szVersionInfo [i++] = p + length - header->wValueLength;
                    } else {
                        szVersionInfo [i++] = L"";
                    }

                    p += length;
                    if (length % 2) {
                        ++p;
                    }
                } while ((p < e) && (i < sizeof szVersionInfo / sizeof szVersionInfo [0]) && header->wLength);

                if (i == sizeof szVersionInfo / sizeof szVersionInfo [0])
                    return true;
            }
        }
    }
    return false;
}

ATOM InitializeGUI (HINSTANCE hInstance) {
    const INITCOMMONCONTROLSEX classes = {
        sizeof (INITCOMMONCONTROLSEX),
        ICC_STANDARD_CLASSES | ICC_COOL_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES | ICC_LINK_CLASS
    };

    InitCommonControls ();
    if (!InitCommonControlsEx (&classes))
        return false;

    switch (CoInitializeEx (NULL, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)) {
        case S_OK:
        case S_FALSE: // already initialized
            break;
        default:
            return false;
    }//*/

    Windows::WindowPresentation::Initialize ();
    //RegisterWindowMessage (L"TaskbarCreated");
    return Window::Initialize (hInstance);
}

DWORD WINAPI ApplicationRecoveryCallback (PVOID) {
    BOOL cancelled = FALSE;
    ApplicationRecoveryInProgress (&cancelled);

    // dump diff data for all open files, and set open file IDs to registry, for recovery
    // TODO: do the same for all open files, as each window does for WM_QUERYENDSESSION

    ApplicationRecoveryFinished (TRUE); // success?
    return 0;
}

