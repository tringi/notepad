#ifndef WINDOWS_GETCURRENTMODULEHANDLE_HPP
#define WINDOWS_GETCURRENTMODULEHANDLE_HPP

extern "C" IMAGE_DOS_HEADER __ImageBase;
namespace Windows {
    
    // GetCurrentModuleHandle
    //  - retrieves handle to the EXE or DLL in which this function resides
    //  - requires MinGW or MSVC to get __ImageBase symbol
    
    static inline HMODULE GetCurrentModuleHandle () {
        return reinterpret_cast <HMODULE> (&__ImageBase);
    };
};

#endif
