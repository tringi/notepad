#include "Notepad_Window.hpp"
#include "Windows\Windows_Symbol.hpp"

namespace {
    HMODULE LoadIconModule () {
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

    LRESULT CALLBACK ProperBgSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
        auto self = reinterpret_cast <Window *> (dwRefData);

        switch (message) {
            case WM_ERASEBKGND:
                if (self->IsDark ()) {
                    RECT r;
                    if (GetClientRect (hWnd, &r)) {
                        BOOL enabled;
                        HBRUSH hBrush;
                        if (SUCCEEDED (DwmIsCompositionEnabled (&enabled)) && enabled) {
                            hBrush = (HBRUSH) GetStockObject (BLACK_BRUSH);
                        } else {
                            hBrush = (HBRUSH) SendMessage (GetParent (hWnd), WM_CTLCOLORBTN, wParam, (LPARAM) hWnd);
                        }
                        FillRect ((HDC) wParam, &r, hBrush);
                        return TRUE;
                    }
                }
        }
        return DefSubclassProc (hWnd, message, wParam, lParam);
    }

}

ATOM Window::Initialize (HINSTANCE hInstance) {
    instance = hInstance;
    
    Window::atom = Windows::Window::Initialize (hInstance, L"TRIMCORE.NOTEPAD");
    Window::menu = LoadMenu (hInstance, MAKEINTRESOURCE (1));
    Window::iconsrc = LoadIconModule ();

    atomCOMBOBOX = GetClassAtom (L"COMBOBOX");
    return Window::atom;
}

Window::Window ()
    : Windows::Window (this->iconsrc, MAKEINTRESOURCE (2)) {

    this->icons.aux.try_emplace (1);

    DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE;
    DWORD extra = WS_EX_COMPOSITED;
    
    // TODO: resolve window position (empty => last closed, file => load from file's ADS), ensure visible on screen

    auto hWnd = this->Create (this->instance, this->atom, style, extra /* &r */);
}

LRESULT Window::OnCreate (const CREATESTRUCT * cs) {

    if (auto menu = GetSystemMenu (this->hWnd, FALSE)) {
        AppendMenu (menu, MF_SEPARATOR, 0, NULL);

        for (SHORT id = ID::FIRST_MENU_CMD; ; ++id) {
            wchar_t buffer [64];
            if (LoadString (this->instance, id, buffer, sizeof buffer / sizeof buffer [0])) {
                AppendMenu (menu, MF_STRING, id, buffer);
            } else
                break;
        }
        if (cs->dwExStyle & WS_EX_TOPMOST) {
            CheckMenuItem (menu, ID::MENU_CMD_TOPMOST, MF_CHECKED);
        }
    }

    if (auto hMenuBar = CreateWindowEx (WS_EX_COMPOSITED, TOOLBARCLASSNAME, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS
                                        | CCS_NODIVIDER | CCS_NOPARENTALIGN | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT,
                                        0, 0, 0, 0, hWnd, (HMENU) ID::MENUBAR, cs->hInstance, NULL)) {

        SetWindowSubclass (hMenuBar, ProperBgSubclassProcedure, 0, 0); // W7 & W11+

        SendMessage (hMenuBar, TB_BUTTONSTRUCTSIZE, sizeof (TBBUTTON), 0);
        SendMessage (hMenuBar, TB_SETEXTENDEDSTYLE, 0, TBSTYLE_EX_MIXEDBUTTONS);
        SendMessage (hMenuBar, TB_SETIMAGELIST, 0, 0);
        SendMessage (hMenuBar, TB_SETDRAWTEXTFLAGS, DT_HIDEPREFIX, DT_HIDEPREFIX);

        this->RecreateMenuButtons (hMenuBar);
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

        SetWindowSubclass (hStatusBar, ProperBgSubclassProcedure, 0, (DWORD_PTR) this); // W7 & W11+

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
        SetFocus (GetDlgItem (hWnd, (int) ID::EDITOR));
    }
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
    } else {
        DestroyWindow (this->hWnd);
    }
    return 0;
}

LRESULT Window::OnGetMinMaxInfo (MINMAXINFO * mmi) {
    if (this->minimum.statusbar.cx) {
        mmi->ptMinTrackSize.x = this->minimum.statusbar.cx + 64;
    }
    mmi->ptMinTrackSize.y = 64 + this->minimum.statusbar.cy + 0;
    return 0;
}

