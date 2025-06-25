#ifndef WINDOWS_GETFULLPATH_HPP
#define WINDOWS_GETFULLPATH_HPP

#include <windows.h>
#include <cstddef>

namespace Windows {
    
    // GetFullPath
    //  - calls GetFullPathName and prepends proper prefix
    //  - parameters: filename - input path
    //                length   - output buffer length (in TCHARs)
    //                buffer   - output buffer pointes
    //                basename - if not NULL, receives pointer to last component
    //                           inside the output buffer
    //  - returns: number of characters written on success,
    //             0 on failure, GetLastError:
    //              - ERROR_BUFFER_OVERFLOW if the buffer was too small
    //              - other code set by GetFullPathName
    
    std::size_t GetFullPath (const wchar_t * filename, std::size_t length, wchar_t * buffer, wchar_t ** basename = nullptr);
    
};

#endif
