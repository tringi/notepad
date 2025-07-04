#ifndef NOTEPAD_FILE_HPP
#define NOTEPAD_FILE_HPP

#include <Windows.h>

class File {
    HANDLE    handle = INVALID_HANDLE_VALUE;
    HANDLE    mapping = INVALID_HANDLE_VALUE;
    //wchar_t * filename = nullptr;
    void *    data = nullptr;

public:
    bool Open (const wchar_t *, DWORD disposition);
    void Close ();

    bool IsFileOpen () const noexcept;
    bool IsFileOpen (const FILE_ID_INFO *) const noexcept;

};

#endif
