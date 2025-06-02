#include "Windows_WindowPresentation.hpp"
#include "Windows_GetCurrentModuleHandle.hpp"
#include "Windows_Symbol.hpp"

Windows::WindowPresentation::GlobalEnvironment Windows::WindowPresentation::global;

namespace {
    struct {
        DWORD major = 0;
        DWORD minor = 0;
        DWORD build = 0;
    } winver;

    extern "C" void WINAPI RtlGetNtVersionNumbers (LPDWORD, LPDWORD, LPDWORD); // NTDLL

    struct CompositionAttributeData {
        ULONG attr;
        PVOID data;
        ULONG size;
    };

    void (WINAPI * ptrFlushMenuThemes) () = NULL;
    BOOL (WINAPI * ptrAllowDarkModeForWindow) (HWND, BOOL) = NULL;
    BOOL (WINAPI * ptrSetWindowCompositionAttribute) (HWND, CompositionAttributeData *) = NULL;
    int (WINAPI * ptrGetSystemMetricsForDpi) (int, UINT) = NULL;

    UINT_PTR GlobalRefreshTimerId = 0;
    UINT_PTR GlobalRefreshMessage = 0;

    void CALLBACK GuiChangesCoalescingTimer (HWND hWnd, UINT, UINT_PTR id, DWORD) {
        GlobalRefreshTimerId = 0;
        KillTimer (hWnd, id);

        Windows::WindowPresentation::RefreshGlobals ();
        EnumThreadWindows (GetCurrentThreadId (),
                           [] (HWND hWnd, LPARAM)->BOOL {
                               return PostMessage (hWnd, (UINT) GlobalRefreshMessage, 0, 0);
                           }, 0);
    }
}

UINT_PTR Windows::WindowPresentation::GetGlobalRefreshNotificationMessage () {
    return GlobalRefreshMessage;
}

LRESULT Windows::WindowPresentation::RefreshWindowPresentation (HWND hWnd, UINT dpiNULL) {

    // dark mode

    if (ptrAllowDarkModeForWindow) {
        ptrAllowDarkModeForWindow (hWnd, global.dark);
    }

    if (winver.build >= 22543) {
        DWM_SYSTEMBACKDROP_TYPE backdrop = DWMSBT_MAINWINDOW;
        DwmSetWindowAttribute (hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof backdrop);
    }

    if (winver.build >= 20161) {
        DwmSetWindowAttribute (hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &global.dark, sizeof global.dark);
    } else
    if (winver.build >= 18875 && ptrSetWindowCompositionAttribute) {
        CompositionAttributeData attr = { 26, &global.dark, sizeof global.dark }; // WCA_USEDARKMODECOLORS
        ptrSetWindowCompositionAttribute (hWnd, &attr);
    } else
    if (winver.build >= 14393) {
        DwmSetWindowAttribute (hWnd, 0x13, &global.dark, sizeof global.dark);
    }

    // metrics

    if (ptrGetSystemMetricsForDpi) {
        for (auto i = 0; i != sizeof metrics / sizeof metrics [0]; ++i) {
            this->metrics [i] = ptrGetSystemMetricsForDpi (i, this->dpi);
        }
    } else {
        for (auto i = 0; i != sizeof metrics / sizeof metrics [0]; ++i) {
            this->metrics [i] = this->dpi * GetSystemMetrics (i) / dpiNULL;
        }
    }

    // icons

    this->icons.main.reload (*this, this->wndIconInstance, this->wndIconName, dpiNULL);
    for (auto & icon : this->icons.aux) {
        icon.second.reload (*this, Windows::GetCurrentModuleHandle (), MAKEINTRESOURCE (icon.first), dpiNULL);
    }
    
    // set primary pair of icons for the window

    SendMessage (hWnd, WM_SETICON, ICON_SMALL, (LPARAM) this->icons.main.small);
    SendMessage (hWnd, WM_SETICON, ICON_BIG, (LPARAM) this->icons.main.large);
    return 0;
}

