#include "Windows_DrawTextComposited.hpp"

HRESULT Windows::DrawTextComposited (HWND hWnd, HDC hDC, LPCWSTR text, int length, DWORD format, COLORREF color, const RECT * r) {

    LONG padding = 0; // glow;
    HFONT font = (HFONT) SendMessage (hWnd, WM_GETFONT, 0, 0);
    HTHEME theme = GetWindowTheme (hWnd);

    if (HDC hMemoryDC = CreateCompatibleDC (hDC)) {

        BITMAPINFO info {};
        info.bmiHeader.biSize = sizeof info;
        info.bmiHeader.biWidth = 2 * padding + r->right - r->left + 1;
        info.bmiHeader.biHeight = -(2 * padding + r->bottom - r->top + 1);
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;

        void * bits = nullptr;
        if (HBITMAP dib = CreateDIBSection (hDC, &info, DIB_RGB_COLORS, &bits, NULL, 0u)) {

            auto hOldBitmap = SelectObject (hMemoryDC, dib);
            auto hOldFont = SelectObject (hMemoryDC, font ? font : GetStockObject (DEFAULT_GUI_FONT));

            RECT bounds = {
                padding, padding,
                padding + r->right - r->left,
                padding + r->bottom - r->top
            };
            if (GetBkMode (hDC) == TRANSPARENT) {
                BitBlt (hMemoryDC,
                        0, 0, bounds.right + padding, bounds.bottom + padding,
                        hDC, r->left - padding, r->top - padding, SRCCOPY);
            }

            DTTOPTS dtt;
            dtt.dwSize = sizeof dtt;
            dtt.dwFlags = DTT_COMPOSITED | DTT_TEXTCOLOR;

            /*if (glow) {
                dtt.dwFlags |= DTT_GLOWSIZE;
                dtt.iGlowSize = options->glow;
            }*/

            dtt.crText = color;
            auto result = DrawThemeTextEx (theme, hMemoryDC, 0, 0, text, length, format & ~DT_CALCRECT, &bounds, &dtt);

            if (SUCCEEDED (result)) {
                BitBlt (hDC, r->left - padding, r->top - padding,
                        bounds.right + padding, bounds.bottom + padding,
                        hMemoryDC, 0, 0, SRCCOPY);
            }

            if (hOldBitmap) {
                SelectObject (hMemoryDC, hOldBitmap);
            }
            if (hOldFont) {
                SelectObject (hMemoryDC, hOldFont);
            }
            DeleteObject (dib);
            DeleteDC (hMemoryDC);
            return result;
        } else {
            DeleteDC (hMemoryDC);
            return GetLastError ();
        }
    } else
        return GetLastError ();
}