LRESULT Window::OnDrawItem (WPARAM id, const DRAWITEMSTRUCT * draw) {
    switch (id) {
        case ID::STATUSBAR:
            // SetDCBrushColor (draw->hDC, this->global.inactive); // ???
            // FillRect (draw->hDC, &draw->rcItem, (HBRUSH) GetStockObject (DC_BRUSH));

            /*DrawCompositedTextOptions options;
            options.font = this->fonts.text.handle;
            options.theme = GetWindowTheme (draw->hwndItem);
            this->GetCaptionTextColor (NULL, options.color, options.glow);
            draw->rcItem.top += 1;
            draw->rcItem.left += this->metrics [SM_CXPADDEDBORDER];
            draw->rcItem.right -= this->metrics [SM_CXFIXEDFRAME];
            if (SendMessage (draw->hwndItem, SB_GETICON, draw->itemID, 0)) {
                draw->rcItem.left += this->metrics [SM_CXSMICON] + this->metrics [SM_CXFIXEDFRAME];
            }
            SetBkMode (draw->hDC, TRANSPARENT);
            DrawCompositedText (draw->hDC, (LPWSTR) draw->itemData, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS, draw->rcItem, &options);*/
            return TRUE;

        case ID::SEPARATOR:
            SetDCBrushColor (draw->hDC, this->global.dark ? this->global.inactive : GetSysColor (COLOR_3DFACE));
            FillRect (draw->hDC, &draw->rcItem, (HBRUSH) GetStockObject (DC_BRUSH));
            return TRUE;
        default:
            return FALSE;
    }
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

    static const short widths [] = { 32, 128, 128, 128 };
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

    try {
        SendMessage (hStatusBar, SB_SETICON, n, (LPARAM) this->icons.aux.at (1).small);
    } catch (...) {}
    return status.cy;
}

void DeferWindowPos (HDWP & hDwp, HWND hCtrl, const RECT & r, UINT flags = 0) {
    if (hCtrl) {
        hDwp = DeferWindowPos (hDwp, hCtrl, NULL, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_NOZORDER | flags);
    }
}

LRESULT Window::OnPositionChange (const WINDOWPOS & position) {
    if (!(position.flags & SWP_NOSIZE) || (position.flags & (SWP_SHOWWINDOW | SWP_FRAMECHANGED))) {

        auto client = GetClientSize (hWnd);
        auto hEditor = GetDlgItem (hWnd, (int) ID::EDITOR);
        auto hMenuBar = GetDlgItem (hWnd, (int) ID::MENUBAR);
        auto hSeparator = GetDlgItem (hWnd, (int) ID::SEPARATOR);
        auto hStatusBar = GetDlgItem (hWnd, (int) ID::STATUSBAR);

        SIZE szMenuBar;
        SendMessage (hMenuBar, TB_GETMAXSIZE, 0, (LPARAM) &szMenuBar);

        auto yMenuBar = szMenuBar.cy;
        auto ySeparator = 1;
        auto yStatusBar = UpdateStatusBar (hStatusBar, this->dpi, client);

        if (HDWP hDwp = BeginDeferWindowPos (4)) {
            DeferWindowPos (hDwp, hMenuBar,   { 0, 0, client.cx, yMenuBar }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hSeparator, { 0, yMenuBar, client.cx, yMenuBar + ySeparator }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hEditor,    { 0, yMenuBar + ySeparator, client.cx, client.cy - yStatusBar }, SWP_SHOWWINDOW);
            DeferWindowPos (hDwp, hStatusBar, { 0, client.cy - yStatusBar, client.cx, client.cy }, SWP_SHOWWINDOW);
            EndDeferWindowPos (hDwp);
        }

        MARGINS margins = { 0,0,0,0 };
        margins.cyBottomHeight = yStatusBar;

        DwmExtendFrameIntoClientArea (hWnd, &margins);
        InvalidateRect (hWnd, NULL, TRUE);
    }
    return 0;
}

void Window::ShowMenuAccelerators (BOOL show) {
    if (auto hMenuBar = GetDlgItem (this->hWnd, Window::ID::MENUBAR)) {
        SendMessage (hMenuBar, TB_SETDRAWTEXTFLAGS, DT_HIDEPREFIX, show ? 0 : DT_HIDEPREFIX);
        InvalidateRect (hMenuBar, NULL, TRUE);
    }
}

