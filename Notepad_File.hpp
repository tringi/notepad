#ifndef NOTEPAD_FILE_HPP
#define NOTEPAD_FILE_HPP

#include <Windows.h>
#include <algorithm>

class File {
public:
    HANDLE handle = INVALID_HANDLE_VALUE;
private:
    HANDLE mapping = NULL;
    void * data = nullptr;
    bool   allocated = false;

    FILE_ID_TYPE id_type = MaximumFileIdType;
public:
    FILE_ID_INFO id {};

public:
    File () = default;
    File (HANDLE h) noexcept {
        this->init (h);
    }
    ~File () {
        this->close ();
    }

    File (File && f) noexcept
        : handle (f.handle)
        , mapping (f.mapping)
        , data (f.data)
        , id_type (f.id_type)
        , id (f.id) {

        f.handle = nullptr;
        f.mapping = nullptr;
        f.data = nullptr;
        f.id_type = MaximumFileIdType;
    }

    File & operator = (File && other) noexcept {
        std::swap (this->handle, other.handle);
        std::swap (this->mapping, other.mapping);
        std::swap (this->data, other.data);
        std::swap (this->id_type, other.id_type);
        std::swap (this->id, other.id);
        return *this;
    }

    bool init (HANDLE h) noexcept;
    bool init (const wchar_t *) noexcept;
    bool open (bool writable) noexcept;
    bool open (const wchar_t *, bool writable) noexcept;

    void close ();

    bool IsOpen () const noexcept;
    bool IsOpen (const FILE_ID_INFO *) const noexcept;

    wchar_t * GetCurrentFileName (wchar_t * buffer, DWORD length) const;

private:
    bool load ();

    File (const File &) = delete;
    File & operator = (const File &) = delete;
};

#endif
