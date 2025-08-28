#include "Notepad_Window.hpp"
#include "Windows\Windows_Symbol.hpp"
#include "Windows\Windows_AccessibilityTextScale.hpp"
#include "Windows\Windows_CreateSolidBrushEx.hpp"
#include "Windows\Windows_IsWindowsBuildOrGreater.hpp"
#include "Windows\Windows_GetCurrentModuleHandle.hpp"
#include "Windows\Windows_DrawTextComposited.hpp"

#include <commdlg.h>

extern Windows::TextScale scale;
static constexpr auto MAX_NT_PATH = 32768u;

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

    LRESULT CALLBACK TooltipThemeSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        switch (message) {
            case WM_NOTIFY:
                auto nm = reinterpret_cast <NMHDR *> (lParam);
                if (nm->code == TTN_SHOW) {

                    auto marker = L"TRIMCORE.ThemeSet";
                    auto theme_dark = (std::intptr_t) reinterpret_cast <Window *> (dwRefData)->GetPresentation ().dark;
                    auto theme_set = (std::intptr_t) GetProp (nm->hwndFrom, marker);

                    if (theme_set != theme_dark + 1) {
                        SetProp (nm->hwndFrom, marker, (HANDLE) (theme_dark + 1));
                        SetWindowTheme (nm->hwndFrom, theme_dark ? L"DarkMode_Explorer" : NULL, NULL);
                    }
                }
        }
        return DefSubclassProc (hWnd, message, wParam, lParam);
    }

    LRESULT CALLBACK StatusBarTooltipSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                        UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        switch (message) {
            case WM_NOTIFY:
                if (reinterpret_cast <NMHDR *> (lParam)->code == TTN_GETDISPINFO) {
                    auto nmTT = reinterpret_cast <TOOLTIPTEXT *> (lParam);
                    switch (wParam) {
                        case 0: nmTT->lpszText = (LPWSTR) L"TBD: full path of open file, again"; return 0;
                        case 1: nmTT->lpszText = (LPWSTR) L"Current line and column"; return 0;
                        case 3: nmTT->lpszText = (LPWSTR) L"Disk: 160 kB, Memory: 500 kB, 10 kB of unsaved edits, 200 kB in undo steps"; return 0;
                        case 4: nmTT->lpszText = (LPWSTR) L"Ctrl+Wheel"; return 0;
                        case 5: nmTT->lpszText = (LPWSTR) L"Windows mode, click to change"; return 0;
                        case 6: nmTT->lpszText = (LPWSTR) L"Click to change"; return 0;
                    }
                    return 0;
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

    if (!IsWindows11OrGreater ()) {
        EnableMenuItem (menu, 0x5A, MF_BYCOMMAND | MF_GRAYED);
        CheckMenuItem (menu, 0x5A, MF_BYCOMMAND | MF_CHECKED);
        EnableMenuItem (menu, 0x5B, MF_BYCOMMAND | MF_GRAYED);
        CheckMenuItem (menu, 0x5B, MF_BYCOMMAND | MF_CHECKED);
    }

    atomCOMBOBOX = GetClassAtom (L"COMBOBOX");
    return Window::atom;
}

Window::Window ()
    : Windows::Window (this->rsrc, MAKEINTRESOURCE (2)) {

    this->icons.aux.try_emplace (1);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE;
    DWORD extra = WS_EX_COMPOSITED;
    
    // TODO: last position?

    this->Create (this->instance, this->atom, style, extra);
}

LRESULT Window::OnCreate (const CREATESTRUCT * cs) {
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

    if (auto hMenuBar = CreateWindowEx (WS_EX_COMPOSITED, TOOLBARCLASSNAME, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS
                                        | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                                        0, 0, 0, 0, hWnd, (HMENU) ID::MENUBAR, cs->hInstance, NULL)) {

        SetWindowSubclass (hMenuBar, ProperBgSubclassProcedure, 0, (DWORD_PTR) this);

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

    if (auto hEditor = CreateWindowEx (0, L"EDIT", L"Editor", WS_CHILD | WS_VISIBLE | WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
                                       0, 0, 0, 0, hWnd, (HMENU) ID::EDITOR, cs->hInstance, NULL)) {
        // This is temporary
    }

    auto style = WS_CHILD | SBARS_TOOLTIPS | CCS_NOPARENTALIGN;
    if (cs->style & WS_THICKFRAME) {
        style |= SBARS_SIZEGRIP;
    }

    if (auto hStatusBar = CreateWindowEx (WS_EX_COMPOSITED, STATUSCLASSNAME, L"", style,
                                          0, 0, 0, 0, hWnd, (HMENU) ID::STATUSBAR, cs->hInstance, NULL)) {

        SetWindowSubclass (hStatusBar, ProperBgSubclassProcedure, 1, (DWORD_PTR) this);
        SetWindowSubclass (hStatusBar, StatusBarTooltipSubclassProcedure, 0, (DWORD_PTR) this);
        SetWindowSubclass (hStatusBar, TooltipThemeSubclassProcedure, 0, (DWORD_PTR) this);

        SendMessage (hStatusBar, SB_SIMPLE, FALSE, 0);
    }

    // TODO: construct window caption

    extern const wchar_t * szVersionInfo [9];
    SetWindowText (this->hWnd, szVersionInfo [0]);

    this->OnVisualEnvironmentChange ();
    this->OnDpiChange (NULL, this->dpi);
    return 0;
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
    InvalidateRect (GetDlgItem (this->hWnd, ID::MENUNOTE), NULL, TRUE);
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

LONG Window::UpdateStatusBar (HWND hStatusBar, UINT dpi, SIZE parent) {
    SendMessage (hStatusBar, WM_SIZE, 0, MAKELPARAM (parent.cx, parent.cy));

    SIZE status = GetClientSize (hStatusBar);

    static const short widths [] = { 64, 64, 52, 44, 44, 96, 128 };
    static const auto  n = sizeof widths / sizeof widths [0];

    int scaled [n + 1] = {};
    int offset = parent.cx;
    int i = n;

    while (i-- != 0) {
        offset -= dpi * widths [i] / USER_DEFAULT_SCREEN_DPI;
        scaled [i] = offset;
    }

    scaled [n] = -1;

    this->minimum.statusbar = { parent.cx - scaled [0], status.cy };
    SendMessage (hStatusBar, SB_SETPARTS, n + 1, (LPARAM) scaled);

    SendMessage (hStatusBar, SB_SETTEXT, 0 | SBT_OWNERDRAW, (LPARAM) L"C:\\Full\\File\\Path\\here.txt"); // right click for menu: Copy Path, Open Path, Properties

    SendMessage (hStatusBar, SB_SETTEXT, 1 | SBT_OWNERDRAW, (LPARAM) L"5\x2236""16"); // TODO: click to Go To
    SendMessage (hStatusBar, SB_SETTEXT, 3 | SBT_OWNERDRAW, (LPARAM) L"240\x200AkB");
    SendMessage (hStatusBar, SB_SETTEXT, 4 | SBT_OWNERDRAW, (LPARAM) L"100\x200A%"); // TODO: click to Zoom
    SendMessage (hStatusBar, SB_SETTEXT, 5 | SBT_OWNERDRAW, (LPARAM) L"CR LF"); // TODO: click to change
    SendMessage (hStatusBar, SB_SETTEXT, 6 | SBT_OWNERDRAW, (LPARAM) L"Windows 1250"); // UTF-16 LE, click to change

    return status.cy;
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
        InvalidateRect (hWnd, NULL, TRUE);
    }
    return 0;
}

void Window::ShowMenuAccelerators (BOOL show) {
    if (auto hMenuBar = GetDlgItem (this->hWnd, Window::ID::MENUBAR)) {
        this->bMenuAccelerators = show;
        InvalidateRect (hMenuBar, NULL, TRUE);
    }
}

LRESULT Window::OnTimer (WPARAM id) {
    switch (id) {
        case TimerID::DarkMenuBarTracking:
            if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
                POINT pt;
                if (GetCursorPos (&pt)) {
                    MapWindowPoints (HWND_DESKTOP, hMenuBar, &pt, 1);

                    auto item = SendMessage (hMenuBar, TB_HITTEST, 0, (LPARAM) &pt);
                    if (item >= 0) {
                        if (this->active_menu != item) {
                            this->next_menu = item;
                            SendMessage (this->hWnd, WM_CANCELMODE, 0, 0);
                        }
                    }
                }
            }
            break;
    }
    return 0;
}

void Window::TrackMenu (UINT index) {
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {

        RECT r {};
        if (SendMessage (hMenuBar, TB_GETITEMRECT, index, (LPARAM) &r)) {
            MapWindowPoints (this->hWnd, NULL, (POINT *) &r, 2);

            if (auto hSubMenu = GetSubMenu (this->menu, index)) {
                UINT style = 0;
                if (GetSystemMetrics (SM_MENUDROPALIGNMENT)) {
                    style |= TPM_RIGHTALIGN;
                }

                this->next_menu = ~0;
                this->active_menu = index;
                this->dark_menu_tracking = this;
                this->hActiveMenu = hSubMenu;
                InvalidateRect (hMenuBar, NULL, TRUE);

                SetTimer (this->hWnd, TimerID::DarkMenuBarTracking, USER_TIMER_MINIMUM, NULL);
                TrackPopupMenu (hSubMenu, style, r.left, r.bottom, 0, this->hWnd, NULL);
                KillTimer (this->hWnd, TimerID::DarkMenuBarTracking);

                this->hActiveMenu = NULL;
                this->dark_menu_tracking = nullptr;
                this->active_menu = ~0;
                InvalidateRect (hMenuBar, NULL, TRUE);

                if (this->next_menu != ~0) {
                    return this->TrackMenu (this->next_menu);
                }
            }
        }
    }
}

void Window::OnTrackedMenuKey (HWND hMenuWindow, WPARAM vk) {
    switch (vk) {
        case VK_LEFT:
            if (!this->bInSubMenu) {
                if (this->active_menu != 0) {
                    this->next_menu = this->active_menu - 1;
                } else {
                    this->next_menu = this->menu_size - 1;
                }
                SendMessage (this->hWnd, WM_CANCELMODE, 0, 0);
            }
            break;
        case VK_RIGHT:
            if (!this->bOnSubMenu) {
                if (this->active_menu != this->menu_size - 1) {
                    this->next_menu = this->active_menu + 1;
                } else {
                    this->next_menu = 0;
                }
                SendMessage (this->hWnd, WM_CANCELMODE, 0, 0);
            }
            break;
    }
}

LRESULT Window::OnMenuSelect (HMENU menu, USHORT index, WORD flags) {
    if (menu) {
        this->bInSubMenu = (menu != this->hActiveMenu);
        this->bOnSubMenu = (flags & MF_POPUP);
    } else {
        this->bInSubMenu = false;
        this->bOnSubMenu = false;
    }
    return 0;
}

LRESULT Window::OnMenuClose (HMENU menu, USHORT flags) {
    if (menu == this->hActiveMenu) {
        this->ShowMenuAccelerators (false);
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
            new Window;
            break;

        case 0x22:
            //if unsaved changes ask: Save, Save As, Don't Save (forget changes), Cancel

            // go with open
        {
            wchar_t path [MAX_NT_PATH];
            wchar_t mask [256];

            int i = 0x100;
            int m = 0;
            int n = 0;
            while (m = LoadString (Windows::GetCurrentModuleHandle (), i, &mask [n], sizeof mask / sizeof mask [0] - n - 1)) {
                n += m + 1;
                i++;
            }
            mask [n] = L'\0';

            path [0] = L'\0';

            OPENFILENAME ofn {};
            ofn.lStructSize = sizeof (OPENFILENAME);
            ofn.hwndOwner = this->hWnd;
            ofn.hInstance = Windows::GetCurrentModuleHandle ();
            ofn.lpstrFilter = mask;
            ofn.lpstrCustomFilter = NULL;
            ofn.nFilterIndex = 1;
            ofn.lpstrFile = path; // TODO: copy current path here?
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
            EnumWindows ([] (HWND hWnd, LPARAM lParam) {
                if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {
                    PostMessage (hWnd, Window::Message::UpdateSettings, 0, lParam);
                }
                return TRUE;
            }, MAKELPARAM (id, !IsMenuItemChecked (menu, id)));
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

HBRUSH Window::CreateDarkMenuBarBrush () {
    if (this->global.dark) {
        HBRUSH hBrush;
        if (this->bFullscreen) {
            if (!IsWindows11OrGreater ()) {
                COLORREF clr;
                if (GetForegroundWindow () == this->hWnd) {
                    clr = RGB (GetRValue (this->global.active) / 2,
                               GetGValue (this->global.active) / 2,
                               GetBValue (this->global.active) / 2);
                } else {
                    clr = this->global.inactive;
                }
                hBrush = Windows::CreateSolidBrushEx (clr, 255);
            } else {
                hBrush = Windows::CreateSolidBrushEx (this->global.inactive, 255);
            }
        } else {
            hBrush = Windows::CreateSolidBrushEx (0x000000, 128);
        }
        return hBrush;
    } else
        return NULL;
}

LRESULT Window::OnNotify (WPARAM id, NMHDR * nm) {
    switch (id) {
        case ID::MENUBAR:
            switch (nm->code) {
                case TBN_BEGINDRAG:
                    this->TrackMenu (reinterpret_cast <NMTOOLBAR *> (nm)->iItem);
                    break;

                case NM_CUSTOMDRAW:
                    auto nmtb = reinterpret_cast <NMTBCUSTOMDRAW *> (nm);

                    switch (nmtb->nmcd.dwDrawStage) {
                        case CDDS_PREPAINT:
                            if (auto hBrush = this->CreateDarkMenuBarBrush ()) {
                                FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, hBrush);
                                DeleteObject (hBrush);
                                return CDRF_NOTIFYITEMDRAW;
                            } else
                                return CDRF_DODEFAULT;

                        case CDDS_ITEMPREPAINT:
                            const bool active = (GetActiveWindow () == this->hWnd);

                            if ((nmtb->nmcd.dwItemSpec == this->active_menu) || (nmtb->nmcd.uItemState & CDIS_HOT)) {
                                
                                COLORREF clr = 0;
                                if (IsWindows11OrGreater ()) {
                                    if (IsMenuItemChecked (menu, 0x5A) && active) {
                                        clr = this->global.accent;
                                    }
                                } else {
                                    clr = this->global.accent;
                                }

                                if (clr) {
                                    if (auto hBrush = Windows::CreateSolidBrushEx (clr, 255)) {
                                        FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, hBrush);
                                        DeleteObject (hBrush);
                                    }
                                } else {
                                    FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, (HBRUSH) GetStockObject (BLACK_BRUSH));
                                }
                            }

                            COLORREF color;
                            if (active || (nmtb->nmcd.uItemState & CDIS_HOT)) {
                                color = this->global.text;
                            } else {
                                color = GetSysColor (COLOR_GRAYTEXT);
                            }

                            wchar_t text [64];
                            auto n = GetMenuString (Window::menu, (UINT) nmtb->nmcd.dwItemSpec, text, sizeof text / sizeof text [0], MF_BYPOSITION);
                            DWORD flags = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
                            if (!this->bMenuAccelerators) {
                                flags |= DT_HIDEPREFIX;
                            }

                            SetBkMode (nmtb->nmcd.hdc, TRANSPARENT);
                            Windows::DrawTextComposited (nm->hwndFrom, nmtb->nmcd.hdc, text, n, flags, color, &nmtb->nmcd.rc);

                            return TBCDRF_NOOFFSET | CDRF_SKIPDEFAULT;
                    }
            }
            break;

        case ID::STATUSBAR:
            std::printf ("WM_NOTIFY ID::STATUSBAR %llu\n", id);
            break;
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
            if (auto hBrush = this->CreateDarkMenuBarBrush ()) {
                FillRect (draw->hDC, &draw->rcItem, hBrush);
                DeleteObject (hBrush);

                COLORREF color;
                if (GetActiveWindow () == this->hWnd) {
                    color = this->global.text;
                } else {
                    color = GetSysColor (COLOR_GRAYTEXT);
                }

                SetBkMode (draw->hDC, TRANSPARENT);
                Windows::DrawTextComposited (draw->hwndItem, draw->hDC, L"F11 ", 4, DT_VCENTER | DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX, color, &draw->rcItem);
                return TRUE;
            } else
                return FALSE;

        case ID::STATUSBAR:
            COLORREF color;
            if (GetActiveWindow () == this->hWnd) {
                color = this->global.text;
            } else {
                color = GetSysColor (COLOR_GRAYTEXT);
            }

            RECT r = draw->rcItem;
            r.top += 1;
            r.left += this->metrics [SM_CXPADDEDBORDER];
            r.right -= this->metrics [SM_CXFIXEDFRAME];
            if (SendMessage (draw->hwndItem, SB_GETICON, draw->itemID, 0)) {
                r.left += this->metrics [SM_CXSMICON];
            }
            SetBkMode (draw->hDC, TRANSPARENT);
            Windows::DrawTextComposited (draw->hwndItem, draw->hDC, (LPWSTR) draw->itemData, -1, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, color, &r);
            return TRUE;
    }
    return FALSE;
}

LRESULT Window::OnEraseBackground (HDC hDC) {
    //SetDCBrushColor (hDC, 0);
    return NULL; // (LRESULT) GetStockObject (DC_BRUSH);
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
            }
            CheckMenuItem (menu, id, set ? MF_CHECKED : 0);
            break;
    }
    return 0;
}

