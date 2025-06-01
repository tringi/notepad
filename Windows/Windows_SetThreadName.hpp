#ifndef WINDOWS_SETTHREADNAME_HPP
#define WINDOWS_SETTHREADNAME_HPP

#include <windows.h>

namespace Windows {

    // SetThreadName
    //  - sets thread description (on systems that support it)
    //  - if debugging is enabled then sets thread debugging name
    //
    bool SetThreadName (HANDLE h, const wchar_t * name);
    bool SetThreadName (DWORD id, const wchar_t * name);
    bool SetThreadName (const wchar_t * name);
}

#endif
