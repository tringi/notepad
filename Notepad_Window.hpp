#ifndef NOTEPAD_WINDOW_HPP
#define NOTEPAD_WINDOW_HPP

#include "Windows/Windows_Window.hpp"
#include "Notepad_File.hpp"

constexpr auto MAX_NT_PATH = 32768u;

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

    bool bActive = false;
    bool bInSubMenu = false;
    bool bOnSubMenu = false;
    bool bMenuAccelerators = false;
    std::uint8_t menu_size = 0;
    std::uint8_t active_menu = ~0;
    std::uint8_t next_menu = ~0;

    HMENU hActiveMenu = NULL;

    bool bTmpChanged = false; // TODO: !editor.diffs.empty()

public:
    using File::handle;
    using File::writable;
    using File::GetActualMemoryUsage;

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
            OpenFile      = 1, // data = OpenFileRequest
            OpenFileCheck = 2, // data = FILE_ID_INFO
        };
    };
    struct OpenFileRequest {
        std::int64_t handle;
        std::uint8_t nCmdShow;
        std::uint8_t reserved [7];
    };
    struct TimerID {
        enum : UINT_PTR {
            DarkMenuBarTracking = 1,
            UnCloak = 2,
        };
    };
    struct StatusBarCell {
        enum Constant : UCHAR {
            FileName = 0,
            CursorPos = 1,
            ReadOnly = 2,
            FileSize = 3,
            ZoomLevel = 4,
            LineEnds = 5,
            Encoding = 6,
            Corner = 7
        };
    };

    static ATOM InitAtom (HINSTANCE hInstance);
    static ATOM Initialize (HINSTANCE hInstance);
    static void InitializeFileOps (HINSTANCE hInstance);

public:
    struct OpenMode {
        int  show = SW_SHOWNORMAL;
        HWND above = NULL; // if set, window is shown below this hWnd

        OpenMode (int show) {
            this->show = show;
        }
        explicit OpenMode (HWND hAbove) {
            if (GetKeyState (VK_CONTROL) & 0x8000) {
                this->show = SW_SHOWNOACTIVATE;
                this->above = hAbove;
            }
        }
    };

    explicit Window (OpenMode mode);
    explicit Window (OpenMode mode, File &&);

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
    virtual LRESULT OnDropFiles (HDROP) override;

    bool ToggleTopmost (int topmost = -1);
    bool CloakIfRequired ();
    void CommonConstructor (OpenMode mode);
    void TrackMenu (UINT index);
    void ShowMenuAccelerators (BOOL show);
    void RecreateMenuButtons (HWND hMenuBar);
    void UpdateFileName ();
    void OpenFile (bool multiple);
    const wchar_t * GetSaveAsPath ();
    void SetStatus (StatusBarCell::Constant, const wchar_t *);
    LONG UpdateStatusBar (HWND hStatusBar, UINT dpi, SIZE size);
    HBRUSH CreateDarkMenuBarBrush (bool hot);

    LRESULT OnFileOpCommand (HWND hChild, USHORT id, USHORT notification);
    LRESULT OnDrawStatusBar (WPARAM id, const DRAWITEMSTRUCT * draw);
    LRESULT OnNotifyStatusBar (WPARAM id, NMHDR *);
    LRESULT OnNotifyMenuBar (WPARAM id, NMHDR *);
    LRESULT OnTimerDarkMenuBarTracking (WPARAM id);

public:
    void OnTrackedMenuKey (HWND hMenuWindow, WPARAM vk);

    static const auto & GetPresentation () { return global; }

private:
    Window (const Window &) = delete;
    Window & operator = (const Window &) = delete;
};

bool IsMenuItemChecked (HMENU menu, UINT id);
bool IsWindows11OrGreater ();

#endif
