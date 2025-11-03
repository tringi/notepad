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

bool File::init (HANDLE h) noexcept {
    if (h != INVALID_HANDLE_VALUE) {
        this->handle = h;

        if (GetFileInformationByHandleEx (this->handle, FileIdInfo, &this->id, sizeof this->id)) {
            this->id_type = ExtendedFileIdType;

        } else {
            BY_HANDLE_FILE_INFORMATION info;
            if (GetFileInformationByHandle (this->handle, &info)) {

                this->id_type = FileIdType;
                this->id.VolumeSerialNumber = info.dwVolumeSerialNumber;
                std::memcpy (&this->id.FileId.Identifier [0], &info.nFileIndexLow, sizeof info.nFileIndexLow);
                std::memcpy (&this->id.FileId.Identifier [4], &info.nFileIndexHigh, sizeof info.nFileIndexHigh);
                std::memset (&this->id.FileId.Identifier [8], 0, sizeof this->id.FileId.Identifier - 8);
            } else {
                this->id_type = MaximumFileIdType;
            }
        }
        return true;
    } else
        return false;
}

bool File::init (const wchar_t * path) noexcept {

    // TODO: support passing file id like: <volume:id-bytes> for OpenFileById

    return this->init (CreateFile (path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
}

bool File::open (bool writable) noexcept {
    if (this->id_type == MaximumFileIdType)
        return false;

    FILE_ID_DESCRIPTOR descriptor {};
    descriptor.dwSize = sizeof descriptor;
    descriptor.Type = this->id_type;
    switch (this->id_type) {

        case FileIdType:
            std::memcpy (&descriptor.FileId.LowPart, &this->id.FileId.Identifier [0], sizeof descriptor.FileId.LowPart);
            std::memcpy (&descriptor.FileId.HighPart, &this->id.FileId.Identifier [4], sizeof descriptor.FileId.HighPart);
            break;

        case ExtendedFileIdType:
            descriptor.ExtendedFileId = this->id.FileId;
            break;
    }

    auto h = OpenFileById (this->handle, &descriptor,
                           GENERIC_READ | (writable ? GENERIC_WRITE : 0),
                           FILE_SHARE_READ, NULL, FILE_FLAG_SEQUENTIAL_SCAN);

    if (h != INVALID_HANDLE_VALUE) {

        // TODO: on commit, close mapping, append to the file, and reopen mapping

        LARGE_INTEGER size;
        if (GetFileSizeEx (h, &size)) {
            if (size.QuadPart) {

                // TODO: how to handle file that's too large?
                // TODO: opening from network doesn't work, copy content

                if (auto m = CreateFileMapping (h, NULL,
                                                writable ? PAGE_READWRITE : PAGE_READONLY,
                                                0, 0, NULL)) {
                    if (auto p = MapViewOfFile (m,
                                                writable ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ,
                                                0, 0, 0)) {
                        this->close ();

                        this->handle = h;
                        this->mapping = m;
                        this->data = p;
                        return true;
                    }

                    CloseHandle (m);
                }
            } else {
                // empty file
                this->close ();
                this->handle = h;
                return true;
            }
        }

        CloseHandle (h);
    }
    return false;
}

bool File::open (const wchar_t * path, bool writable) noexcept {
    if (this->init (CreateFile (path, GENERIC_READ | (writable ? GENERIC_WRITE : 0), FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL))) {

        // TODO: try mapping, then try copy

        return true;
    } else
        return false;
}

void File::close () {
    this->id_type = MaximumFileIdType;

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
        this->handle = INVALID_HANDLE_VALUE;
    }
}

bool File::IsOpen () const noexcept {
    return this->handle != INVALID_HANDLE_VALUE;
}

bool File::IsOpen (const FILE_ID_INFO * check) const noexcept {
    if (this->handle == INVALID_HANDLE_VALUE)
        return false;

    return std::memcmp (check, &this->id, sizeof this->id) == 0;
}

wchar_t * File::GetCurrentFileName (wchar_t * buffer, DWORD length) const {
    if (GetFinalPathNameByHandle (this->handle, buffer, length, 0)) {
        DWORD offset = 0;

        // remove prefixes
        if (std::wcsncmp (buffer, L"\\\\?\\UNC\\", 8) == 0) {
            buffer [6] = L'\\';
            offset = 6;
        } else
        if (std::wcsncmp (buffer, L"\\\\?\\", 4) == 0) {
            offset = 4;
        }

        return buffer + offset;
    } else
        return nullptr;
}
