#include "Notepad_Window.hpp"
#include "Notepad_Settings.hpp"

#include "Windows\Windows_Symbol.hpp"
#include "Windows\Windows_CreateSolidBrushEx.hpp"
#include "Windows\Windows_IsWindowsBuildOrGreater.hpp"
#include "Windows\Windows_DrawTextComposited.hpp"

namespace {
    inline bool IsMenuItemChecked (HMENU menu, UINT id) {
        return GetMenuState (menu, id, MF_BYCOMMAND) & MF_CHECKED;
    }
    inline bool IsWindows11OrGreater () {
        return Windows::IsWindowsBuildOrGreater (22000);
    }
}

void Window::ShowMenuAccelerators (BOOL show) {
    if (auto hMenuBar = GetDlgItem (this->hWnd, Window::ID::MENUBAR)) {
        this->bMenuAccelerators = show;
        InvalidateRect (hMenuBar, NULL, FALSE);
    }
}

LRESULT Window::OnTimerDarkMenuBarTracking (WPARAM id) {
    if (auto hMenuBar = GetDlgItem (this->hWnd, ID::MENUBAR)) {
        POINT pt;
        if (GetCursorPos (&pt)) {
            MapWindowPoints (HWND_DESKTOP, hMenuBar, &pt, 1);

            auto item = SendMessage (hMenuBar, TB_HITTEST, 0, (LPARAM) &pt);
            if (item >= 0) {
                if (this->active_menu != item) {
                    this->next_menu = (std::uint8_t) item;
                    SendMessage (this->hWnd, WM_CANCELMODE, 0, 0);
                }
            }
        }
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

HBRUSH Window::CreateDarkMenuBarBrush (bool hot) {
    if (this->global.dark) {
        if (hot) {
            COLORREF clr = 0;
            if (IsWindows11OrGreater ()) {
                if (IsMenuItemChecked (menu, 0x5A) && (GetActiveWindow () == this->hWnd)) {
                    clr = this->global.accent;
                }
            } else {
                clr = this->global.accent;
            }

            if (clr) {
                return Windows::CreateSolidBrushEx (clr, 255);
            } else {
                return CreateSolidBrush (0x000000);
            }

        } else {
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
        }
    } else
        return NULL;
}

LRESULT Window::OnNotifyMenuBar (WPARAM id, NMHDR * nm) {
    switch (nm->code) {
        case TBN_BEGINDRAG:
            this->TrackMenu (reinterpret_cast <NMTOOLBAR *> (nm)->iItem);
            break;

        case NM_CUSTOMDRAW:
            auto nmtb = reinterpret_cast <NMTBCUSTOMDRAW *> (nm);

            switch (nmtb->nmcd.dwDrawStage) {
                case CDDS_PREPAINT:
                    if (auto hBrush = this->CreateDarkMenuBarBrush (false)) {
                        FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, hBrush);
                        DeleteObject (hBrush);
                        return CDRF_NOTIFYITEMDRAW;
                    } else
                        return CDRF_DODEFAULT;

                case CDDS_ITEMPREPAINT:
                    const bool active = (GetActiveWindow () == this->hWnd);

                    if ((nmtb->nmcd.dwItemSpec == this->active_menu) || (nmtb->nmcd.uItemState & CDIS_HOT)) {
                        if (auto hBrush = this->CreateDarkMenuBarBrush (true)) {
                            FillRect (nmtb->nmcd.hdc, &nmtb->nmcd.rc, hBrush);
                            DeleteObject (hBrush);
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

    return 0;
}

LRESULT Window::OnDrawMenuNote (WPARAM id, const DRAWITEMSTRUCT * draw) {
    if (auto hBrush = this->CreateDarkMenuBarBrush (false)) {
        FillRect (draw->hDC, &draw->rcItem, hBrush);
        DeleteObject (hBrush);

        COLORREF color;
        if (GetActiveWindow () == this->hWnd) {
            color = this->global.text;
        } else {
            color = GetSysColor (COLOR_GRAYTEXT);
        }

        SetBkMode (draw->hDC, TRANSPARENT);
        Windows::DrawTextComposited (draw->hwndItem, draw->hDC, L"\xE1D8 ", 2, DT_VCENTER | DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX, color, &draw->rcItem);
        return TRUE;
    } else
        return FALSE;
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
