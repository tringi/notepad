#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"
#include "Windows\Windows_DrawTextComposited.hpp"
#include "Windows\Windows_IsWindowsBuildOrGreater.hpp"
#include "Windows\Windows_CreateSolidBrushEx.hpp"

#include <winioctl.h>

extern wchar_t szTmpPathBuffer [MAX_NT_PATH];

void FormatSize (ULONGLONG value, wchar_t * buffer, std::size_t length) {
    buffer [0] = L'\0';

    if (value <= 921u) {
        wchar_t string [4];
        std::swprintf (string, 4, L"%llu", value);

        wchar_t format [64];
        LoadString (GetModuleHandle (L"SHELL32"), 4113, format, 64);

        std::swprintf (buffer, length, format, string);

    } else {
        int scale = -1;
        static const char prefix [] = { 'k', 'M', 'G', 'T', 'P', 'E' };

        while (value > 1048576u) {
            value /= 1024u;
            ++scale;
        }

        double divided = (double) value;
        while ((divided > 921.0) && (scale < (int) (sizeof prefix - 1))) {
            divided /= 1024.0;
            ++scale;
        }

        wchar_t number [8];
        std::swprintf (number, 8, L"%.2f", divided);

        if (auto n = GetNumberFormatEx (LOCALE_NAME_USER_DEFAULT, 0, number, NULL, buffer, (int) length - 2)) {
            buffer [n - 1] = L'\x2008'; // punctuation space
            buffer [n - 0] = prefix [scale];
            buffer [n + 1] = L'B';
            buffer [n + 2] = L'\0';
        } else {
            std::swprintf (buffer, length, L"%s\x2008%cB", number, prefix [scale]);
        }
    }
}

