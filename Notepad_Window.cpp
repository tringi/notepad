#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"

#include "Windows\Windows_Symbol.hpp"
#include "Windows\Windows_AccessibilityTextScale.hpp"
#include "Windows\Windows_CreateSolidBrushEx.hpp"
#include "Windows\Windows_IsWindowsBuildOrGreater.hpp"

#include <Shobjidl.h>

extern Windows::TextScale scale;
extern ITaskbarList3 * taskbar;
extern wchar_t szTmpPathBuffer [MAX_NT_PATH];

LRESULT CALLBACK StatusBarTooltipSubclassProcedure (HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK TooltipThemeSubclassProcedure (HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK ProperBgSubclassProcedure (HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

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

    ATOM atomCOMBOBOX = 0;
}

bool IsMenuItemChecked (HMENU menu, UINT id) {
    return GetMenuState (menu, id, MF_BYCOMMAND) & MF_CHECKED;
}
bool IsWindows11OrGreater () {
    return Windows::IsWindowsBuildOrGreater (10, 0, 22000);
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

Window::Window (OpenMode mode)
    : Windows::Window (this->rsrc, MAKEINTRESOURCE (2)) {

    this->CommonConstructor (mode);
}

Window::Window (OpenMode mode, File && file)
    : Windows::Window (this->rsrc, MAKEINTRESOURCE (2))
    , File (std::move (file)) {

    this->CommonConstructor (mode);
}

void Window::CommonConstructor (OpenMode mode) {
    this->icons.aux.try_emplace (1);
    this->icons.aux.try_emplace (2);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    DWORD extra = WS_EX_APPWINDOW | WS_EX_ACCEPTFILES;

    // TODO: last position?

    auto h = this->Create (this->instance, this->atom, style, extra);
    ShowWindow (h, mode.show);
    if (mode.above) {
        SetWindowPos (h, mode.above, 0, 0, 0, 0,
                      SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    }
}

bool Window::CloakIfRequired () {

    // in dark mode, to prevent flashbang, we cloak the window
    // and uncloak it after a cycle (first repaint) has finished;
    // unfortunatelly we lose the window opening animation

    if (auto option = Settings::Get (L"Start Cloaked", Settings::Defaults::StartCloaked)) {
        if (this->global.dark || (option > 1)) { // 0 - don't cloak, 1 - cloak in dark mode, 2 - always cloak
            BOOL cloak = TRUE;
            if (SUCCEEDED (DwmSetWindowAttribute (this->hWnd, DWMWA_CLOAK, &cloak, sizeof cloak))) {
                SetTimer (this->hWnd, TimerID::UnCloak, Settings::Get (L"Cloak Duration", USER_TIMER_MINIMUM), NULL);
                return true;
            }
        }
    }
    return false;
}

LRESULT Window::OnCreate (const CREATESTRUCT * cs) {
    this->CloakIfRequired ();

    ChangeWindowMessageFilter (Window::Message::UpdateSettings, MSGFLT_ADD);

    if (IsWindows11OrGreater ()) {
        DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute (hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof backdrop);
    }

    for (auto id = 0x5A; id != 0x5C; ++id) {
        SendMessage (this->hWnd, Window::Message::UpdateSettings, 0, MAKELPARAM (id, !!IsMenuItemChecked (menu, id)));
    }

    if (auto menu = GetSystemMenu (this->hWnd, FALSE)) {
        auto place = GetMenuItemCount (menu) - 3;
        wchar_t buffer [64];

        MENUITEMINFO item;
        item.cbSize = sizeof item;
        item.fMask = MIIM_ID | MIIM_STATE | MIIM_FTYPE | MIIM_STRING;
        item.dwTypeData = buffer;

        UINT i = 0;
        do {
            item.cch = sizeof buffer / sizeof buffer [0];
        } while (GetMenuItemInfo (Window::wndmenu, i++, TRUE, &item)
                && InsertMenuItem (menu, place + i, TRUE, &item));

        if (cs->dwExStyle & WS_EX_TOPMOST) {
            CheckMenuItem (menu, ID::MENU_CMD_TOPMOST, MF_CHECKED);
        }
    }

    if (auto hMenuBar = CreateWindowEx (WS_EX_COMPOSITED, TOOLBARCLASSNAME, L"",
                                        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
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

    if (auto hMenuNote = CreateWindowEx (WS_EX_COMPOSITED, TOOLBARCLASSNAME, L"",
                                         WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
                                         | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                                         0, 0, 0, 0, hWnd, (HMENU) ID::MENUNOTE, cs->hInstance, NULL)) {

        if (IsWindows11OrGreater ()) {
            SetWindowSubclass (hMenuNote, ProperBgSubclassProcedure, 0, (DWORD_PTR) this);
        }

        SendMessage (hMenuNote, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);
        SendMessage (hMenuNote, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
        SendMessage (hMenuNote, TB_SETIMAGELIST, 0, 0);
        SendMessage (hMenuNote, TB_SETDRAWTEXTFLAGS, DT_HIDEPREFIX, DT_HIDEPREFIX);

        TBBUTTON button {};
        button.fsState = TBSTATE_ENABLED;
        button.fsStyle = BTNS_SHOWTEXT | BTNS_AUTOSIZE;
        button.iString = (INT_PTR) L"F11";
        button.idCommand = ID::MENUNOTE;

        SendMessage (hMenuNote, TB_ADDBUTTONS, 1, (LPARAM) &button);
        SendMessage (hMenuNote, TB_AUTOSIZE, 0, 0);
    }

    CreateWindowEx (0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                    0, 0, 0, 0, hWnd, (HMENU) ID::SEPARATOR, cs->hInstance, NULL);

    if (auto hEditor = CreateWindowEx (0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                       WS_HSCROLL | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
                                       0, 0, 0, 0, hWnd, (HMENU) ID::EDITOR, cs->hInstance, NULL)) {
        // This is temporary
        // TODO: WS_EX_STATICEDGE if AeroLite, nothing otherwise
    }

    auto style = WS_CHILD | SBARS_TOOLTIPS | CCS_NOPARENTALIGN | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
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

LRESULT Window::OnFinalize () {
    delete this;
    return 0;
}

LRESULT Window::OnActivate (WORD activated, BOOL minimized, HWND hOther) {
    this->bActive = activated;
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
        SIZE szMenuNote;
        SendMessage (hMenuBar, TB_GETMAXSIZE, 0, (LPARAM) &szMenuBar);
        SendMessage (hMenuNote, TB_GETMAXSIZE, 0, (LPARAM) &szMenuNote);

        auto yMenuBar = szMenuBar.cy;
        auto ySeparator = 1;
        if (!this->global.dark) {
            yMenuBar = 0;
            ySeparator = 0;
        }
        auto yStatusBar = UpdateStatusBar (hStatusBar, this->dpi, client);
        auto xMenuNote = szMenuNote.cx;
        if (!this->bFullscreen || !this->global.dark) {
            xMenuNote = 0;
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
    }
    return 0;
}

LRESULT Window::OnTimer (WPARAM id) {
    switch (id) {
        case TimerID::DarkMenuBarTracking:
            return this->OnTimerDarkMenuBarTracking (id);

        case TimerID::UnCloak:
            BOOL cloak = FALSE;
            DwmSetWindowAttribute (this->hWnd, DWMWA_CLOAK, &cloak, sizeof cloak);
            KillTimer (this->hWnd, id);

            if (this->bFullscreen) {
                // this nudges taskbar to actually hide, as cloaking prevents that for some reason
                ShowWindow (this->hWnd, SW_HIDE);
                ShowWindowAsync (this->hWnd, SW_SHOW);
            }
            break;
    }
    return 0;
}

bool Window::ToggleTopmost (int topmost) {
    if (auto menu = GetSystemMenu (this->hWnd, FALSE)) {
        bool checked = GetMenuState (menu, ID::MENU_CMD_TOPMOST, 0) & MF_CHECKED;

        if (topmost == (int) checked)
            return false;

        // TODO: set/unset taskbar button badge

        SetWindowPos (this->hWnd, checked ? HWND_NOTOPMOST : HWND_TOPMOST,
                      0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        CheckMenuItem (menu, ID::MENU_CMD_TOPMOST, checked ? 0 : MF_CHECKED);
        return true;
    } else
        return false;
}

LRESULT Window::OnCommand (HWND hChild, USHORT id, USHORT notification) {
    switch (id) {
        case IDCLOSE:
            PostMessage (this->hWnd, WM_CLOSE, 0, 0);
            break;

        case ID::MENU_CMD_TOPMOST:
            this->ToggleTopmost ();
            break;

        case ID::EDITOR: // this is temporary
            if (notification == EN_CHANGE) {
                if (!bTmpChanged) {
                    bTmpChanged = true;
                    this->UpdateFileName ();
                }
            }
            break;

        case 0x20:
        case 0x21:
        case 0x22:
        case 0x23:
        case 0x24:
        case 0x25:
            return this->OnFileOpCommand (hChild, id, notification);

        case 0x5A:
        case 0x5B:
        case 0x5F:
        case 0x6E:
        case 0x6F:
            Settings::Set (id, !IsMenuItemChecked (menu, id));
            EnumWindows ([] (HWND hWnd, LPARAM lParam) {
                             if (GetClassWord (hWnd, GCW_ATOM) == Window::atom) {
                                 PostMessage (hWnd, Window::Message::UpdateSettings, 0, lParam);
                             }
                             return TRUE;
                         },
                         MAKELPARAM (id, !IsMenuItemChecked (menu, id)));
            break;

        case 0x12:
        case 0x13:
        case 0x14:
        case 0x15:
        case 0x16:
            this->TrackMenu (id - 0x12);
            break;

        case 0xCF: // Ctrl+Alt+F4
            PostMessage (NULL, WM_COMMAND, id, 0);
            break;

        case 0xFB: // F11
            this->bFullscreen = !this->bFullscreen;
            this->CloakIfRequired ();

            if (taskbar) {
                taskbar->MarkFullscreenWindow (this->hWnd, this->bFullscreen);
            }

            if (this->bFullscreen) {
                if (IsZoomed (this->hWnd) || IsIconic (this->hWnd)) {
                    ShowWindow (this->hWnd, SW_RESTORE);
                }
                GetWindowRect (this->hWnd, &this->rBeforeFullscreen);
                SetWindowLong (this->hWnd, GWL_STYLE, WS_POPUP);
                ShowWindow (this->hWnd, SW_MAXIMIZE);

            } else {
                SetWindowLong (this->hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
                ShowWindow (this->hWnd, SW_RESTORE);
                MoveWindow (this->hWnd, rBeforeFullscreen.left, rBeforeFullscreen.top,
                            rBeforeFullscreen.right - rBeforeFullscreen.left,
                            rBeforeFullscreen.bottom - rBeforeFullscreen.top, TRUE);
            }
            break;
    }
    return 0;
}

LRESULT Window::OnNotify (WPARAM id, NMHDR * nm) {
    switch (id) {
        case ID::MENUBAR:
        case ID::MENUNOTE:
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

        case ID::STATUSBAR:
            return this->OnDrawStatusBar (id, draw);
    }
    return FALSE;
}

LRESULT Window::OnEraseBackground (HDC hDC) {
    RECT r;
    GetClientRect (this->hWnd, &r);
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

LRESULT Window::OnDpiChange (RECT *, LONG) {
    auto dpiNULL = Windows::GetDPI (NULL);
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
        SendMessage (hMenuBar, TB_SETPADDING, 0, MAKELPARAM (8 * this->dpi / dpiNULL, 4 * this->dpi / dpiNULL));
        this->RecreateMenuButtons (hMenuBar);
    }
    if (auto hMenuNote = GetDlgItem (this->hWnd, ID::MENUNOTE)) {
        SendMessage (hMenuNote, TB_SETPADDING, 0, MAKELPARAM (8 * this->dpi / dpiNULL, 4 * this->dpi / dpiNULL));
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