LRESULT Window::OnCopyData (HWND hSender, ULONG_PTR code, const void * data, std::size_t size) {
    std::printf ("OnCopyData from %p: %llu, size: %llu\n", hSender, code, size);

    switch (code) {
        case CopyCode::OpenFileCheck:
            // TODO: if single instance mode, open new Window

            return (size == sizeof (FILE_ID_INFO))
                && this->IsFileOpen ((const FILE_ID_INFO *) data);
    }
    return 0;
}

void Window::RecreateMenuButtons (HWND hMenuBar) {
    while (SendMessage (hMenuBar, TB_DELETEBUTTON, 0, 0))
        ;

    wchar_t text [64];
    TBBUTTON button {};
    button.fsState = TBSTATE_ENABLED;
    button.fsStyle = BTNS_SHOWTEXT | BTNS_AUTOSIZE;
    button.iString = (INT_PTR) text;

    this->menu_size = 0;
    while (GetMenuString (Window::menu, this->menu_size, text, sizeof text / sizeof text [0], MF_BYPOSITION)) {
        button.idCommand = this->menu_size;
        if (SendMessage (hMenuBar, TB_ADDBUTTONS, 1, (LPARAM) &button)) {
            ++this->menu_size;
        }
    }

    SendMessage (hMenuBar, TB_AUTOSIZE, 0, 0);
}

LRESULT Window::OnDpiChange (RECT *, LONG) {
    auto dpiNULL = Windows::GetDPI (NULL);
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
        SendMessage (hMenuBar, TB_SETPADDING, 0, MAKELPARAM (8 * this->dpi / dpiNULL, 4 * this->dpi / dpiNULL));
        this->RecreateMenuButtons (hMenuBar);

        SendDlgItemMessage (hWnd, ID::MENUNOTE, WM_SETFONT, SendMessage (hMenuBar, WM_GETFONT, 0, 0), 1);
    }

    wchar_t aaa [64];
    std::swprintf (aaa, 64, L"DPI %u/%u, SCALE %u", this->dpi, dpiNULL, scale.GetCurrentScale ());
    SetDlgItemText (this->hWnd, ID::EDITOR, aaa);
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

