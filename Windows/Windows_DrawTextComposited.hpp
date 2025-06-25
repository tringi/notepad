#ifndef WINDOWS_DRAWTEXTCOMPOSITED_HPP
#define WINDOWS_DRAWTEXTCOMPOSITED_HPP

#include <Windows.h>
#include <Uxtheme.h>

namespace Windows {

    // DrawTextComposited
    //  - abstracts composited DrawThemeTextEx into DIB for rendering onto translucent surfaces
    //
    HRESULT DrawTextComposited (HWND, HDC, LPCWSTR, int, DWORD, COLORREF, const RECT *);
}

#endif
