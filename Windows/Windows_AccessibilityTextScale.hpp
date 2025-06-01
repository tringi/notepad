#ifndef WINDOWS_ACCESSIBILITYTEXTSCALE_HPP
#define WINDOWS_ACCESSIBILITYTEXTSCALE_HPP

#include <Windows.h>

namespace Windows {

    // TextScale
    //  - tracking the per-user "Settings > Accessibility > Text size" feature for UWP Apps
    //  - there is no documented Win32 API to get this value, so we read it from registry
    //  - we craft this notification facility because:
    //     - the OS doesn't always broadcast WM_SETTINGCHANGE message when the scale factor changes
    //     - the "Accessibility" key and "TextScaleFactor" value may not exist at first
    //
    class TextScale {
    public:
        bool initialize ();
        void terminate ();

        // Apply 
        //  - adjusts font height according to current text scale factor
        //
        inline void Apply (LOGFONT & lf) const {
            lf.lfHeight = MulDiv (lf.lfHeight, this->current, 100);
        }

        ~TextScale ();

    private:
        HKEY    hKey = NULL; // HKCU\SOFTWARE\Microsoft[\Accessibility]
        bool    parent = false; // if true, we are waiting for Accessibilty subkey to be created first
        DWORD   current = 100;
        HANDLE  hEvent = NULL;
        HANDLE  hThreadPoolWait = NULL;

        // OnEvent
        //  - should be called whenever this->hEvent gets signalled
        //  - returns 'true' if the scale factor might have changed, and application should redraw the GUI
        //
        bool    OnEvent ();

        bool    ReOpenKeys ();
        DWORD   GetCurrentTextScaleFactor () const;
    };
}

#endif
