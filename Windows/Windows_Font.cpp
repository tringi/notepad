#include "Windows_Font.hpp"

Windows::Font::~Font () {
    if (this->handle != NULL) {
        DeleteObject (this->handle);
    }
}

bool Windows::Font::update (LOGFONT lf) {
    if (lf.lfHeight > 0) {
        this->height_ = lf.lfHeight;
    } else {
        this->height_ = 96 * -lf.lfHeight / 72;
    }
    if (this->override.width != FW_DONTCARE) {
        lf.lfWeight = this->override.width;
    }
    if (this->override.italic) {
        lf.lfItalic = TRUE;
    }
    if (this->override.underlined) {
        lf.lfUnderline = TRUE;
    }
    if (auto hNewFont = CreateFontIndirect (&lf)) {
        if (this->handle != NULL) {
            DeleteObject (this->handle);
        }
        this->handle = hNewFont;
        return true;
    } else {
        if (this->handle == NULL) {
            this->handle = (HFONT) GetStockObject (DEFAULT_GUI_FONT);
        }
        return false;
    }
}

bool Windows::Font::update (HTHEME hTheme, int id, UINT dpi, UINT dpiNULL) {
    LOGFONT lf;
    if (GetThemeSysFont (hTheme, id, &lf) == S_OK) {
        lf.lfHeight = MulDiv (lf.lfHeight, dpi, dpiNULL);
        return this->update (lf);
    } else
        return false;
}
