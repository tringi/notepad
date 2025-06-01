#include "Windows_FileID.hpp"
#include "Windows_Symbol.hpp"
#include <cstring>

Windows::FileID::FileID () noexcept {}

Windows::FileID::~FileID () noexcept {
    if (this->file != INVALID_HANDLE_VALUE) {
        CloseHandle (this->file);
    }
}

// initialization

Windows::FileID::FileID (const wchar_t * path) noexcept
    : file (CreateFile (path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL)) {

    if (this->file != INVALID_HANDLE_VALUE) {

        if (GetFileInformationByHandleEx (this->file, FileIdInfo, &this->info, sizeof this->info))
            return;

        BY_HANDLE_FILE_INFORMATION info;
        if (GetFileInformationByHandle (this->file, &info)) {

            this->info.VolumeSerialNumber = info.dwVolumeSerialNumber;

            reinterpret_cast <DWORD *> (this->info.FileId.Identifier) [0] = info.nFileIndexLow;
            reinterpret_cast <DWORD *> (this->info.FileId.Identifier) [1] = info.nFileIndexHigh;
        } else {
            CloseHandle (this->file);
            this->file = INVALID_HANDLE_VALUE;
        }
    }
}

// copy

Windows::FileID::FileID (const FileID & from) noexcept
    : info (from.info) {

    if (!DuplicateHandle ((HANDLE) -1, from.file, (HANDLE) -1, &this->file, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        this->file = INVALID_HANDLE_VALUE;
    }
}

Windows::FileID & Windows::FileID::operator = (const FileID & from) noexcept {
    if (this->file != INVALID_HANDLE_VALUE) {
        CloseHandle (this->file);
    }
    if (!DuplicateHandle ((HANDLE) -1, from.file, (HANDLE) -1, &this->file, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        this->file = INVALID_HANDLE_VALUE;
    }
    this->info = from.info;
    return *this;
}

// move

Windows::FileID::FileID (FileID && from) noexcept
    : file (from.file)
    , info (from.info) {

    from.file = INVALID_HANDLE_VALUE;
}

Windows::FileID & Windows::FileID::operator = (FileID && from) noexcept {
    if (this->file != INVALID_HANDLE_VALUE) {
        CloseHandle (this->file);
    }
    this->file = from.file;
    this->info = from.info;
    from.file = INVALID_HANDLE_VALUE;
    return *this;
}
