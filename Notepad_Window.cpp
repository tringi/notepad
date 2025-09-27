#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"

#include "Windows\Windows_Symbol.hpp"
#include "Windows\Windows_AccessibilityTextScale.hpp"
#include "Windows\Windows_CreateSolidBrushEx.hpp"
#include "Windows\Windows_IsWindowsBuildOrGreater.hpp"
#include "Windows\Windows_GetCurrentModuleHandle.hpp"

#include <commdlg.h>

extern Windows::TextScale scale;
extern wchar_t szTmpPathBuffer [MAX_NT_PATH];

LRESULT CALLBACK StatusBarTooltipSubclassProcedure (HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK TooltipThemeSubclassProcedure (HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

namespace {
    HMODULE LoadResourcesModule () {
        wchar_t path [MAX_PATH + 12];
        auto length = GetSystemDirectory (path, MAX_PATH);
        std::wcscpy (&path [length], L"\\NOTEPAD.EXE");

        return LoadLibraryEx (path, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    }

    ATOM GetClassAtom (LPCTSTR name) {
        WNDCLASSEX wc {};
        wc.cbSize = sizeof wc;
        return (ATOM) GetClassInfoEx (NULL, name, &wc);
    }

    inline bool IsMenuItemChecked (HMENU menu, UINT id) {
        return GetMenuState (menu, id, MF_BYCOMMAND) & MF_CHECKED;
    }
    inline bool IsWindows11OrGreater () {
        return Windows::IsWindowsBuildOrGreater (22000);
    }

    LRESULT CALLBACK ProperBgSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        auto self = reinterpret_cast <Window *> (dwRefData);

        switch (message) {
            case WM_ERASEBKGND:
                if (self->GetPresentation ().dark) {
                    RECT r;
                    if (GetClientRect (hWnd, &r)) {
                        HBRUSH hBrush = NULL;
                        if (IsWindows11OrGreater ()) {
                            uIdSubclass = 0;
                        }
                        switch (uIdSubclass) {
                            case 1: // Windows 10 dark menu background
                                if (GetForegroundWindow () == GetParent (hWnd)) {
                                    hBrush = Windows::CreateSolidBrushEx (self->GetPresentation ().active, 128);
                                } else {
                                    hBrush = Windows::CreateSolidBrushEx (self->GetPresentation ().inactive, 255);
                                }
                                break;
                            case 0: // W7/W11 fully transparent background to expose aero/mica backdrop
                                BOOL enabled;
                                if (SUCCEEDED (DwmIsCompositionEnabled (&enabled)) && enabled) {
                                    hBrush = (HBRUSH) GetStockObject (BLACK_BRUSH);
                                } else {
                                    hBrush = (HBRUSH) SendMessage (GetParent (hWnd), WM_CTLCOLORBTN, wParam, (LPARAM) hWnd);
                                }
                                break;
                        }
                        if (hBrush) {
                            FillRect ((HDC) wParam, &r, hBrush);
                            switch (uIdSubclass) {
                                case 1:
                                    DeleteObject (hBrush);
                            }
                        }
                        return TRUE;
                    }
                }
        }
        return DefSubclassProc (hWnd, message, wParam, lParam);
    }

    ATOM atomCOMBOBOX = 0;
}

ATOM Window::InitAtom (HINSTANCE hInstance) {
    Window::atom = Windows::Window::Initialize (hInstance, L"TRIMCORE.NOTEPAD");
    return Window::atom;
}

ATOM Window::Initialize (HINSTANCE hInstance) {
    instance = hInstance;
    
    Window::rsrc = LoadResourcesModule ();
    Window::menu = LoadMenu (hInstance, MAKEINTRESOURCE (1));
    Window::wndmenu = LoadMenu (hInstance, MAKEINTRESOURCE (2));

    if (IsWindows11OrGreater ()) {
        if (Settings::Get (0x5A, 0)) {
            CheckMenuItem (menu, 0x5A, MF_BYCOMMAND | MF_CHECKED);
        }
        if (Settings::Get (0x5B, 0)) {
            CheckMenuItem (menu, 0x5B, MF_BYCOMMAND | MF_CHECKED);
        }
    } else {
        EnableMenuItem (menu, 0x5A, MF_BYCOMMAND | MF_GRAYED);
        CheckMenuItem (menu, 0x5A, MF_BYCOMMAND | MF_CHECKED);
        EnableMenuItem (menu, 0x5B, MF_BYCOMMAND | MF_GRAYED);
        CheckMenuItem (menu, 0x5B, MF_BYCOMMAND | MF_CHECKED);
    }

    atomCOMBOBOX = GetClassAtom (L"COMBOBOX");
    return Window::atom;
}

Window::Window (int show)
    : Windows::Window (this->rsrc, MAKEINTRESOURCE (2)) {

    this->CommonConstructor (show);
}

Window::Window (int show, File && file)
    : Windows::Window (this->rsrc, MAKEINTRESOURCE (2))
    , File (std::move (file)) {

    this->CommonConstructor (show);
}

void Window::CommonConstructor (int show) {
    this->icons.aux.try_emplace (1);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    DWORD extra = 0; // WS_EX_COMPOSITED; // prevents flashbang on start, but makes everything flash on repaint

    // TODO: last position?

    ShowWindow (this->Create (this->instance, this->atom, style, extra), show);
}

LRESULT Window::OnCreate (const CREATESTRUCT * cs) {

    // in dark mode, to prevent flashbang, we cloak the window
    // and uncloak it after first painting has finished;
    // unfortunatelly we lose the window opening animation

    if (auto option = Settings::Get (L"Start Cloaked", 1)) {
        if (this->global.dark || (option > 1)) {
            BOOL cloak = TRUE;
            DwmSetWindowAttribute (this->hWnd, DWMWA_CLOAK, &cloak, sizeof (cloak));
            SetTimer (this->hWnd, TimerID::UnCloak, 10, NULL);
        }
    }

    ChangeWindowMessageFilter (Window::Message::UpdateSettings, MSGFLT_ADD);

    if (IsWindows11OrGreater ()) {
        DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute (hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof backdrop);
    }

    for (auto id = 0x5A; id != 0x5C; ++id) {
        SendMessage (this->hWnd, Window::Message::UpdateSettings, 0, MAKELPARAM (id, !!IsMenuItemChecked (menu, id)));
    }

    if (auto menu = GetSystemMenu (this->hWnd, FALSE)) {
        wchar_t buffer [64];

        MENUITEMINFO item;
        item.cbSize = sizeof item;
        item.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
        item.dwTypeData = buffer;

        UINT i = 0;
        do {
            item.cch = sizeof buffer / sizeof buffer [0];
        } while (GetMenuItemInfo (Window::wndmenu, i++, TRUE, &item)
                && InsertMenuItem (menu, 127, TRUE, &item));

        if (cs->dwExStyle & WS_EX_TOPMOST) {
            CheckMenuItem (menu, ID::MENU_CMD_TOPMOST, MF_CHECKED);
        }
    }

    if (auto hMenuBar = CreateWindowEx (0, TOOLBARCLASSNAME, L"", WS_CHILD | WS_VISIBLE
                                        | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                                        0, 0, 0, 0, hWnd, (HMENU) ID::MENUBAR, cs->hInstance, NULL)) {

        if (IsWindows11OrGreater ()) {
            SetWindowSubclass (hMenuBar, ProperBgSubclassProcedure, 0, (DWORD_PTR) this);
        }

        SendMessage (hMenuBar, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);
        SendMessage (hMenuBar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
        SendMessage (hMenuBar, TB_SETIMAGELIST, 0, 0);
        SendMessage (hMenuBar, TB_SETDRAWTEXTFLAGS, DT_HIDEPREFIX, DT_HIDEPREFIX);

        this->RecreateMenuButtons (hMenuBar);
    }

    if (auto hMenuNote = CreateWindowEx (0, L"BUTTON", L"F11", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                         0, 0, 0, 0, hWnd, (HMENU) ID::MENUNOTE, cs->hInstance, NULL)) {
    }

    CreateWindowEx (0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                    0, 0, 0, 0, hWnd, (HMENU) ID::SEPARATOR, cs->hInstance, NULL);

    if (auto hEditor = CreateWindowEx (0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
                                       0, 0, 0, 0, hWnd, (HMENU) ID::EDITOR, cs->hInstance, NULL)) {
        // This is temporary
    }

    auto style = WS_CHILD | SBARS_TOOLTIPS | CCS_NOPARENTALIGN;
    if (cs->style & WS_THICKFRAME) {
        style |= SBARS_SIZEGRIP;
    }

    if (auto hStatusBar = CreateWindowEx (0, STATUSCLASSNAME, L"", style,
                                          0, 0, 0, 0, hWnd, (HMENU) ID::STATUSBAR, cs->hInstance, NULL)) {

        SetWindowSubclass (hStatusBar, ProperBgSubclassProcedure, 1, (DWORD_PTR) this);
        SetWindowSubclass (hStatusBar, StatusBarTooltipSubclassProcedure, 0, (DWORD_PTR) this);
        SetWindowSubclass (hStatusBar, TooltipThemeSubclassProcedure, 0, (DWORD_PTR) this);

        SendMessage (hStatusBar, SB_SIMPLE, FALSE, 0);
    }

    this->UpdateFileName ();

    this->OnVisualEnvironmentChange ();
    this->OnDpiChange (NULL, this->dpi);
    return 0;
}

void Window::UpdateFileName () {
    extern const wchar_t * szVersionInfo [9];

    if (this->handle != INVALID_HANDLE_VALUE) {
        if (GetFinalPathNameByHandle (this->handle, szTmpPathBuffer, MAX_NT_PATH, 0)) {
            DWORD offset = 0;

            // remove prefixes
            if (std::wcsncmp (szTmpPathBuffer, L"\\\\?\\UNC\\", 8) == 0) {
                szTmpPathBuffer [6] = L'\\';
                offset = 6;
            } else
            if (std::wcsncmp (szTmpPathBuffer, L"\\\\?\\", 4) == 0) {
                offset = 4;
            }

            // status bar
            SendDlgItemMessage (this->hWnd, ID::STATUSBAR, SB_SETTEXT, StatusBarCell::FileName | SBT_OWNERDRAW, (LPARAM) (szTmpPathBuffer + offset));

            // construct caption
            if (auto file = std::wcsrchr (szTmpPathBuffer, L'\\')) {
                std::wmemmove (szTmpPathBuffer, file + 1, std::wcslen (file));
            }

        } else {
            SendDlgItemMessage (this->hWnd, ID::STATUSBAR, SB_SETTEXT, StatusBarCell::FileName | SBT_OWNERDRAW, (LPARAM) L"\x2370");
            std::wcscpy (szTmpPathBuffer, L"\x2370");
        }

        std::wcscat (szTmpPathBuffer, L" \x2013 ");
    } else {
        SendDlgItemMessage (this->hWnd, ID::STATUSBAR, SB_SETTEXT, StatusBarCell::FileName | SBT_OWNERDRAW, (LPARAM) L"");
        szTmpPathBuffer [0] = L'\0';
    }

    // TODO: prefix with symbol if there are unsaved changes; maybe use taskbar overlay

    std::wcscat (szTmpPathBuffer, szVersionInfo [0]);
    SetWindowText (this->hWnd, szTmpPathBuffer);
}

LRESULT Window::OnFinalize () {
    delete this;
    return 0;
}

LRESULT Window::OnActivate (WORD activated, BOOL minimized, HWND hOther) {
    if (activated) {
        SetFocus (GetDlgItem (this->hWnd, (int) ID::EDITOR));
    }
    InvalidateRect (GetDlgItem (this->hWnd, ID::STATUSBAR), NULL, TRUE);
    InvalidateRect (GetDlgItem (this->hWnd, ID::MENUNOTE), NULL, FALSE);
    this->ShowMenuAccelerators (false);
    return TRUE;
}

LRESULT Window::OnEndSession (WPARAM ending, LPARAM flags) {
    if (ending) {

    }
    return Windows::Window::OnEndSession (ending, flags);
}

LRESULT Window::OnClose (WPARAM wParam) {
    if (this->IsLastWindow ()) {
        if (wParam == ERROR_SHUTDOWN_IN_PROGRESS) {

            // TODO: Save unsaved temporary here

            ExitProcess (ERROR_SHUTDOWN_IN_PROGRESS);
        } else {
            PostQuitMessage ((int) wParam);
        }
    }
    DestroyWindow (this->hWnd);
    return 0;
}

LRESULT Window::OnGetMinMaxInfo (MINMAXINFO * mmi) {
    if (this->minimum.statusbar.cx) {
        mmi->ptMinTrackSize.x = this->minimum.statusbar.cx + 64;
    }
    mmi->ptMinTrackSize.y = 64 + this->minimum.statusbar.cy + 0;
    return 0;
}

SIZE GetClientSize (HWND hWnd) {
    RECT r;
    if (GetClientRect (hWnd, &r)) {
        return { r.right - r.left, r.bottom - r.top };
    } else
        return { 0, 0 };
}

void DeferWindowPos (HDWP & hDwp, HWND hCtrl, const RECT & r, UINT flags = 0) {
    if (hCtrl) {
        if (auto hNewDwp = DeferWindowPos (hDwp, hCtrl, NULL,
                                           r.left, r.top, r.right - r.left, r.bottom - r.top,
                                           SWP_NOACTIVATE | SWP_NOZORDER | flags)) {
            hDwp = hNewDwp;
        }
    }
}

LRESULT Window::OnPositionChange (const WINDOWPOS & position) {
    if (!(position.flags & SWP_NOSIZE) || (position.flags & (SWP_SHOWWINDOW | SWP_FRAMECHANGED))) {

        auto client = GetClientSize (hWnd);
        auto hEditor = GetDlgItem (hWnd, (int) ID::EDITOR);
        auto hMenuBar = GetDlgItem (hWnd, (int) ID::MENUBAR);
        auto hMenuNote = GetDlgItem (hWnd, (int) ID::MENUNOTE);
        auto hSeparator = GetDlgItem (hWnd, (int) ID::SEPARATOR);
        auto hStatusBar = GetDlgItem (hWnd, (int) ID::STATUSBAR);

        SIZE szMenuBar;
        SendMessage (hMenuBar, TB_GETMAXSIZE, 0, (LPARAM) &szMenuBar);

        auto yMenuBar = szMenuBar.cy;
        auto ySeparator = 1;
        if (!this->global.dark) {
            yMenuBar = 0;
            ySeparator = 0;
        }
        auto yStatusBar = UpdateStatusBar (hStatusBar, this->dpi, client);
        auto xMenuNote = 0;
        if (this->bFullscreen && this->global.dark) {
            xMenuNote = 48 * this->dpi / 96;
        }

        if (HDWP hDwp = BeginDeferWindowPos (5)) {
            DeferWindowPos (hDwp, hMenuBar,   { 0, 0, client.cx - xMenuNote, yMenuBar }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hMenuNote,  { client.cx - xMenuNote, 0, client.cx, yMenuBar }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hSeparator, { 0, yMenuBar, client.cx, yMenuBar + ySeparator }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hEditor,    { 0, yMenuBar + ySeparator, client.cx, client.cy - yStatusBar }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hStatusBar, { 0, client.cy - yStatusBar, client.cx, client.cy }, SWP_SHOWWINDOW);
            EndDeferWindowPos (hDwp);
        }

        MARGINS margins = { 0,0,0,0 };
        if (this->global.dark) {
            margins.cyTopHeight = yMenuBar;
            if (IsWindows11OrGreater ()) {
                margins.cyBottomHeight = yStatusBar;
            }
        }
        DwmExtendFrameIntoClientArea (hWnd, &margins);
        //InvalidateRect (hWnd, NULL, TRUE);
    }
    return 0;
}

LRESULT Window::OnTimer (WPARAM id) {
    switch (id) {
        case TimerID::DarkMenuBarTracking:
            return this->OnTimerDarkMenuBarTracking (id);

        case TimerID::UnCloak:
            BOOL cloak = FALSE;
            DwmSetWindowAttribute (this->hWnd, DWMWA_CLOAK, &cloak, sizeof (cloak));
            KillTimer (this->hWnd, id);
            break;
    }
    return 0;
}

LRESULT Window::OnCommand (HWND hChild, USHORT id, USHORT notification) {
    switch (id) {
        case IDCLOSE:
            PostMessage (this->hWnd, WM_CLOSE, 0, 0);
            break;

        case ID::MENU_CMD_TOPMOST:
            if (auto menu = GetSystemMenu (this->hWnd, FALSE)) {
                auto checked = GetMenuState (menu, id, 0) & MF_CHECKED;
                SetWindowPos (this->hWnd, checked ? HWND_NOTOPMOST : HWND_TOPMOST,
                              0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
                CheckMenuItem (menu, id, checked ? 0 : MF_CHECKED);

                // TODO: set/unset taskbar button badge
            }
            break;

        case 0x20:
            // New file
            // TODO: if unsaved changes, ask to save or not
            break;

        case 0x21:
            new Window (SW_SHOWDEFAULT);
            break;

        case 0x22:
            //if unsaved changes ask: Save, Save As, Don't Save (forget changes), Cancel

            // go with open
        {
            wchar_t mask [256];

            int i = 0x100;
            int m = 0;
            int n = 0;
            while (m = LoadString (Windows::GetCurrentModuleHandle (), i, &mask [n], sizeof mask / sizeof mask [0] - n - 1)) {
                n += m + 1;
                i++;
            }
            mask [n] = L'\0';

            szTmpPathBuffer [0] = L'\0';

            OPENFILENAME ofn {};
            ofn.lStructSize = sizeof (OPENFILENAME);
            ofn.hwndOwner = this->hWnd;
            ofn.hInstance = Windows::GetCurrentModuleHandle ();
            ofn.lpstrFilter = mask;
            ofn.lpstrCustomFilter = NULL;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = szTmpPathBuffer; // TODO: copy current path here? TODO: do not use shared buffer, multiple Open dialogs could be open
            ofn.nMaxFile = MAX_NT_PATH;
            ofn.lpstrFileTitle = NULL;
            ofn.lpstrInitialDir = NULL;
            ofn.lpstrTitle = NULL;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_ENABLESIZING; // ??? OFN_ALLOWMULTISELECT 
            ofn.lpstrDefExt = L"txt";

            if (GetOpenFileName (&ofn)) {
                

            }
        }
            break;

        case 0x5A:
        case 0x5B:
        case 0x5F:
        case 0x6E:
        case 0x6F:
            Settings::Set (id, IsMenuItemChecked (menu, id));
            EnumWindows ([] (HWND hWnd, LPARAM lParam) {
                             if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {
                                 PostMessage (hWnd, Window::Message::UpdateSettings, 0, lParam);
                             }
                             return TRUE;
                         },
                         MAKELPARAM (id, !IsMenuItemChecked (menu, id)));
            break;

        case 0x12: case 0x13: case 0x14: case 0x15: case 0x16:
            this->TrackMenu (id - 0x12);
            break;

        case 0xCF: // Ctrl+Alt+F4
            PostMessage (NULL, WM_COMMAND, id, 0);
            break;

        case 0xFB: // F11
            this->bFullscreen = !this->bFullscreen;
            if (this->bFullscreen) {
                if (IsZoomed (this->hWnd))
                    ShowWindow (this->hWnd, SW_RESTORE);

                GetWindowRect (this->hWnd, &this->rBeforeFullscreen);
                SetWindowLong (this->hWnd, GWL_STYLE, WS_POPUP);

                // bool shift = GetKeyState (VK_SHIFT) & 0x8000; // without menu and status bar?
                ShowWindow (this->hWnd, SW_MAXIMIZE);

            } else {
                SetWindowLong (this->hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
                ShowWindow (this->hWnd, SW_RESTORE);

                MoveWindow (this->hWnd, rBeforeFullscreen.left, rBeforeFullscreen.top,
                            rBeforeFullscreen.right - rBeforeFullscreen.left,
                            rBeforeFullscreen.bottom - rBeforeFullscreen.top, TRUE);
            };

            break;
    }
    return 0;
}

LRESULT Window::OnNotify (WPARAM id, NMHDR * nm) {
    switch (id) {
        case ID::MENUBAR:
            return this->OnNotifyMenuBar (id, nm);

        case ID::STATUSBAR:
            return this->OnNotifyStatusBar (id, nm);
    }
    return 0;
}

LRESULT Window::OnDrawItem (WPARAM id, const DRAWITEMSTRUCT * draw) {
    switch (id) {
        case ID::SEPARATOR:
            if (auto hBrush = Windows::CreateSolidBrushEx (this->global.inactive, 255)) {
                FillRect (draw->hDC, &draw->rcItem, hBrush);
                DeleteObject (hBrush);
            }
            return TRUE;

        case ID::MENUNOTE:
            return this->OnDrawMenuNote (id, draw);

        case ID::STATUSBAR:
            return this->OnDrawStatusBar (id, draw);
    }
    return FALSE;
}

LRESULT Window::OnEraseBackground (HDC hDC) {
    RECT r;
    GetClientRect (this->hWnd, &r);

    // printf ("OnEraseBackground %p: %p\n", this->hWnd, WindowFromDC (hDC));

    SetDCBrushColor (hDC, 0x000000);
    FillRect (hDC, &r, (HBRUSH) GetStockObject (DC_BRUSH));
    return TRUE;
}

LRESULT Window::OnUserMessage (UINT message, WPARAM, LPARAM lParam) {
    switch (message) {
        case Message::ShowAccelerators:
            this->ShowMenuAccelerators (true);
            break;
        case Message::UpdateFontSize:
            this->OnDpiChange (NULL, 0);
            break;

        case Message::UpdateSettings:
            auto id = LOWORD (lParam);
            bool set = HIWORD (lParam) & 1;

            switch (id) {
                case 0x5A: {
                    COLORREF clr = set ? DWMWA_COLOR_DEFAULT : DWMWA_COLOR_NONE;
                    DwmSetWindowAttribute (hWnd, DWMWA_CAPTION_COLOR, &clr, sizeof clr);
                } break;
                case 0x5B: {
                    DWM_WINDOW_CORNER_PREFERENCE corners = set ? DWMWCP_DONOTROUND : DWMWCP_DEFAULT;
                    DwmSetWindowAttribute (hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
                } break;

                case 0x6F: // TODO: refresh to show/hide status bar
                    break;
                
                // TODO: on others refresh editor
            }
            CheckMenuItem (menu, id, set ? MF_CHECKED : 0);
            break;
    }
    return 0;
}

LRESULT Window::OnCopyData (HWND hSender, ULONG_PTR code, const void * data, std::size_t size) {
    switch (code) {
        case CopyCode::OpenFile:
            if (size >= sizeof (OpenFileRequest)) {
                auto request = static_cast <const Window::OpenFileRequest *> (data);
                try {
                    if (request->handle) {
                        return (LRESULT) (new Window (request->nCmdShow, File ((HANDLE) (std::intptr_t) request->handle)))->hWnd;
                    } else {
                        return (LRESULT) (new Window (request->nCmdShow))->hWnd;
                    }
                } catch (...) {
                }
            }
            return FALSE;

        case CopyCode::OpenFileCheck:
            return (size >= sizeof (FILE_ID_INFO))
                && this->File::IsOpen ((const FILE_ID_INFO *) data);
    }
    return 0;
}

LRESULT Window::OnDpiChange (RECT *, LONG) {
    auto dpiNULL = Windows::GetDPI (NULL);
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
        SendMessage (hMenuBar, TB_SETPADDING, 0, MAKELPARAM (8 * this->dpi / dpiNULL, 4 * this->dpi / dpiNULL));
        this->RecreateMenuButtons (hMenuBar);

        SendDlgItemMessage (hWnd, ID::MENUNOTE, WM_SETFONT, SendMessage (hMenuBar, WM_GETFONT, 0, 0), 1);
    }
    return 0;
}

BOOL WINAPI UpdateWindowTreeTheme (HWND hCtrl, LPARAM param) {
    EnumChildWindows (hCtrl, UpdateWindowTreeTheme, param);

    if (auto atom = GetClassWord (hCtrl, GCW_ATOM)) {
        if (atom == atomCOMBOBOX) {
            SetWindowTheme (hCtrl, param ? L"DarkMode_CFD" : NULL, NULL);
        } else {
            SetWindowTheme (hCtrl, param ? L"DarkMode_Explorer" : NULL, NULL);
        }
    }
    return TRUE;
}

LRESULT Window::OnVisualEnvironmentChange () {
    EnumChildWindows (this->hWnd, UpdateWindowTreeTheme, (LPARAM) this->global.dark);
    SetMenu (this->hWnd, this->global.dark ? NULL : this->menu);
    return FALSE;
}

