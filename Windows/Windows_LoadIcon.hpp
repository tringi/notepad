#ifndef WINDOWS_LOADICON_HPP
#define WINDOWS_LOADICON_HPP

#include <Windows.h>

namespace Windows {

    // IconSize
    //  -
    //
    enum class IconSize {
        Small = 0, // 16x16
        Start, // 24x24
        Large, // 32x32
        Taskbar,
        Count
    };

    // LoadBestIcon
    //  - 
    //
    HICON LoadBestIcon (HMODULE hModule, LPCWSTR resource, SIZE size);
    HICON LoadBestIcon (LPCWSTR resource, SIZE size);

    // GetIconMetrics
    //  - 
    //
    SIZE GetIconMetrics (IconSize size, const LONG * metrics, UINT dpi, UINT dpiSystem = 0);
}

#endif

