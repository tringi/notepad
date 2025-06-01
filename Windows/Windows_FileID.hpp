#ifndef WINDOWS_FILEID_HPP
#define WINDOWS_FILEID_HPP

#include <windows.h>

namespace Windows {

    // FileID
    //  - retrieves unique ID of particular file, independent of underlying filesystem, current OS, hardlink status, etc.
    //    and provides proper comparison useful to determine if two filesystem paths point to the same physical file
    //
    class FileID {

        // file
        //  - handle to the file remains open to prevent FILE ID from changing
        //
        HANDLE file = INVALID_HANDLE_VALUE;

    public:

        // info
        //  - retrieved or synthesized file information
        //
        FILE_ID_INFO info = {};

    public:
        FileID () noexcept;
        ~FileID () noexcept;

        // FileID
        //  - does not fail, result is not 'valid'
        //
        explicit FileID (const wchar_t * path) noexcept;

        FileID (const FileID &) noexcept;
        FileID & operator = (const FileID &) noexcept;

        FileID (FileID && from) noexcept;
        FileID & operator = (FileID && from) noexcept;

        // valid
        //  - returns true if the 'info' was retrieved and is stable (handle open)
        //
        [[nodiscard("'valid' is query operation")]]
        bool valid () const noexcept {
            return this->file != INVALID_HANDLE_VALUE;
        }

        // isValid
        //  - fast comparison against known invalid states of FILE_ID_128
        //
        inline static bool isValid (const FILE_ID_128 & id) {
            const auto u64 = reinterpret_cast <const LONGLONG *> (id.Identifier);
            return (u64 [0] != FILE_INVALID_FILE_ID)
                && (u64 [0] != 0 || u64 [1] != 0);
        }

        // operator ==
        //  - comparison for equality
        //  - uninitialized or failed-to-retrieve IDs always compare unequal
        //
        bool operator == (const FileID & other) const noexcept {
            return this->file != INVALID_HANDLE_VALUE
                && other.file != INVALID_HANDLE_VALUE
                && this->info.VolumeSerialNumber == other.info.VolumeSerialNumber
                && isValid (this->info.FileId)
                && isValid (other.info.FileId)
                && memcmp (&this->info.FileId, &other.info.FileId, sizeof (FILE_ID_128)) == 0
                ;
        }

        // operator <
        //  - 
        //
        bool operator < (const FileID & other) const noexcept {
            if (this->file < other.file)
                return true;

            if (this->file == other.file) {
                if (this->info.VolumeSerialNumber < other.info.VolumeSerialNumber)
                    return true;

                if (this->info.VolumeSerialNumber == other.info.VolumeSerialNumber) {
                    return memcmp (&this->info.FileId, &other.info.FileId, sizeof (FILE_ID_128)) < 0;
                }
            }

            return false;
        }
    };
}

#endif
