#include "Notepad_File.hpp"
#include <cstring>
#include <cwchar>

#include <set>
#include <vector>
#include <string>
#include <cstdint>

// LineTracker
//  - std::set <std::uint32_t> ??
//  - background initial scanning: low priority data scan while changes are already happening
//  - algs: 1) I'm at offset 123456, what is line number?
//          2) inserted/removed N bytes at 12345, update yourself

// diff: offset & length into source, replacement std::wstring?

// TODO: per-line, remember replacement lines

struct Editor {
    const void * source;
    std::size_t  length;
    UINT         charset;

    struct Difference {
        std::size_t  offset;
        std::size_t  length;
        std::wstring replacement;
    };

    std::vector <std::size_t>   lines;

    std::vector <Difference>    diffs;

    std::set <std::size_t, std::wstring>    insertions;
    std::set <std::size_t, std::size_t>     deletions;
};

bool File::Open (const wchar_t * path, DWORD disposition) {

    auto h = CreateFile (path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                         NULL, disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) {

        // TODO: on commit, close mapping, append to the file, and reopen mapping

        LARGE_INTEGER size;
        if (GetFileSizeEx (h, &size)) {
            if (size.QuadPart) {
                // TODO: how to handle file too large?
                if (auto m = CreateFileMapping (h, NULL, PAGE_READWRITE, 0, 0, NULL)) {
                    if (auto p = MapViewOfFile (m, FILE_MAP_ALL_ACCESS, 0, 0, 0)) {

                        this->Close ();
                        this->handle = h;
                        this->mapping = m;
                        this->data = p;
                        return true;
                    }
                    CloseHandle (m);
                }
            } else {
                this->Close ();
                this->handle = h;
                return true;
            }
        }
        CloseHandle (h);
    }

    std::wprintf (L"error %u opening: %s\n", GetLastError (), path);
    return false;
}

void File::Close () {
    if (this->data) {
        UnmapViewOfFile (this->data);
        this->data = nullptr;
    }
    if (this->mapping) {
        CloseHandle (this->mapping);
        this->mapping = NULL;
    }
    if (this->handle) {
        CloseHandle (this->handle);
        this->handle = NULL;
    }
}

bool File::IsFileOpen () const noexcept {
    return this->handle != INVALID_HANDLE_VALUE;
}

bool File::IsFileOpen (const FILE_ID_INFO * check) const noexcept {
    if (this->handle == INVALID_HANDLE_VALUE)
        return false;

    FILE_ID_INFO id {};
    if (GetFileInformationByHandleEx (this->handle, FileIdInfo, &id, sizeof id))
        return std::memcmp (check, &id, sizeof id) == 0;

    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileInformationByHandle (this->handle, &info)) {

        id.VolumeSerialNumber = info.dwVolumeSerialNumber;
        std::memcpy (&id.FileId.Identifier [0], &info.nFileIndexLow, sizeof info.nFileIndexLow);
        std::memcpy (&id.FileId.Identifier [4], &info.nFileIndexHigh, sizeof info.nFileIndexHigh);
        std::memset (&id.FileId.Identifier [8], 0, sizeof id.FileId.Identifier - 8);

        return std::memcmp (check, &id, sizeof id) == 0;
    } else
        return false;
}
