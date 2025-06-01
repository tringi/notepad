#ifndef WINDOWS_FONT_HPP
#define WINDOWS_FONT_HPP

#include <Windows.h>
#include <Uxtheme.h>

namespace Windows {

    // Font
    //  - 
    //
    class Font {
        HFONT handle = NULL;
        long  height_;

    public:
        const long & height = height_; // pixel height (row) to use when repositioning controls on window resize/restore
        operator HFONT () const { return this->handle; }

        // override
        //  - 
        //
        struct {
            SHORT width = FW_DONTCARE;
            bool  italic = false;
            bool  underlined = false;
        } override;

        // update
        //  - 
        //
        bool update (LOGFONT);
        bool update (HTHEME hTheme, int id, UINT dpi, UINT dpiNULL);

        ~Font ();
    };
}

#endif

