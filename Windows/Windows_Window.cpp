#include "Windows_Window.hpp"
#include "Windows_Symbol.hpp"
#include "Windows_LoadIcon.hpp"

#include <shellapi.h>
#include <cstdint>
#include <new>

namespace {
    DWORD tlsInitSlot = TLS_OUT_OF_INDEXES;

    UINT (WINAPI * pfnGetDpiForSystem) () = NULL;
    UINT (WINAPI * pfnGetDpiForWindow) (HWND) = NULL;
    BOOL (WINAPI * ptrEnableNonClientDpiScaling) (HWND) = NULL;
    BOOL (WINAPI * ptrAreDpiAwarenessContextsEqual) (DPI_AWARENESS_CONTEXT, DPI_AWARENESS_CONTEXT) = NULL;
    DPI_AWARENESS_CONTEXT (WINAPI * ptrGetWindowDpiAwarenessContext) (HWND) = NULL;
}

// Generalized DPI retrieval
//  - GetDpiFor(System/Window) available since 1607 / LTSB2016 / Server 2016
//  - GetDeviceCaps is classic way, working way back to XP
//
UINT Windows::GetDPI (HWND hWnd) {
    if (hWnd != NULL) {
        if (pfnGetDpiForWindow)
            return pfnGetDpiForWindow (hWnd);
    } else {
        if (pfnGetDpiForSystem)
            return pfnGetDpiForSystem ();
    }
    if (HDC hDC = GetDC (hWnd)) {
        auto dpi = GetDeviceCaps (hDC, LOGPIXELSX);
        ReleaseDC (hWnd, hDC);
        return dpi;
    } else
        return USER_DEFAULT_SCREEN_DPI;
}

