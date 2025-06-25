#include "Windows_GetFullPath.hpp"
#include <cwchar>

std::size_t Windows::GetFullPath (const wchar_t * lpFileName, std::size_t nBufferLength, wchar_t * lpBuffer, wchar_t ** lpFilePart) {

    // valid path cannot be longer
    //  - this check also prevents potential overflows later

    if (nBufferLength > 32768u) {
        nBufferLength = 32768u;
    }

    // check if path has \\?\ prefix and is thus already a full path
    //  - then just copy it to output and easily find last component

    if (   lpFileName [0] == L'\\'
        && lpFileName [1] == L'\\'
        && lpFileName [2] == L'?'
        && lpFileName [3] == L'\\') {

        auto length = std::wcslen (lpFileName);
        if (length < nBufferLength) {
            std::wcscpy (lpBuffer, lpFileName);

            if (lpFilePart) {
                if (const wchar_t * p = std::wcsrchr (lpFileName, L'\\'))
                    *lpFilePart = lpBuffer + (p - lpFileName) + 1;
                else
                    *lpFilePart = NULL;
            }

            return length;
        } else {
            SetLastError (ERROR_BUFFER_OVERFLOW);
            return 0;
        }
    }

    const bool unc = lpFileName [0] == L'\\'
                  && lpFileName [1] == L'\\';

    // sanity check for output buffer size
    //  - \\?\UNC\a\b\c - 14 characters with NUL terminator
    //  - \\?\C:\a - 9 characters with NUL terminator

    if (   (unc == true && nBufferLength >= 14u)
        || (unc == false && nBufferLength >= 9u)) {

        // prefix length
        //  - UNC use 6 (not 7) because first '\\' is replaced by 'C'

        const std::size_t prefix = unc ? 6 : 4;
        const DWORD r = GetFullPathName (lpFileName,
                                         (DWORD) (nBufferLength - prefix), lpBuffer + prefix,
                                         lpFilePart);

        if (r) {
            if (r < nBufferLength - prefix) {

                lpBuffer [0] = L'\\';
                lpBuffer [1] = L'\\';
                lpBuffer [2] = L'?';
                lpBuffer [3] = L'\\';

                if (unc) {
                    lpBuffer [4] = L'U';
                    lpBuffer [5] = L'N';
                    lpBuffer [6] = L'C';
                }

                return r + prefix;
            } else {
                SetLastError (ERROR_BUFFER_OVERFLOW);
            }
        }
    } else {
        SetLastError (ERROR_BUFFER_OVERFLOW);
    }
    return 0;
}
