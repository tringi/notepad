#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"
#include "Windows\Windows_DrawTextComposited.hpp"

extern wchar_t szTmpPathBuffer [MAX_NT_PATH];

LRESULT CALLBACK StatusBarTooltipSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
        case WM_NOTIFY:
            if (reinterpret_cast <NMHDR *> (lParam)->code == TTN_GETDISPINFO) {
                    
                auto window = reinterpret_cast <Window *> (dwRefData);
                auto nmTT = reinterpret_cast <TOOLTIPTEXT *> (lParam);

                switch (wParam) {
                    case Window::StatusBarCell::FileName:
                        nmTT->szText [0] = L'\0';
                        nmTT->lpszText = NULL;

                        if (window->handle != INVALID_HANDLE_VALUE) {
                            if (GetFinalPathNameByHandle (window->handle, szTmpPathBuffer, MAX_NT_PATH, VOLUME_NAME_DOS)) {
                                nmTT->lpszText = szTmpPathBuffer;
                            }
                        }
                        return 0;

                    case Window::StatusBarCell::CursorPos:nmTT->lpszText = (LPWSTR) L"Current line and column"; return 0;
                    case Window::StatusBarCell::FileSize: nmTT->lpszText = (LPWSTR) L"Disk: 160 kB, Memory: 500 kB, 10 kB of unsaved edits, 200 kB in undo steps"; return 0;
                    case Window::StatusBarCell::ZoomLevel:nmTT->lpszText = (LPWSTR) L"Ctrl+Wheel"; return 0;
                    case Window::StatusBarCell::LineEnds: nmTT->lpszText = (LPWSTR) L"Windows mode, click to change"; return 0;
                    case Window::StatusBarCell::Encoding: nmTT->lpszText = (LPWSTR) L"Click to change"; return 0;
                }
                return 0;
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

SIZE GetClientSize (HWND hWnd);

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

    this->UpdateFileName ();

    SendMessage (hStatusBar, SB_SETTEXT, StatusBarCell::CursorPos| SBT_OWNERDRAW, (LPARAM) L"5\x2236""16"); // TODO: click to Go To
    SendMessage (hStatusBar, SB_SETTEXT, StatusBarCell::FileSize | SBT_OWNERDRAW, (LPARAM) L"240\x200AkB");
    SendMessage (hStatusBar, SB_SETTEXT, StatusBarCell::ZoomLevel| SBT_OWNERDRAW, (LPARAM) L"100\x200A%"); // TODO: click to Zoom
    SendMessage (hStatusBar, SB_SETTEXT, StatusBarCell::LineEnds | SBT_OWNERDRAW, (LPARAM) L"CR LF"); // TODO: click to change
    SendMessage (hStatusBar, SB_SETTEXT, StatusBarCell::Encoding | SBT_OWNERDRAW, 0); // UTF-16 LE, click to change

    return status.cy;
}

LRESULT Window::OnDrawStatusBar (WPARAM id, const DRAWITEMSTRUCT * draw) {
    COLORREF color;
    if (this->bActive) {
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

    LPCWSTR string = NULL;
    switch (draw->itemID) {
        case StatusBarCell::FileName:
            string = this->File::GetCurrentFileName (szTmpPathBuffer, MAX_NT_PATH);
            break;
        case StatusBarCell::CursorPos:
            
            break;
        case StatusBarCell::ZoomLevel:
            
            break;
        case StatusBarCell::LineEnds:
            
            break;
        case StatusBarCell::Encoding:
            string = L"Windows 1250";
            break;
    }

    if (draw->itemData >= 0x10000) {
        string = (LPCWSTR) draw->itemData;
    }
    if (string) {
        Windows::DrawTextComposited (draw->hwndItem, draw->hDC, string, -1, DT_VCENTER | DT_SINGLELINE | DT_PATH_ELLIPSIS | DT_NOPREFIX, color, &r);
    }
    return TRUE;
}

LRESULT Window::OnNotifyStatusBar (WPARAM id, NMHDR * nm) {
    switch (nm->code) {
        case NM_CLICK:
        case NM_DBLCLK:
        case NM_RCLICK:
            auto nmMouse = reinterpret_cast <NMMOUSE *> (nm);
            switch (nmMouse->dwItemSpec) {

                case StatusBarCell::FileName: // Filename
                    switch (nm->code) {
                        case NM_DBLCLK:
                            // default from the menu below
                            break;
                        case NM_RCLICK:
                            // menu with options:
                            //  * Open Path // default?
                            //  * Copy Full File Name
                            //  * File Properties
                            break;
                    }
                    break;
                case StatusBarCell::CursorPos:
                    switch (nm->code) {
                        case NM_CLICK:
                            // Go To Line dialog
                            break;
                    }
                    break;
                case StatusBarCell::ZoomLevel:
                    switch (nm->code) {
                        case NM_CLICK:
                            // Zoom menu
                            break;
                    }
                    break;
                case StatusBarCell::LineEnds:
                    switch (nm->code) {
                        case NM_CLICK:
                            // click to cycle between EOL conventions
                            break;
                        case NM_RCLICK:
                            // display menu with EOL types
                            break;
                    }
                    break;
                case StatusBarCell::Encoding:
                    switch (nm->code) {
                        case NM_CLICK:
                            // click to cycle between encodings
                            break;
                        case NM_RCLICK:
                            // display menu with enccoding types
                            break;
                    }
                    break;
            }
            break;
    }
    return FALSE;
}