LRESULT CALLBACK StatusBarTooltipSubclassProcedure (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    switch (message) {
        case WM_NOTIFY:
            if (reinterpret_cast <NMHDR *> (lParam)->code == TTN_GETDISPINFO) {
                    
                auto window = reinterpret_cast <Window *> (dwRefData);
                auto nmTT = reinterpret_cast <TOOLTIPTEXT *> (lParam);
                
                nmTT->lpszText = NULL;
                szTmpPathBuffer [0] = L'\0';

                switch (wParam) {
                    case Window::StatusBarCell::FileName:
                        if (window->handle != INVALID_HANDLE_VALUE) {
                            GetFinalPathNameByHandle (window->handle, szTmpPathBuffer, MAX_NT_PATH, VOLUME_NAME_GUID);

                            // FileStreamInfo ???
                            /*if (GetFileInformationByHandleEx (window->handle, FileRemoteProtocolInfo, &info, sizeof info)) { // Win7+

                            }*/
                        }
                        break;

                    case Window::StatusBarCell::CursorPos:nmTT->lpszText = (LPWSTR) L"Current line and column"; break;
                    case Window::StatusBarCell::FileSize:
                        if (window->handle != INVALID_HANDLE_VALUE) {

                            FILE_STANDARD_INFO info {};
                            STARTING_VCN_INPUT_BUFFER vcninput {};
                            RETRIEVAL_POINTERS_BUFFER pointers {};
                            DWORD dwRed = 0;

                            GetFileInformationByHandleEx (window->handle, FileStandardInfo, &info, sizeof info);
                            DeviceIoControl (window->handle, FSCTL_GET_RETRIEVAL_POINTERS, &vcninput, sizeof vcninput, &pointers, sizeof pointers, &dwRed, NULL);
                            
                            if (pointers.ExtentCount == 0) { // ERROR_HANDLE_EOF, small file content that is allocated only within MFT entry
                                info.AllocationSize.QuadPart = 0;
                            }

                            wchar_t ondisk [32];
                            wchar_t inmemory [32];
                            FormatSize (info.AllocationSize.QuadPart, ondisk, 32);
                            FormatSize (window->GetActualMemoryUsage (), inmemory, 32);

                            std::swprintf (szTmpPathBuffer, MAX_NT_PATH,
                                           L"Size on disk: %s; Memory usage: %s; Unsaved edits: TBD bytes, Undo memory: TBD bytes",
                                           ondisk, inmemory);
                        }
                        break;

                    case Window::StatusBarCell::ReadOnly:
                        if (!window->writable) {
                            //LoadString (NULL, ..., szTmpPathBuffer, MAX_NT_PATH);
                            nmTT->lpszText = (LPWSTR) L"...";
                        }
                        break;

                    case Window::StatusBarCell::ZoomLevel:nmTT->lpszText = (LPWSTR) L"Ctrl+Wheel"; break;
                    case Window::StatusBarCell::LineEnds: nmTT->lpszText = (LPWSTR) L"Windows mode, click to change"; break;
                    case Window::StatusBarCell::Encoding: nmTT->lpszText = (LPWSTR) L"Click to change"; break;
                }

                if (!nmTT->lpszText && szTmpPathBuffer [0] != L'\0') {
                    nmTT->lpszText = szTmpPathBuffer;
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

void Window::SetStatus (StatusBarCell::Constant cell, const wchar_t * text) {
    WPARAM wParam = SBT_OWNERDRAW | (WPARAM) cell;
    if (this->global.dark) {
        wParam |= SBT_NOBORDERS;
    }
    SendDlgItemMessage (this->hWnd, ID::STATUSBAR, SB_SETTEXT, wParam, (LPARAM) text);
}

SIZE GetClientSize (HWND hWnd);

LONG Window::UpdateStatusBar (HWND hStatusBar, UINT dpi, SIZE parent) {
    SendMessage (hStatusBar, WM_SIZE, 0, MAKELPARAM (parent.cx, parent.cy));

    SIZE status = GetClientSize (hStatusBar);

    static const short widths [] = { 64, 80, 72, 52, 48, 96, 128 };
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

    // TODO: if changes were made, number of rows changed (size in hint)

    this->SetStatus (StatusBarCell::CursorPos, L"5\x2236""16"); // TODO: click to Go To
    this->SetStatus (StatusBarCell::FileSize,  nullptr);
    this->SetStatus (StatusBarCell::ReadOnly,  nullptr);
    this->SetStatus (StatusBarCell::ZoomLevel, L"100\x200A%"); // TODO: click to Zoom
    this->SetStatus (StatusBarCell::LineEnds,  L"CR LF"); // TODO: click to change
    this->SetStatus (StatusBarCell::Encoding,  nullptr); // UTF-16 LE, click to change
    this->SetStatus (StatusBarCell::Corner,    L"");

    return status.cy;
}

LRESULT Window::OnDrawStatusBar (WPARAM id, const DRAWITEMSTRUCT * draw) {

    if (draw->itemID != StatusBarCell::Corner && this->global.dark) {
        RECT r = draw->rcItem;
        r.top += 1 * this->dpi / USER_DEFAULT_SCREEN_DPI;
        r.left = r.right - 1 * this->dpi / USER_DEFAULT_SCREEN_DPI;
        r.bottom -= 2 * this->dpi / USER_DEFAULT_SCREEN_DPI;

        COLORREF color;
        UCHAR alpha;
        if (this->bActive) {
            if (this->global.prevalence) {
                color = this->global.accent;
                alpha = this->global.accent >> 24u;
            } else {
                color = this->global.text;
                alpha = 255;
            }
        } else {
            color = GetSysColor (COLOR_GRAYTEXT);
            alpha = 192;
        }

        if (auto hBrush = Windows::CreateSolidBrushEx (color, alpha)) {
            FillRect (draw->hDC, &r, hBrush);
            DeleteObject (hBrush);
        }
    }

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
            if (this->handle != INVALID_HANDLE_VALUE) {
                string = this->File::GetCurrentFileName (szTmpPathBuffer, MAX_NT_PATH);
                if (!string) {
                    string = L"\x2370";
                }
            }
            break;
        case StatusBarCell::ReadOnly:
            if (this->handle != INVALID_HANDLE_VALUE) {
                if (!this->File::writable) {
                    string = L"Read Only";
                }
            }
            break;
        case StatusBarCell::FileSize:
            // TODO: compute size of the final file

            if (this->handle != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER size;
                if (GetFileSizeEx (this->handle, &size)) {
                    FormatSize (size.QuadPart, szTmpPathBuffer, MAX_NT_PATH);
                    string = szTmpPathBuffer;
                }
            }
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
