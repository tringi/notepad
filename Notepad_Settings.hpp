#ifndef NOTEPAD_SETTINGS_HPP
#define NOTEPAD_SETTINGS_HPP

#include <Windows.h>

namespace Settings {
    
    BOOL  Init ();
    
    DWORD Get (unsigned int, DWORD);
    DWORD Get (const wchar_t *, DWORD);
    
    BOOL  Set (unsigned int, DWORD);
    BOOL  Set (const wchar_t *, DWORD);
}

#endif
