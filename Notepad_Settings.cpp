#include "Notepad_Settings.hpp"
#include <cwchar>

namespace {
    HKEY hKeySettings = NULL;
    HKEY hKeyFiles = NULL;
    HKEY hKeyInstance = NULL;

    const wchar_t * IdMap (unsigned int id) {
        switch (id) {
            case 0x5A: return L"Classic Caption";
            case 0x5B: return L"Square Corners";
            case 0x5F: return L"Separate Processes";
            case 0x6E: return L"Line Numbers";
            case 0x6F: return L"Status Bar";
        }
        return nullptr;
    }
}

extern const wchar_t * szVersionInfo [8];

BOOL Settings::Init () {
    BOOL result = FALSE;

    if (!hKeySettings) {
        HKEY hKeySoftware = NULL;
        if (RegCreateKeyEx (HKEY_CURRENT_USER, L"SOFTWARE", 0, NULL, 0,
                            KEY_CREATE_SUB_KEY, NULL, &hKeySoftware, NULL) == ERROR_SUCCESS) {

            HKEY hKeyTRIMCORE = NULL;
            if (RegCreateKeyEx (hKeySoftware, szVersionInfo [2], 0, NULL, 0, // CompanyName - TRIM CORE SOFTWARE s.r.o.
                                KEY_ALL_ACCESS, NULL, &hKeyTRIMCORE, NULL) == ERROR_SUCCESS) {

                if (RegCreateKeyEx (hKeyTRIMCORE, szVersionInfo [0], 0, NULL, 0, // InternalName - Notepad
                                    KEY_ALL_ACCESS, NULL, &hKeySettings, NULL) == ERROR_SUCCESS) {

                    // Files
                    //  - value name = ID for OpenFileById
                    //  - value content = Last change time + temp file name for diff content

                    if (RegCreateKeyEx (hKeySettings, L"Files", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKeyFiles, NULL) == ERROR_SUCCESS) {
                        result = TRUE;
                    }

                    // Instance/PID
                    //  - volatile for reporting errors etc.

                    wchar_t szPID [12];
                    std::swprintf (szPID, 12, L"%u", GetCurrentProcessId ());
                    RegCreateKeyEx (hKeySettings, szPID, 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyInstance, NULL);
                }
                RegCloseKey (hKeyTRIMCORE);
            }
            RegCloseKey (hKeySoftware);
        }
    }
    return result;
}

DWORD Settings::Get (unsigned int id, DWORD default_) {
    return Get (IdMap (id), default_);
}
BOOL Settings::Set (unsigned int id, DWORD value) {
    return Set (IdMap (id), value);
}

DWORD Settings::Get (const wchar_t * item, DWORD default_) {
    if (item) {
        DWORD value = 0;
        DWORD size = sizeof (DWORD);
        if (RegQueryValueEx (hKeySettings, item, NULL, NULL, reinterpret_cast <BYTE *> (&value), &size) == ERROR_SUCCESS) {
            return value;
        }
    }
    return default_;
}

BOOL Settings::Set (const wchar_t * item, DWORD value) {
    if (item) {
        return RegSetValueEx (hKeySettings, item, 0, REG_DWORD, reinterpret_cast <const BYTE *> (&value), sizeof value)
            == ERROR_SUCCESS;
    } else
        return FALSE;
}

void Settings::ReportError (const wchar_t * format, ...) {
    wchar_t buffer [1536];
    std::swprintf (buffer, 1536, L"%08X: ", GetLastError ());

    va_list args;
    va_start (args, format);
    std::vswprintf (buffer + 10, 1526, format, args);
    va_end (args);

    RegSetValueEx (hKeyInstance, L"Last Error", 0, REG_SZ,
                   reinterpret_cast <const BYTE *> (buffer),
                   (DWORD) sizeof (wchar_t) * (std::wcslen (buffer) + 1));
}