bool Windows::AreDpiApisScaled (HWND hWnd) {
    if (ptrGetWindowDpiAwarenessContext && ptrAreDpiAwarenessContextsEqual) {
        return ptrAreDpiAwarenessContextsEqual (ptrGetWindowDpiAwarenessContext (hWnd), DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    } else
        return false;
}

ATOM Windows::Window::Initialize (HINSTANCE hInstance, LPCTSTR classname) {
    tlsInitSlot = TlsAlloc ();
    if (pfnGetDpiForSystem == NULL) {
        if (HMODULE hUser32 = GetModuleHandle (L"USER32")) {
            Symbol (hUser32, pfnGetDpiForSystem, "GetDpiForSystem");
            Symbol (hUser32, pfnGetDpiForWindow, "GetDpiForWindow");
            Symbol (hUser32, ptrEnableNonClientDpiScaling, "EnableNonClientDpiScaling");
            Symbol (hUser32, ptrGetWindowDpiAwarenessContext, "GetWindowDpiAwarenessContext");
            Symbol (hUser32, ptrAreDpiAwarenessContextsEqual, "AreDpiAwarenessContextsEqual");
        }
    }
    WNDCLASSEX wndclass = {
        sizeof (WNDCLASSEX), CS_HREDRAW | CS_VREDRAW,
        Initializer, 0, sizeof (Window *), hInstance,  NULL,
        LoadCursor (NULL, IDC_ARROW), NULL, NULL, classname, NULL
    };
    return RegisterClassEx (&wndclass);
}

HWND Windows::Window::Create (HINSTANCE hInstance, ATOM atom, DWORD style, DWORD extra, const RECT * r) {
    if (this->hWnd) {
        SetLastError (ERROR_ALREADY_EXISTS);
        return NULL;
    }

    if (tlsInitSlot != TLS_OUT_OF_INDEXES) {
        TlsSetValue (tlsInitSlot, this);
    }
    auto h = CreateWindowEx (extra, (LPCTSTR) (std::intptr_t) atom, L"", style,
                             r ? r->left            : CW_USEDEFAULT, r ? r->top             : CW_USEDEFAULT,
                             r ? r->right - r->left : CW_USEDEFAULT, r ? r->bottom - r->top : CW_USEDEFAULT,
                             HWND_DESKTOP, NULL, hInstance, NULL);
    
    if (tlsInitSlot != TLS_OUT_OF_INDEXES) {
        TlsSetValue (tlsInitSlot, nullptr);
    }
    return h;
}

bool Windows::Window::IsLastWindow () const {
    return EnumThreadWindows (GetCurrentThreadId (),
                              [] (HWND hWnd, LPARAM lParam) {
                                  HWND hSelf = (HWND) lParam;
                                  auto aWnd = GetClassLongPtr (hWnd, GCW_ATOM);
                                  auto aSelf = GetClassLongPtr (hSelf, GCW_ATOM);

                                  return (BOOL) !((aWnd == aSelf) && (hWnd != hSelf));
                              }, (LPARAM) this->hWnd);
}

Windows::Window::Window (HINSTANCE hInstance, LPCWSTR icon)
    : WindowPresentation (hInstance, icon) {}

Windows::Window::~Window () {
    if (this->hWnd) {
        if (this->linger) {
            SetWindowLongPtr (hWnd, GWLP_WNDPROC, reinterpret_cast <LONG_PTR> (DefWindowProc));
        } else {
            DestroyWindow (this->hWnd);
        }
    }
    this->hWnd = NULL;
}

// Initializer
//  - initialization procedure
//
LRESULT CALLBACK Windows::Window::Initializer (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    const auto window = static_cast <Window *> (TlsGetValue (tlsInitSlot));
    if (!window)
        return FALSE;

    window->hWnd = hWnd;
    window->dpi = Windows::GetDPI (hWnd);

    SetWindowLongPtr (hWnd, 0, reinterpret_cast <LONG_PTR> (window));
    SetWindowLongPtr (hWnd, GWLP_WNDPROC, reinterpret_cast <LONG_PTR> (&Window::Procedure));

    try {
        return window->Dispatch (message, wParam, lParam);
    } catch (const std::exception & x) {
        return window->OnException (x);
    } catch (...) {
        return window->OnException ();
    }
}

// Procedure
//  - forwarding to actual procedure (member function)
//  - we are also eating exceptions so they don't escape to foreign frames
//
LRESULT CALLBACK Windows::Window::Procedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto window = reinterpret_cast <Window *> (GetWindowLongPtr (hWnd, 0));
    if (window->hWnd) [[likely]] {
        try {
            return window->Dispatch (message, wParam, lParam);
        } catch (const std::exception & x) {
            return window->OnException (x);
        } catch (...) {
            return window->OnException ();
        }
    } else
        return DefWindowProc (hWnd, message, wParam, lParam);
}

