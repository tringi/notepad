#ifndef NOTEPAD_WINDOW_HPP
#define NOTEPAD_WINDOW_HPP

#include "Windows/Windows_Window.hpp"
#include "Notepad_File.hpp"

class Window
    : private Windows::Window
    , private File {

public:
    inline static ATOM atom = NULL;
private:
    inline static HMENU menu = NULL;
    inline static HMENU wndmenu = NULL;
    inline static HMODULE rsrc = NULL;
    inline static HINSTANCE instance = NULL;

    struct {
        SIZE statusbar = { 0, 0 };
    } minimum;

    std::uint8_t active_menu = ~0;

    bool bFullscreen = false;
    bool bMenuAccelerators = false;
    RECT rBeforeFullscreen {};

public:
    struct ID {
        enum : WORD {
            FIRST_MENU_CMD = 0xA0,
            MENU_CMD_TOPMOST = 0xA0,

            EDITOR = 0xB0,
            MENUBAR = 0xB1,
            SEPARATOR = 0xB2,
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

    static ATOM InitAtom (HINSTANCE hInstance);
    static ATOM Initialize (HINSTANCE hInstance);

public:
    Window ();

private:
    virtual LRESULT OnCreate (const CREATESTRUCT *) override;
    virtual LRESULT OnFinalize () override;
    virtual LRESULT OnActivate (WORD activated, BOOL minimized, HWND hOther) override;
    virtual LRESULT OnGetMinMaxInfo (MINMAXINFO *) override;
    virtual LRESULT OnPositionChange (const WINDOWPOS &) override;
    virtual LRESULT OnDpiChange (RECT * r, LONG previous) override;
    virtual LRESULT OnEraseBackground (HDC) override;
    virtual LRESULT OnDrawItem (WPARAM id, const DRAWITEMSTRUCT *) override;
    virtual LRESULT OnEndSession (WPARAM ending, LPARAM flags) override;
    virtual LRESULT OnClose (WPARAM wParam) override;
    virtual LRESULT OnCommand (HWND hChild, USHORT id, USHORT notification) override;
    virtual LRESULT OnNotify (WPARAM id, NMHDR *) override;
    virtual LRESULT OnMenuClose (HMENU, USHORT mf) override;
    virtual LRESULT OnMenuNext (WPARAM vk, MDINEXTMENU * next) override;
    virtual LRESULT OnVisualEnvironmentChange () override;
    virtual LRESULT OnUserMessage (UINT, WPARAM, LPARAM) override;
    virtual LRESULT OnCopyData (HWND, ULONG_PTR, const void *, std::size_t) override;

    void TrackMenu (UINT index);
    void ShowMenuAccelerators (BOOL show);
    void RecreateMenuButtons (HWND hMenuBar);
    LONG UpdateStatusBar (HWND hStatusBar, UINT dpi, SIZE size);

public:
    static const auto & GetPresentation () { return global; }

private:
    Window (const Window &) = delete;
    Window & operator = (const Window &) = delete;
};

#endif
