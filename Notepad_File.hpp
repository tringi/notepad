#ifndef NOTEPAD_FILE_HPP
#define NOTEPAD_FILE_HPP

#include <Windows.h>

class File {
public:
    HANDLE handle = INVALID_HANDLE_VALUE;
private:
    HANDLE mapping = INVALID_HANDLE_VALUE;
    void * data = nullptr;

    FILE_ID_TYPE id_type = MaximumFileIdType;
public:
    FILE_ID_INFO id {};

public:
    File () = default;
    File (HANDLE h) noexcept {
        this->init (h);
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

    bool init (const wchar_t *);
    bool init (HANDLE h);

protected:
    bool open (bool writable);
    void close ();

    bool IsOpen () const noexcept;
    bool IsOpen (const FILE_ID_INFO *) const noexcept;

private:
    File (const File &) = delete;
    File & operator = (const File &) = delete;
};

#endif