// Dispatch
//  - unpack wParam/lParam and call appropriate handler function
//
LRESULT Windows::Window::Dispatch (UINT message, WPARAM wParam, LPARAM lParam) {
    this->z_message = message;
    this->z_wParam = wParam;
    this->z_lParam = lParam;

    switch (message) {
        case WM_NCCREATE:
            if (ptrEnableNonClientDpiScaling) {
                ptrEnableNonClientDpiScaling (hWnd);
            }
            this->RefreshWindowPresentation (hWnd);
            return TRUE;
        case WM_CREATE:
            try {
                return this->OnCreate (reinterpret_cast <const CREATESTRUCT *> (lParam));
            } catch (...) {
                return -1;
            }
        case WM_CLOSE:
            return this->OnClose (wParam);
        case WM_DESTROY:
            return this->OnDestroy ();
        case WM_NCDESTROY:
            return this->OnFinalize ();
        case WM_ENDSESSION:
            return this->OnEndSession (wParam, lParam);

        case WM_ACTIVATE:
            return this->OnActivate (LOWORD (wParam), HIWORD (wParam), (HWND) lParam);

        case WM_DPICHANGED:
            if (auto r = reinterpret_cast <RECT *> (lParam)) {
                wParam = LOWORD (wParam);
                if (this->dpi != wParam) {
                    auto previous = this->dpi;
                    this->dpi = long (wParam);
                    this->OnDpiChange (r, previous);
                }
                this->RefreshWindowPresentation (hWnd);
                SetWindowPos (hWnd, NULL, r->left, r->top, r->right - r->left, r->bottom - r->top, 0);
            }
            return 0;
        case WM_WINDOWPOSCHANGED:
            return this->OnPositionChange (*reinterpret_cast <const WINDOWPOS *> (lParam));
        case WM_GETMINMAXINFO:
            return this->OnGetMinMaxInfo (reinterpret_cast <MINMAXINFO *> (lParam));

        case WM_TIMER:
            return this->OnTimer (wParam);

        case WM_CHAR:
            return this->OnChar (wParam, lParam);
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            this->OnKey (wParam, TRUE, LOWORD (lParam));
            break;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            this->OnKey (wParam, FALSE, LOWORD (lParam));
            break;

        case WM_COMMAND:
            return this->OnCommand (reinterpret_cast <HWND> (lParam), LOWORD (wParam), HIWORD (wParam));
        case WM_SYSCOMMAND:
            this->OnCommand (NULL, LOWORD (wParam), HIWORD (wParam));
            break;
        case WM_NOTIFY:
            return this->OnNotify (wParam, reinterpret_cast <NMHDR *> (lParam));

        case WM_COPYDATA:
            if (auto cps = reinterpret_cast <const COPYDATASTRUCT *> (lParam)) {
                return this->OnCopyData ((HWND) wParam, cps->dwData, cps->lpData, cps->cbData);
            } else
                return false;
                                        
        /*case WM_DROPFILES:
            if (auto n = DragQueryFile ((HDROP) wParam, 0xFFFFFFFF, NULL, 0)) {
                for (auto i = 0u; i != n; ++i) {
                    TCHAR szFileName [256];
                    if (DragQueryFile ((HDROP) wParam, i, szFileName, 256)) {
                        MessageBox (hWnd, szFileName, szFileName, 0);
                    }
                }
            }
            DragFinish ((HDROP) wParam);
            break;// */

        case WM_THEMECHANGED:
        case WM_SETTINGCHANGE:
        case WM_DWMCOMPOSITIONCHANGED:
            return this->OnPresentationChangeNotification (message, wParam, lParam);

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
            return this->OnCtlColor ((HDC) wParam, (HWND) lParam);

        case WM_PRINTCLIENT:
        case WM_ERASEBKGND:
            return this->OnEraseBackground ((HDC) wParam);

        case WM_DRAWITEM:
            return this->OnDrawItem (wParam, reinterpret_cast <DRAWITEMSTRUCT *> (lParam));

        case WM_NEXTMENU:
            return this->OnMenuNext (wParam, reinterpret_cast <MDINEXTMENU *> (lParam));
            break;
        case WM_UNINITMENUPOPUP:
            return this->OnMenuClose ((HMENU) wParam, HIWORD (lParam));
    }

    if (message == this->GetGlobalRefreshNotificationMessage ()) {
        this->OnVisualEnvironmentChange ();
    }

    if (message >= WM_USER && message <= 0x7FFF) {
        return this->OnUserMessage (message, wParam, lParam);
    }
    if (message >= WM_APP && message <= 0xBFFF) {
        return this->OnAppMessage (message, wParam, lParam);
    }
    if (auto result = this->ProcessPresentationMessage (this->hWnd, message, wParam, lParam)) {
        return *result;
    }
    if (message >= 0xC000 && message <= 0xFFFF) {
        return this->OnRegisteredMessage (message, wParam, lParam);
    }
    /*if (auto result = DwmDefWindowProc (this->hWnd, message, wParam, lParam)) {
        return *result;
    }// */
    return DefWindowProc (hWnd, message, wParam, lParam);
}

