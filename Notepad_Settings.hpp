#ifndef NOTEPAD_SETTINGS_HPP
#define NOTEPAD_SETTINGS_HPP

#include <Windows.h>

namespace Settings {
    namespace Defaults {
        constexpr DWORD SingleInstance = 1;
        constexpr DWORD StartCloaked = 1;
    };
    
    BOOL  Init ();
    
    DWORD Get (unsigned int, DWORD);
    DWORD Get (const wchar_t *, DWORD);
    
    BOOL  Set (unsigned int, DWORD);
    BOOL  Set (const wchar_t *, DWORD);

    void  ReportError (const wchar_t *, ...);
}

#endif
