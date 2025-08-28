#include <Windows.h>
#include <objbase.h>
#include <ShellAPI.h>
#include <CommCtrl.h>
#include <Uxtheme.h>
#include <DwmAPI.h>
#include <io.h>
#include <fcntl.h>

#include <VersionHelpers.h>

#include "Notepad_Window.hpp"
#include "Windows\Windows_FileID.hpp"
#include "Windows\Windows_GetFullPath.hpp"
#include "Windows\Windows_SetThreadName.hpp"
#include "Windows\Windows_AccessibilityTextScale.hpp"

#include <list>
#include <cwchar>

const wchar_t * szVersionInfo [9];
Windows::TextScale scale;
HHOOK hHook = NULL;

ATOM InitializeGUI (HINSTANCE);
bool InitVersionInfoStrings (HINSTANCE);

int CALLBACK wWinMain (_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    Windows::SetThreadName (L"GUI");

    SetLastError (0);
    if (Window::InitAtom (hInstance)) {

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

        if (lpCmdLine [0]) {
            // TODO: support passing file id like: <volume:id-bytes>

            //RegisterApplicationRecoveryCallback;

            // test:
            //File f;
            //f.Open (lpCmdLine, OPEN_ALWAYS);


            Windows::FileID id (lpCmdLine); // TODO: merge with 'Window'
            if (id.valid ()) {

                if (EnumWindows ([] (HWND hWnd, LPARAM pFileId) {
                                    if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {
                                        COPYDATASTRUCT data = {
                                            Window::CopyCode::OpenFileCheck,
                                            sizeof (FILE_ID_INFO),
                                            (void *) pFileId
                                        };
                                        if (SendMessage (hWnd, WM_COPYDATA, NULL, (LPARAM) &data)) {
                                            SetForegroundWindow (hWnd);
                                            return FALSE;
                                        }
                                    }
                                    return TRUE;
                                 }, (LPARAM) &id.info) == FALSE) {

                    // other window has the file open and was activated, 
                    // or the single instance mode is active,
                    // we're done here

                    return ERROR_SUCCESS;
                }


                std::printf ("FILE ID:");
                for (auto i = 0u; i != 16u; ++i) {
                    std::printf (" %02x", id.info.FileId.Identifier [i]);
                }
                std::printf ("\n");

                // ask other instances if they have it open
                //  - yes: activate them and end
                //  - no: OpenFileById (...) // share only for read
                //     - on share failure
                //        - ask if really edit (a copy (disable 'Save')) or read-only
                //     - check if there's unsaved buffer and the file hasn't changed since
            } else {
                // ask if to create new file?
            }
        }

        // if not empty, get file ID 
        //   - file may not exist, ask to create
        //   - have file size, ask other instances if they can map it (if not, reuse this one)

        if (InitVersionInfoStrings (hInstance)
                && InitializeGUI (hInstance)
                && scale.initialize ()) {

            RegisterApplicationRestart (L"?", 0);

            const auto hAccelerators = LoadAccelerators (hInstance, MAKEINTRESOURCE (1));
            const auto hDarkMenuAccelerators = LoadAccelerators (hInstance, MAKEINTRESOURCE (2));

            hHook = SetWindowsHookEx (WH_CALLWNDPROC,
                                      [] (int code, WPARAM wParam, LPARAM lParam) -> LRESULT {
                                          if (code == HC_ACTION) {
                                              auto cwp = reinterpret_cast <const CWPSTRUCT *> (lParam);
                                              switch (cwp->message) {
                                                  case WM_KEYDOWN:
                                                      if (Window::dark_menu_tracking != nullptr) {
                                                          if (GetClassWord (cwp->hwnd, GCW_ATOM) == 32768) { // menu window class
                                                              Window::dark_menu_tracking->OnTrackedMenuKey (cwp->hwnd, cwp->wParam);
                                                          }
                                                      }
                                              }
                                          }
                                          return CallNextHookEx (hHook, code, wParam, lParam);
                                      },
                                      NULL, GetCurrentThreadId ());

            new Window;

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

            scale.terminate ();

            CoUninitialize ();
            return (int) message.wParam;
        }
    }
    return (int) GetLastError ();
}

bool InitVersionInfoStrings (HINSTANCE hInstance) {
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

                auto pstrings = static_cast <const unsigned char *> (data) + 76
                                + reinterpret_cast <const Header *> (data)->wValueLength;
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
    return true;
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
    return Window::Initialize (hInstance);
}
