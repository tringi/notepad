#ifndef WINDOWS_WINDOWPRESENTATION_HPP
#define WINDOWS_WINDOWPRESENTATION_HPP

#include <Windows.h>
#include <dwmapi.h>

#include <map>
#include <optional>
#include "Windows_LoadIcon.hpp"

#undef small

namespace Windows {
    UINT GetDPI (HWND hWnd);

    // WindowPresentation
    //  - stores current per-Window DPI, metrics and icons; realoads all those on request
    //  - also tracks global colors and dark/contrast mode
    //
    struct WindowPresentation {
        LONG dpi = USER_DEFAULT_SCREEN_DPI;
        LONG metrics [SM_CMETRICS] = {};

        WindowPresentation (HINSTANCE, LPCWSTR icon);
        ~WindowPresentation ();

        std::optional <LRESULT> ProcessPresentationMessage (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

        LRESULT RefreshWindowPresentation (HWND hWnd, UINT dpiNULL = GetDPI (NULL));
        LRESULT OnPresentationChangeNotification (UINT message, WPARAM wParam, LPARAM lParam);

    public:
        static void Initialize ();
        static void RefreshGlobals ();
        static UINT_PTR GetGlobalRefreshNotificationMessage ();

    protected:
        template <std::size_t CacheSize = 8>
        struct IconDir {
            HICON small = NULL;
            HICON large = NULL;

            struct PerDpiIcon {
                WPARAM type = 0;
                LPARAM dpi = 0;
                HICON  icon = NULL;
            };
            struct Cache {
                PerDpiIcon data [CacheSize];
                HICON overflow = NULL;

                // purge
                //  - clears icon cache, call on DPI change
                //
                void purge ();

                // find
                //  - finds proper DPI icon in cache, or free slot
                //
                struct Result {
                    bool         found;
                    PerDpiIcon * icon;
                };
                Result find (WPARAM type, LPARAM dpi);
            } cache;

            void reload (const WindowPresentation &, HINSTANCE, LPCWSTR, UINT);
            void destroy ();

        private:
            void reload (const WindowPresentation &, HINSTANCE, LPCWSTR, UINT, HICON &, IconSize);
        };

        struct Icons {
            IconDir <8> main;
            std::map <USHORT, IconDir <1>> aux;
        } icons;

        static struct GlobalEnvironment {
            BOOL dark = false;
            BOOL contrast = false;
            BOOL prevalence = false;

            COLORREF accent = 0xFFFFFF;
            COLORREF active = 0xFFFFFF;
            COLORREF inactive = 0xFFFFFF;
            COLORREF text = 0;
            COLORREF background = 0xFFFFFF;
        } global;

    private:
        HINSTANCE wndIconInstance;
        LPCWSTR   wndIconName;

        LRESULT OnGetIcon (HWND hWnd, WPARAM wParam, LPARAM lParam);
    };
}

#endif