LRESULT Windows::WindowPresentation::OnGetIcon (HWND hWnd, WPARAM wParam, LPARAM lParam) {
    if (lParam && (lParam != this->dpi) && (wParam <= ICON_SMALL2)) {

        // OS (taskbars on different displays) or other app asked for icon in different DPI
        if (auto [found, data] = this->icons.main.cache.find (wParam, lParam); found) {
            return (LRESULT) data->icon;

        } else {
            SIZE size;
            switch (wParam) {
                default:
                case ICON_BIG:
                    size.cx = LONG (lParam) * GetSystemMetrics (SM_CXICON) / 96;
                    size.cy = LONG (lParam) * GetSystemMetrics (SM_CYICON) / 96;
                    break;
                case ICON_SMALL:
                case ICON_SMALL2:
                    size.cx = LONG (lParam) * GetSystemMetrics (SM_CXSMICON) / 96;
                    size.cy = LONG (lParam) * GetSystemMetrics (SM_CYSMICON) / 96;
                    break;
            }
            if (auto icon = LoadBestIcon (this->wndIconInstance, this->wndIconName, size)) {
                if (data) {
                    data->type = wParam;
                    data->dpi = lParam;
                    data->icon = icon;
                } else {
                    if (this->icons.main.cache.overflow) {
                        DestroyIcon (this->icons.main.cache.overflow);
                    }
                    this->icons.main.cache.overflow = icon;
                }
                return (LRESULT) icon;
            }
        }
    } else {
        switch (wParam) {
            case ICON_SMALL2:
                return (LRESULT) this->icons.main.small;
        }
    }
    return DefWindowProc (hWnd, WM_GETICON, wParam, lParam);
}

std::optional <LRESULT> Windows::WindowPresentation::ProcessPresentationMessage (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_GETICON:
            return this->OnGetIcon (hWnd, wParam, lParam);
    }
    if (GlobalRefreshMessage) {
        if (message == GlobalRefreshMessage) {
            this->RefreshWindowPresentation (hWnd);
            InvalidateRect (hWnd, NULL, TRUE);
        }
    }
    return {};
}

LRESULT Windows::WindowPresentation::OnPresentationChangeNotification (UINT message, WPARAM, LPARAM) {
    GlobalRefreshTimerId = SetTimer (NULL, GlobalRefreshTimerId, 500, GuiChangesCoalescingTimer);
    return 0;
}

template <std::size_t CacheSize>
void Windows::WindowPresentation::IconDir <CacheSize> ::reload (const WindowPresentation & presentation, HINSTANCE hInstance, LPCWSTR name, UINT dpiNULL,
                                                                HICON & x, IconSize size) {
    if (auto icon = LoadBestIcon (hInstance, name,
                                  Windows::GetIconMetrics (size, presentation.metrics, presentation.dpi, dpiNULL))) {
        if (x) {
            DestroyIcon (x);
        }
        x = icon;
    }
}

template <std::size_t CacheSize>
void Windows::WindowPresentation::IconDir <CacheSize> ::reload (const WindowPresentation & presentation, HINSTANCE hInstance, LPCWSTR name, UINT dpiNULL) {
    this->reload (presentation, hInstance, name, dpiNULL, this->large, IconSize::Large);
    this->reload (presentation, hInstance, name, dpiNULL, this->small, IconSize::Small);
    this->cache.purge ();
}

template <std::size_t CacheSize>
void Windows::WindowPresentation::IconDir <CacheSize> ::Cache::purge () {
    for (auto & item : this->data) {
        if (item.icon) {
            DestroyIcon (item.icon);

            item.type = 0;
            item.dpi = 0;
            item.icon = NULL;
        }
    }
    if (this->overflow) {
        DestroyIcon (this->overflow);
        this->overflow = NULL;
    }
}

template <std::size_t CacheSize>
Windows::WindowPresentation::IconDir <CacheSize> ::Cache::Result
Windows::WindowPresentation::IconDir <CacheSize> ::Cache::find (WPARAM type, LPARAM dpi) {
    for (auto & icon : this->data) {
        if ((icon.type == type) && (icon.dpi == dpi))
            return { true, &icon };

        if (icon.dpi == 0)
            return { false, &icon };
    }
    return { false, nullptr };
}

template <std::size_t CacheSize>
void Windows::WindowPresentation::IconDir <CacheSize> ::destroy () {
    if (this->small) DestroyIcon (this->small);
    if (this->large) DestroyIcon (this->large);
    this->cache.purge ();
}

void Windows::WindowPresentation::Initialize () {
    if (HMODULE hUser32 = GetModuleHandle (L"USER32")) {
        Windows::Symbol (hUser32, ptrGetSystemMetricsForDpi, "GetSystemMetricsForDpi");
        Windows::Symbol (hUser32, ptrSetWindowCompositionAttribute, "SetWindowCompositionAttribute");
    }

    RtlGetNtVersionNumbers (&winver.major, &winver.minor, &winver.build);
    winver.build = winver.build & 0x0FFF'FFFF;

    if (HMODULE hUxTheme = GetModuleHandle (L"UXTHEME")) {
        if (winver.build >= 17763) {
            Windows::Symbol (hUxTheme, ptrFlushMenuThemes, 136);
            Windows::Symbol (hUxTheme, ptrAllowDarkModeForWindow, 133);

            BOOL (WINAPI * ptrAllowDarkModeForApp) (BOOL) = NULL;
            if (Windows::Symbol (hUxTheme, ptrAllowDarkModeForApp, 135)) {
                ptrAllowDarkModeForApp (true);
            }
        }
    }

    GlobalRefreshMessage = RegisterWindowMessage (L"Refresh");
    return Windows::WindowPresentation::RefreshGlobals ();
}

