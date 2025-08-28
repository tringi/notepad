#ifndef NOTEPAD_WINDOW_HPP
#define NOTEPAD_WINDOW_HPP

#include "Windows/Windows_Window.hpp"
#include "Notepad_File.hpp"

class Window
    : private Windows::Window
    , private File {

public:
    inline static ATOM atom = NULL;
    inline static Window * dark_menu_tracking = nullptr;

private:
    inline static HMENU menu = NULL;
    inline static HMENU wndmenu = NULL;
    inline static HMODULE rsrc = NULL;
    inline static HINSTANCE instance = NULL;

    struct {
        SIZE statusbar = { 0, 0 };
    } minimum;

    RECT rBeforeFullscreen {};
    bool bFullscreen = false;

    std::uint8_t menu_size = 0;
    std::uint8_t active_menu = ~0;
    std::uint8_t next_menu = ~0;
    bool bInSubMenu = false;
    bool bOnSubMenu = false;
    bool bMenuAccelerators = false;
    HMENU hActiveMenu = NULL;

public:
    struct ID {
        enum : WORD {
            FIRST_MENU_CMD = 0xA0,
            MENU_CMD_TOPMOST = 0xA0,

            EDITOR = 0xB0,
            MENUBAR = 0xB1,
            MENUNOTE = 0xFB,
            SEPARATOR = 0xBE,
            STATUSBAR = 0xBF,
        };
    };
    struct Message {
        enum : WORD {
            ShowAccelerators = WM_USER + 1,
            UpdateFontSize = WM_USER + 2,
            UpdateSettings = WM_USER + 3,
        };
    };
    struct CopyCode {
        enum : ULONG_PTR {
            OpenFileCheck = 1,
        };
    };
    struct TimerID {
        enum : UINT_PTR {
            DarkMenuBarTracking = 1,
        };
    };

    static ATOM InitAtom (HINSTANCE hInstance);
    static ATOM Initialize (HINSTANCE hInstance);

public:
    Window ();

private:
    virtual LRESULT OnCreate (const CREATESTRUCT *) override;
    virtual LRESULT OnFinalize () override;
    virtual LRESULT OnActivate (WORD activated, BOOL minimized, HWND hOther) override;
    virtual LRESULT OnTimer (WPARAM id) override;
    virtual LRESULT OnGetMinMaxInfo (MINMAXINFO *) override;
    virtual LRESULT OnPositionChange (const WINDOWPOS &) override;
    virtual LRESULT OnDpiChange (RECT * r, LONG previous) override;
    virtual LRESULT OnEraseBackground (HDC) override;
    virtual LRESULT OnDrawItem (WPARAM id, const DRAWITEMSTRUCT *) override;
    virtual LRESULT OnEndSession (WPARAM ending, LPARAM flags) override;
    virtual LRESULT OnClose (WPARAM wParam) override;
    virtual LRESULT OnCommand (HWND hChild, USHORT id, USHORT notification) override;
    virtual LRESULT OnNotify (WPARAM id, NMHDR *) override;
    virtual LRESULT OnMenuSelect (HMENU, USHORT index, WORD flags) override;
    virtual LRESULT OnMenuClose (HMENU, USHORT mf) override;
    virtual LRESULT OnVisualEnvironmentChange () override;
    virtual LRESULT OnUserMessage (UINT, WPARAM, LPARAM) override;
    virtual LRESULT OnCopyData (HWND, ULONG_PTR, const void *, std::size_t) override;

    void TrackMenu (UINT index);
    void ShowMenuAccelerators (BOOL show);
    void RecreateMenuButtons (HWND hMenuBar);
    LONG UpdateStatusBar (HWND hStatusBar, UINT dpi, SIZE size);
    HBRUSH CreateDarkMenuBarBrush ();

public:
    void OnTrackedMenuKey (HWND hMenuWindow, WPARAM vk);

    static const auto & GetPresentation () { return global; }

private:
    Window (const Window &) = delete;
    Window & operator = (const Window &) = delete;
};

#endif
