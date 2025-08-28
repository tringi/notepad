#ifndef WINDOWS_WINDOW_HPP
#define WINDOWS_WINDOW_HPP

#include <Windows.h>
#include "Windows_WindowPresentation.hpp"

namespace Windows {

    UINT GetDPI (HWND hWnd);
    bool AreDpiApisScaled (HWND hWnd);

    // Window
    //  - basic abstraction of Win32 window
    //
    class Window
        : protected Windows::WindowPresentation {

    protected:
        HWND hWnd = NULL;

    protected:
        explicit Window (HINSTANCE, LPCWSTR icon);
        virtual ~Window ();

        // Create
        //  - creates the actual window
        //
        HWND Create (HINSTANCE hInstance, ATOM, DWORD style, DWORD extra = 0, const RECT * = NULL);

        // Default
        //  - derived class may call from handler to invoke DefWindowProc
        //
        LRESULT Default ();
        LRESULT Default (WPARAM, LPARAM);

        virtual LRESULT OnException ();
        virtual LRESULT OnException (const std::exception &);
    
        virtual LRESULT OnCreate (const CREATESTRUCT *);
        virtual LRESULT OnClose (WPARAM code);
        virtual LRESULT OnDestroy ();
        virtual LRESULT OnFinalize (); // WM_NCDESTROY
        virtual LRESULT OnActivate (WORD activated, BOOL minimized, HWND hOther);
        virtual LRESULT OnTimer (WPARAM id);
        virtual LRESULT OnKey (WPARAM vk, BOOL down, USHORT repeat);
        virtual LRESULT OnChar (WPARAM, LPARAM);
        virtual LRESULT OnCommand (HWND hChild, USHORT id, USHORT notification);
        virtual LRESULT OnNotify (WPARAM id, NMHDR *);
        virtual LRESULT OnEnterIdle (WPARAM msgf, HWND hOwner);
        virtual LRESULT OnMenuOpen (HMENU, USHORT index, BOOL wndmenu);
        virtual LRESULT OnMenuSelect (HMENU, USHORT index, WORD flags);
        virtual LRESULT OnMenuNext (WPARAM vk, MDINEXTMENU * next);
        virtual LRESULT OnMenuClose (HMENU, USHORT mf);
        virtual LRESULT OnEndSession (WPARAM ending, LPARAM flags);
        virtual LRESULT OnCopyData (HWND hSender, ULONG_PTR code, const void * data, std::size_t size);
        virtual LRESULT OnGetMinMaxInfo (MINMAXINFO * info);
        virtual LRESULT OnPositionChange (const WINDOWPOS &);
        virtual LRESULT OnDpiChange (RECT * r, LONG previous); // this->dpi already updated
        virtual LRESULT OnDrawItem (WPARAM id, const DRAWITEMSTRUCT *);
        virtual LRESULT OnCtlColor (HDC, HWND hCtrl);
        virtual LRESULT OnEraseBackground (HDC);
        virtual LRESULT OnVisualEnvironmentChange ();

        virtual LRESULT OnRegisteredMessage (UINT, WPARAM, LPARAM);
        virtual LRESULT OnUserMessage (UINT, WPARAM, LPARAM);
        virtual LRESULT OnAppMessage (UINT, WPARAM, LPARAM);

    public:
        explicit operator HWND () const { return this->hWnd; }

        // Initialize
        //  - initializes the Windows::Window class
        //  - returns class ATOM that can be passed to CreateWindow
        //
        static ATOM Initialize (HINSTANCE hInstance, LPCTSTR classname);

        // IsLastWindow
        //  - determines if 'this->hWnd' is last instance of its class (atom) in this thread
        //
        bool IsLastWindow () const;

    private:
        bool   linger = false;
        UINT   z_message = 0;
        WPARAM z_wParam = 0;
        LPARAM z_lParam = 0;

        LRESULT Dispatch (UINT, WPARAM, LPARAM);

        static LRESULT CALLBACK Initializer (HWND, UINT, WPARAM, LPARAM);
        static LRESULT CALLBACK Procedure (HWND, UINT, WPARAM, LPARAM);
    };

}

#endif