void Window::TrackMenu (UINT index) {
    RECT r {};
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {

        if (SendMessage (hMenuBar, TB_GETITEMRECT, index, (LPARAM) &r)) {
            MapWindowPoints (this->hWnd, NULL, (POINT *) &r, 2);

            if (auto hSubMenu = GetSubMenu (this->menu, index)) {
                UINT style = 0;
                if (GetSystemMetrics (SM_MENUDROPALIGNMENT)) {
                    style |= TPM_RIGHTALIGN;
                }

                this->active_menu = index;
                InvalidateRect (hMenuBar, NULL, TRUE);

                TrackPopupMenu (hSubMenu, style, r.left, r.bottom, 0, this->hWnd, NULL);

                this->active_menu = ~0;
                InvalidateRect (hMenuBar, NULL, TRUE);
            }
        }
    }
}
LRESULT Window::OnMenuNext (WPARAM vk, MDINEXTMENU * next) {
    // TODO
    return 0;
}
LRESULT Window::OnMenuClose (HMENU, USHORT) {
    this->ShowMenuAccelerators (false);
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

        case 0x1001:
            new Window;
            break;

        case 0x10: case 0x11: case 0x12: case 0x13:
        case 0x14: case 0x15: case 0x16: case 0x17:
        case 0x18: case 0x19: case 0x1A: case 0x1B:
        case 0x1C: case 0x1D: case 0x1E: case 0x1F:
            this->TrackMenu (id - 0x10);
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
            switch (nm->code) {
                case TBN_BEGINDRAG:
                    this->TrackMenu (reinterpret_cast <NMTOOLBAR *> (nm)->iItem);
                    break;

                case NM_CUSTOMDRAW:
                    auto nmtb = reinterpret_cast <NMTBCUSTOMDRAW *> (nm);

                    switch (nmtb->nmcd.dwDrawStage) {
                        case CDDS_PREPAINT:
                            SetDCBrushColor (nmtb->nmcd.hdc,this->global.background);
                            FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, (HBRUSH) GetStockObject (DC_BRUSH));
                            nmtb->clrText = this->global.text;
                            return CDRF_NOTIFYITEMDRAW;

                        case CDDS_ITEMPREPAINT:
                            nmtb->clrText = this->global.text;

                            if (nmtb->nmcd.dwItemSpec == this->active_menu) {
                                nmtb->nmcd.uItemState = CDIS_HOT | CDIS_SELECTED;
                            }
                            if (this->global.dark) {
                                nmtb->clrHighlightHotTrack = GetSysColor (COLOR_MENUHILIGHT); // TODO: get from UXTHEME or (this->global.accent & 0x00FFFFFF)
                                return TBCDRF_USECDCOLORS | TBCDRF_NOOFFSET | TBCDRF_HILITEHOTTRACK;
                            } else {
                                return TBCDRF_USECDCOLORS | TBCDRF_NOOFFSET;
                            }
                    }
            }
            break;
    }
    return 0;
}

LRESULT Window::OnUserMessage (UINT message, WPARAM, LPARAM) {
    switch (message) {
        case Message::ShowAccelerators:
            this->ShowMenuAccelerators (true);
            break;
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

    for (auto i = 0u; ; ++i) {
        if (GetMenuString (Window::menu, i, text, sizeof text / sizeof text [0], MF_BYPOSITION)) {
            button.idCommand = i;
            SendMessage (hMenuBar, TB_ADDBUTTONS, 1, (LPARAM) &button);
        } else
            break;
    }
    SendMessage (hMenuBar, TB_AUTOSIZE, 0, 0);
}

LRESULT Window::OnDpiChange (RECT * r, LONG) {
    auto dpiNULL = Windows::GetDPI (NULL);
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
        SendMessage (hMenuBar, TB_SETPADDING, 0, MAKELPARAM (8 * this->dpi / dpiNULL, 4 * this->dpi / dpiNULL));
        this->RecreateMenuButtons (hMenuBar);
    }

    wchar_t aaa [64];
    std::swprintf (aaa, 64, L"%u %u", this->dpi, dpiNULL);
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
    return FALSE;
}