/*std::optional <LRESULT> DwmDefWindowProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    LRESULT result = 0;
    if (DwmDefWindowProc (hWnd, message, wParam, lParam, &result)) {
        return result;
    } else
        return {};
}// */

LRESULT Windows::Window::Default () {
    return this->Default (this->z_wParam, this->z_lParam);
}
LRESULT Windows::Window::Default (WPARAM wParam, LPARAM lParam) {
    return DefWindowProc (hWnd, this->z_message, wParam, lParam);
}

LRESULT Windows::Window::OnCreate (const CREATESTRUCT *) { return 0; }
LRESULT Windows::Window::OnClose (WPARAM code) { DestroyWindow (hWnd); return 0; }
LRESULT Windows::Window::OnDestroy () { return 0; }
LRESULT Windows::Window::OnFinalize () { return 0; }
LRESULT Windows::Window::OnActivate (WORD activated, BOOL minimized, HWND hOther) { return 0; }
LRESULT Windows::Window::OnTimer (WPARAM id) { return 0; }
LRESULT Windows::Window::OnGetMinMaxInfo (MINMAXINFO * info) { return 0; }
LRESULT Windows::Window::OnPositionChange (const WINDOWPOS &) { return 0; }
LRESULT Windows::Window::OnDpiChange (RECT * r, LONG previous) { return 0; }
LRESULT Windows::Window::OnDrawItem (WPARAM id, const DRAWITEMSTRUCT *) { return 0; }
LRESULT Windows::Window::OnNotify (WPARAM id, NMHDR *) { return 0; }
LRESULT Windows::Window::OnVisualEnvironmentChange () { return 0; }

LRESULT Windows::Window::OnEraseBackground (HDC hDC) {
    RECT client;
    if (GetClientRect (this->hWnd, &client)) {
        SetDCBrushColor (hDC, this->global.background);
        FillRect (hDC, &client, (HBRUSH) GetStockObject (DC_BRUSH));
        return true;
    } else
        return false;
}

LRESULT Windows::Window::OnCtlColor (HDC hDC, HWND hCtrl) {
    SetBkColor (hDC, this->global.background);
    SetTextColor (hDC, this->global.text);
    SetDCBrushColor (hDC, this->global.background);
    return (LRESULT) GetStockObject (DC_BRUSH);
}

LRESULT Windows::Window::OnChar (WPARAM, LPARAM) { return 0; }
LRESULT Windows::Window::OnKey (WPARAM vk, BOOL down, USHORT repeat) { return 0; }
LRESULT Windows::Window::OnMenuClose (HMENU, USHORT mf) { return 0; }
LRESULT Windows::Window::OnMenuNext (WPARAM vk, MDINEXTMENU * next) { return 0; }

LRESULT Windows::Window::OnRegisteredMessage (UINT, WPARAM, LPARAM) { return 0; }
LRESULT Windows::Window::OnUserMessage (UINT, WPARAM, LPARAM) { return 0; }
LRESULT Windows::Window::OnAppMessage (UINT, WPARAM, LPARAM) { return 0; }


LRESULT Windows::Window::OnCommand (HWND hChild, USHORT id, USHORT notification) {
    NMHDR nm = { hChild, id, notification };
    return this->OnNotify (id, &nm);
}

LRESULT Windows::Window::OnEndSession (WPARAM ending, LPARAM flags) {
    if (ending) {
        this->linger = true;
        SendMessage (hWnd, WM_CLOSE, ERROR_SHUTDOWN_IN_PROGRESS, 0);
    }
    return 0;
}

LRESULT Windows::Window::OnCopyData (HWND hSender, ULONG_PTR code, const void * data, std::size_t size) {
    return TRUE;
}

LRESULT Windows::Window::OnException () {
    DestroyWindow (this->hWnd);
    return 0;
}
LRESULT Windows::Window::OnException (const std::exception &) {
    return this->OnException ();
}
