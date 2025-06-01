#include "Windows_SetThreadName.hpp"
#include "Windows_Symbol.hpp"
#include <cstdio>

namespace {
    HRESULT (WINAPI * ptrSetThreadDescription) (HANDLE _In_, PCWSTR _In_) = NULL;

    // TRIMCORE

    static constexpr DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)  
    typedef struct tagTHREADNAME_INFO {
        DWORD  dwType = 0x1000;
        LPCSTR szName;
        DWORD  dwThreadID;
        DWORD  dwFlags = 0;
    } THREADNAME_INFO;
#pragma pack(pop)

    template <std::size_t N>
    void ExtractASCIIfromUTF16 (char (&out) [N], const wchar_t * in) {
        auto p = &out [0];
        auto e = &out [N - 1];
        while ((*in != L'\0') && (p != e)) {
            if (unsigned (*in) < 128) {
                *p++ = (char) *in++;
            } else {
                ++in;
            }
        }
        *p = '\0';
    }

    void SetDebugThreadName (DWORD id, const wchar_t * name) {
        if (IsDebuggerPresent () && id && name) {

            char buffer [256];
            if (!WideCharToMultiByte (CP_ACP, 0, name, -1, buffer, sizeof buffer, NULL, NULL)) {
                ExtractASCIIfromUTF16 (buffer, name);
            }

            THREADNAME_INFO info;
            info.szName = buffer;
            info.dwThreadID = id;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
            __try {
                RaiseException (MS_VC_EXCEPTION, 0, sizeof (info) / sizeof (ULONG_PTR), (ULONG_PTR *) &info);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
            }
#pragma warning(pop)
        }
    }

    bool __cdecl CustomTrimcoreSetThreadNameImpl (HANDLE _In_, const wchar_t * _In_) {
        SetLastError (ERROR_CALL_NOT_IMPLEMENTED);
        return false;
    }

    HRESULT WINAPI UnavailableSetThreadDescription (HANDLE _In_ h, PCWSTR _In_ name) {
        return MAKE_HRESULT (SEVERITY_ERROR, FACILITY_WIN32, ERROR_CALL_NOT_IMPLEMENTED);
    }

    bool SetThreadNameImplementation (HANDLE h, DWORD id, const wchar_t * name) {
        SetDebugThreadName (id, name);

        if (ptrSetThreadDescription == NULL) {
            if (!Windows::Symbol (L"KERNELBASE", ptrSetThreadDescription, "SetThreadDescription")) {
                ptrSetThreadDescription = UnavailableSetThreadDescription;
            }
        }

        return SUCCEEDED (ptrSetThreadDescription (h, name));
    }
}

bool Windows::SetThreadName (HANDLE h, const wchar_t * name) {
    return SetThreadNameImplementation (h, GetThreadId (h), name);
}

bool Windows::SetThreadName (DWORD id, const wchar_t * name) {
    if (auto h = OpenThread (THREAD_SET_LIMITED_INFORMATION, FALSE, id)) {
        auto r = SetThreadNameImplementation (h, id, name);
        CloseHandle (h);
        return r;
    } else {
        SetDebugThreadName (id, name);
        return false;
    }
}

bool Windows::SetThreadName (const wchar_t * name) {
    return SetThreadNameImplementation (GetCurrentThread (), (DWORD) -1, name);
}