void Windows::WindowPresentation::RefreshGlobals () {

    // high contrast mode

    HIGHCONTRAST hc;
    hc.cbSize = sizeof hc;
    if (SystemParametersInfo (SPI_GETHIGHCONTRAST, sizeof hc, &hc, 0)) {
        global.contrast = hc.dwFlags & HCF_HIGHCONTRASTON;
    } else {
        global.contrast = FALSE;
    }

    // colorization

    BOOL opaque = TRUE;
    DWORD color;
    if (DwmGetColorizationColor (&color, &opaque) == S_OK) {
        global.active = RGB (GetBValue (color), GetGValue (color), GetRValue (color));
    } else {
        global.active = GetSysColor (COLOR_WINDOW);
    }
    global.inactive = global.active;

    // treat AeroLite as high contrast

    BOOL compositionEnabled = FALSE;
    DwmIsCompositionEnabled (&compositionEnabled);

    if (!compositionEnabled) {
        wchar_t filename [MAX_PATH];
        if (GetCurrentThemeName (filename, MAX_PATH, NULL, 0, NULL, 0) == S_OK) {
            if (std::wcsstr (filename, L"AeroLite.msstyles")) {
                global.contrast = true;
            }
        }
    }

    if (global.contrast) {
        global.dark = FALSE;
        global.prevalence = true;
        global.active = GetSysColor (COLOR_ACTIVECAPTION);
        global.inactive = GetSysColor (COLOR_INACTIVECAPTION);
    } else {
        if (winver.major >= 10) {
            global.prevalence = false;
            global.active = GetSysColor (COLOR_WINDOW); // 0xFFFFFF;

            // dark mode

            if (ptrAllowDarkModeForWindow) {
                DWORD light;
                DWORD size = sizeof light;
                if (RegGetValue (HKEY_CURRENT_USER,
                                 L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                                 RRF_RT_REG_DWORD, NULL, reinterpret_cast <LPBYTE> (&light), &size) == ERROR_SUCCESS) {
                    global.dark = !light;
                } else {
                    global.dark = FALSE;
                }
            } else {
                global.dark = FALSE;
            }


            HKEY hKey;
            if (RegOpenKeyEx (HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\DWM", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
                DWORD value = TRUE;
                DWORD size = sizeof value;
                RegQueryValueEx (hKey, L"AccentColor", NULL, NULL, reinterpret_cast <LPBYTE> (&global.accent), &size);

                if (RegQueryValueEx (hKey, L"ColorPrevalence", NULL, NULL, reinterpret_cast <LPBYTE> (&value), &size) == ERROR_SUCCESS) {
                    global.prevalence = value;
                }

                if (global.prevalence) {
                    global.active = global.accent;
                } else {
                    if (global.dark) {
                        global.active = 0x000000;
                    } else {
                        global.active = GetSysColor (COLOR_WINDOW);
                    }
                }

                if (global.dark) {
                    global.inactive = 0x2B2B2B; // TODO: retrieve from theme, now hardcoded to match Win10 1809
                } else {
                    global.inactive = GetSysColor (COLOR_WINDOW);
                }
                if (RegQueryValueEx (hKey, L"AccentColorInactive", NULL, NULL, reinterpret_cast <LPBYTE> (&value), &size) == ERROR_SUCCESS) {
                    if (global.prevalence) {
                        global.inactive = value;
                    }
                }
                RegCloseKey (hKey);
            }
        }
    }

    if (global.dark) {
        global.background = 0x121110; // TODO: retrieve from theme if possible?
        global.text = 0xEEEEEE;
    } else {
        global.background = GetSysColor (COLOR_WINDOW);
        global.text = GetSysColor (COLOR_WINDOWTEXT);
    }
}

Windows::WindowPresentation::WindowPresentation (HINSTANCE hInstance, LPCWSTR icon)
    : wndIconInstance (hInstance)
    , wndIconName (icon) {}

Windows::WindowPresentation::~WindowPresentation () {
    this->icons.main.destroy ();
    for (auto & icon : this->icons.aux) {
        icon.second.destroy ();
    }
    this->icons.aux.clear ();
}
